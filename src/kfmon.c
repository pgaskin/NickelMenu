#define _GNU_SOURCE
#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/un.h>
#include <unistd.h>
#include <linux/limits.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include "util.h"
#include "kfmon_helpers.h"
#include "kfmon.h"

// Free all resources allocated by a list and its nodes
static void teardown_list(kfmon_watch_list_t* list) {
    kfmon_watch_node_t* node = list->head;
    while (node) {
        NM_LOG("Freeing node at %p", node);
        kfmon_watch_node_t* p = node->next;
        free(node->watch.filename);
        free(node->watch.label);
        free(node);
        node = p;
    }
    // Don't leave dangling pointers
    list->head = NULL;
    list->tail = NULL;
}

// Allocate a single new node to the list
static int grow_list(kfmon_watch_list_t* list) {
    kfmon_watch_node_t* prev = list->tail;
    kfmon_watch_node_t* node = calloc(1, sizeof(*node));
    if (!node) {
        return KFMON_IPC_CALLOC_FAILURE;
    }
    list->count++;

    // Update the head if this is the first node
    if (!list->head) {
        list->head = node;
    }
    // Update the tail pointer
    list->tail = node;
    // If there was a previous node, link the two together
    if (prev) {
        prev->next = node;
    }

    return EXIT_SUCCESS;
}

// Handle replies from the IPC socket
static int handle_reply(int data_fd, void *data __attribute__((unused))) {
    // Eh, recycle PIPE_BUF, it should be more than enough for our needs.
    char buf[PIPE_BUF] = { 0 };

    // We don't actually know the size of the reply, so, best effort here.
    ssize_t len = xread(data_fd, buf, sizeof(buf));
    if (len < 0) {
        // Only actual failures are left, xread handles the rest
        return KFMON_IPC_REPLY_READ_FAILURE;
    }

    // If there's actually nothing to read (EoF), abort.
    if (len == 0) {
        return KFMON_IPC_ENODATA;
    }

    // Check the reply for failures
    if (strncmp(buf, "ERR_INVALID_ID", 14) == 0) {
        return KFMON_IPC_ERR_INVALID_ID;
    } else if (strncmp(buf, "WARN_ALREADY_RUNNING", 20) == 0) {
        return KFMON_IPC_WARN_ALREADY_RUNNING;
    } else if (strncmp(buf, "WARN_SPAWN_BLOCKED", 18) == 0) {
        return KFMON_IPC_WARN_SPAWN_BLOCKED;
    } else if (strncmp(buf, "WARN_SPAWN_INHIBITED", 20) == 0) {
        return KFMON_IPC_WARN_SPAWN_INHIBITED;
    } else if (strncmp(buf, "ERR_REALLY_MALFORMED_CMD", 24) == 0) {
        return KFMON_IPC_ERR_REALLY_MALFORMED_CMD;
    } else if (strncmp(buf, "ERR_MALFORMED_CMD", 17) == 0) {
        return KFMON_IPC_ERR_MALFORMED_CMD;
    } else if (strncmp(buf, "ERR_INVALID_CMD", 15) == 0) {
        return KFMON_IPC_ERR_INVALID_CMD;
    } else if (strncmp(buf, "OK", 2) == 0) {
        return EXIT_SUCCESS;
    } else {
        return KFMON_IPC_UNKNOWN_REPLY;
    }

    // We're not done until we've got a reply we're satisfied with...
    return KFMON_IPC_EAGAIN;
}

// Handle replies from a 'list' command
static int handle_list_reply(int data_fd, void *data) {
    // Can't do it on the stack because of strsep
    char *buf = NULL;
    buf = calloc(PIPE_BUF, sizeof(*buf));
    if (!buf) {
        return KFMON_IPC_CALLOC_FAILURE;
    }

    // Until proven otherwise...
    int status = EXIT_SUCCESS;

    // We don't actually know the size of the reply, so, best effort here.
    ssize_t len = xread(data_fd, buf, PIPE_BUF);
    if (len < 0) {
        // Only actual failures are left, xread handles the rest
        status = KFMON_IPC_REPLY_READ_FAILURE;
        goto cleanup;
    }

    // If there's actually nothing to read (EoF), abort.
    if (len == 0) {
        status = KFMON_IPC_ENODATA;
        goto cleanup;
    }

    // The only valid reply for list is... a list ;).
    if (strncmp(buf, "ERR_INVALID_CMD", 15) == 0) {
        status = KFMON_IPC_ERR_INVALID_CMD;
        goto cleanup;
    } else if ((strncmp(buf, "WARN_", 5) == 0) ||
               (strncmp(buf, "ERR_", 4) == 0) ||
               (strncmp(buf, "OK", 2) == 0)) {
        status = KFMON_IPC_UNKNOWN_REPLY;
        goto cleanup;
    }

    // NOTE: Replies may be split across multiple reads (and as such, multiple handle_list_reply calls).
    //       So the only way we can be sure that we're done (short of timing out after a while of no POLLIN revents,
    //       which would be stupid), is to check that the final byte we've just read is a NUL,
    //       as that's how KFMon terminates a list.
    bool eot = false;
    // NOTE: The parsing code later does its own take on this to detect the final line,
    //       but this one should be authoritative, as strsep modifies the data.
    if (buf[len - 1] == '\0') {
        eot = true;
    }

    // NOTE to self: syslog strips LF, you dummy.
    NM_LOG("<<< Got a %zd bytes reply:\n%.*s", len, len, buf);
    // Now that we're sure we didn't get a wonky reply from an unrelated command, parse the list
    // NOTE: Format is:
    //       id:filename:label or id:filename for watches without a label
    //       We don't care about id, as it potentially won't be stable across the full powercycle,
    //       filename is what we pass verbatim to a kfmon action
    //       label is our action's lbl (use filename if NULL)
    char *p = buf;
    char *line = NULL;
    // Break the reply line by line
    while ((line = strsep(&p, "\n")) != NULL) {
        // Then parse each line
        NM_LOG("Parsing line: %s\n", line);
        // If it's the final line, its only content is a single NUL
        if (*line == '\0') {
            // NOTE: This might also simply be the end of a single-line read,
            //       in which case the NUL is thanks to calloc...
            NM_LOG("Caught an empty line! EoT? %d", eot);
            break;
        }
        // NOTE: Simple syslog logging for now
        char *watch_idx = strsep(&line, ":");
        if (!watch_idx) {
            status = KFMON_IPC_LIST_PARSE_FAILURE;
            goto cleanup;
        }
        NM_LOG("watch index: %s", watch_idx);
        char *filename = strsep(&line, ":");
        if (!filename) {
            status = KFMON_IPC_LIST_PARSE_FAILURE;
            goto cleanup;
        }
        NM_LOG("filename: '%s'", filename);
        // Final separator is optional, if it's not there, there's no label, use the filename instead.
        char *label = strsep(&line, ":");
        if (label) {
            NM_LOG("label: '%s'", label);
        } else {
            NM_LOG("label -> filename");
        }

        // Store that at the tail of the list
        kfmon_watch_list_t* list = (kfmon_watch_list_t*) data;
        // Make room for a new node
        if (grow_list(list) != EXIT_SUCCESS) {
            status = KFMON_IPC_CALLOC_FAILURE;
            break;
        }
        // Use it
        kfmon_watch_node_t* node = list->tail;
        NM_LOG("New node at %p", node);

        node->watch.idx = (uint8_t) strtoul(watch_idx, NULL, 10);
        node->watch.filename = strdup(filename);
        node->watch.label = label ? strdup(label) : strdup(filename);
    }

    // Are we really done?
    status = eot ? EXIT_SUCCESS : KFMON_IPC_EAGAIN;

cleanup:
    free(buf);
    return status;
}

// Connect to KFMon's IPC socket. Returns error code, store data fd by ref.
static int connect_to_kfmon_socket(int *restrict data_fd) {
    // Setup the local socket
    if ((*data_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0)) == -1) {
        return KFMON_IPC_SOCKET_FAILURE;
    }

    struct sockaddr_un sock_name = { 0 };
    sock_name.sun_family         = AF_UNIX;
    strncpy(sock_name.sun_path, KFMON_IPC_SOCKET, sizeof(sock_name.sun_path) - 1);

    // Connect to IPC socket, retrying safely on EINTR (c.f., http://www.madore.org/~david/computers/connect-intr.html)
    while (connect(*data_fd, (const struct sockaddr*) &sock_name, sizeof(sock_name)) == -1 && errno != EISCONN) {
        if (errno != EINTR) {
            return KFMON_IPC_CONNECT_FAILURE;
        }
    }

    // Wheee!
    return EXIT_SUCCESS;
}

// Send a packet to KFMon over the wire (payload *MUST* be NUL-terminated to avoid truncation, and len *MUST* include that NUL).
static int send_packet(int data_fd, const char *restrict payload, size_t len) {
    // Send it (w/ a NUL)
    if (send_in_full(data_fd, payload, len) < 0) {
        // Only actual failures are left
        if (errno == EPIPE) {
            return KFMON_IPC_EPIPE;
        } else {
            return KFMON_IPC_SEND_FAILURE;
        }
    }

    // Wheee!
    return EXIT_SUCCESS;
}

// Send the requested IPC command:arg pair (or command alone if arg is NULL)
static int send_ipc_command(int data_fd, const char *restrict ipc_cmd, const char *restrict ipc_arg) {
    char buf[256] = { 0 };
    int packet_len = 0;
    // Somme commands don't require an arg
    if (ipc_arg) {
        packet_len = snprintf(buf, sizeof(buf), "%s:%s", ipc_cmd, ipc_arg);
    } else {
        packet_len = snprintf(buf, sizeof(buf), "%s", ipc_cmd);
    }
    // Send it (w/ a NUL)
    return send_packet(data_fd, buf, (size_t) (packet_len + 1));
}

// Poll the IPC socket for potentially *multiple* replies to a single command, timeout after attempts * timeout (ms)
static int wait_for_replies(int data_fd, int timeout, size_t attempts, ipc_handler_t reply_handler, void **data) {
    int status = EXIT_SUCCESS;

    struct pollfd pfd = { 0 };
    // Data socket
    pfd.fd     = data_fd;
    pfd.events = POLLIN;

    // Here goes... We'll wait for <attempts> windows of <timeout>ms
    size_t retry = 0U;
    while (1) {
        int poll_num = poll(&pfd, 1, timeout);
        if (poll_num == -1) {
            if (errno == EINTR) {
                continue;
            }
            return KFMON_IPC_POLL_FAILURE;
        }

        if (poll_num > 0) {
            if (pfd.revents & POLLIN) {
                // There was a reply from the socket
                int reply = reply_handler(data_fd, data);
                if (reply != EXIT_SUCCESS) {
                    // If the remote closed the connection, we get POLLIN|POLLHUP w/ EoF ;).
                    if (pfd.revents & POLLHUP) {
                        // Flag that as an error
                        status = KFMON_IPC_EPIPE;
                    } else {
                        if (reply == KFMON_IPC_EAGAIN) {
                            // We're expecting more stuff to read, keep going.
                            continue;
                        } else {
                            // Something went wrong when handling the reply, pass the error as-is
                            status = reply;
                        }
                    }
                    // We're obviously done if something went wrong.
                    break;
                } else {
                    // We break on success, too, as we only need to send a single command.
                    status = EXIT_SUCCESS;
                    break;
                }
            }

            // Remote closed the connection
            if (pfd.revents & POLLHUP) {
                // Flag that as an error
                status = KFMON_IPC_EPIPE;
                break;
            }
        }

        if (poll_num == 0) {
            // Timed out, increase the retry counter
            retry++;
        }

        // Drop the axe after the final attempt
        if (retry >= attempts) {
            status = KFMON_IPC_ETIMEDOUT;
            break;
        }
    }

    return status;
}

// Handle a simple KFMon IPC request
int nm_kfmon_simple_request(const char *restrict ipc_cmd, const char *restrict ipc_arg) {
    // Assume everything's peachy until shit happens...
    int status = EXIT_SUCCESS;

    int data_fd = -1;
    // Attempt to connect to KFMon...
    // As long as KFMon is up, has very little chance to fail, even if the connection backlog is full.
    status = connect_to_kfmon_socket(&data_fd);
    // If it failed, return early
    if (status != EXIT_SUCCESS) {
        return status;
    }

    // Attempt to send the specified command in full over the wire
    status = send_ipc_command(data_fd, ipc_cmd, ipc_arg);
    // If it failed, return early, after closing the socket
    if (status != EXIT_SUCCESS) {
        close(data_fd);
        return status;
    }

    // We'll be polling the socket for a reply, this'll make things neater, and allows us to abort on timeout,
    // in the unlikely event there's already an IPC session being handled by KFMon,
    // in which case the reply would be delayed by an undeterminate amount of time (i.e., until KFMon gets to it).
    // Here, we'll want to timeout after 2s
    ipc_handler_t handler = &handle_reply;
    status = wait_for_replies(data_fd, 500, 4, handler, NULL);
    // NOTE: We happen to be done with the connection right now.
    //       But if we still needed it, KFMON_IPC_POLL_FAILURE would warrant an early abort w/ a forced close().

    // Bye now!
    close(data_fd);
    return status;
}

// PoC handling of a list request
int nm_kfmon_list_request(const char *restrict foo __attribute__((unused))) {
    // Assume everything's peachy until shit happens...
    int status = EXIT_SUCCESS;

    int data_fd = -1;
    // Attempt to connect to KFMon...
    // As long as KFMon is up, has very little chance to fail, even if the connection backlog is full.
    status = connect_to_kfmon_socket(&data_fd);
    // If it failed, return early
    if (status != EXIT_SUCCESS) {
        return status;
    }

    // Attempt to send the specified command in full over the wire
    status = send_ipc_command(data_fd, "list", NULL);   // FIXME: switch to gui-list
    // If it failed, return early, after closing the socket
    if (status != EXIT_SUCCESS) {
        close(data_fd);
        return status;
    }

    // We'll want to retrieve our watch list in there.
    kfmon_watch_list_t list = { 0 };

    // We'll be polling the socket for a reply, this'll make things neater, and allows us to abort on timeout,
    // in the unlikely event there's already an IPC session being handled by KFMon,
    // in which case the reply would be delayed by an undeterminate amount of time (i.e., until KFMon gets to it).
    // Here, we'll want to timeout after 2s
    ipc_handler_t handler = &handle_list_reply;
    status = wait_for_replies(data_fd, 500, 4, handler, (void *) &list);
    // NOTE: We happen to be done with the connection right now.
    //       But if we still needed it, KFMON_IPC_POLL_FAILURE would warrant an early abort w/ a forced close().

    // Walk the list
    NM_LOG("List has %zu nodes", list.count);
    NM_LOG("Head is at %p", list.head);
    NM_LOG("Tail is at %p", list.tail);
    kfmon_watch_node_t* node = list.head;
    while (node) {
        NM_LOG("Dumping node at %p", node);
        NM_LOG("idx: %hhu // filename: %s // label: %s", node->watch.idx, node->watch.filename, node->watch.label);
        node = node->next;
    }

    // Destroy it
    teardown_list(&list);

    // Bye now!
    close(data_fd);
    return status;
}

// Giant ladder of fail
nm_action_result_t* nm_kfmon_return_handler(int status, char **err_out) {
    #define NM_ERR_RET NULL

    if (status != EXIT_SUCCESS) {
        // Fail w/ the right log message
        if (status == KFMON_IPC_ETIMEDOUT) {
            NM_RETURN_ERR("Timed out waiting for KFMon");
        } else if (status == KFMON_IPC_EPIPE) {
            NM_RETURN_ERR("KFMon closed the connection");
        } else if (status == KFMON_IPC_ENODATA) {
            NM_RETURN_ERR("No more data to read");
        } else if (status == KFMON_IPC_READ_FAILURE) {
            // NOTE: Let's hope close() won't mangle errno...
            NM_RETURN_ERR("read: %m");
        } else if (status == KFMON_IPC_SEND_FAILURE) {
            // NOTE: Let's hope close() won't mangle errno...
            NM_RETURN_ERR("send: %m");
        } else if (status == KFMON_IPC_SOCKET_FAILURE) {
            NM_RETURN_ERR("Failed to create local KFMon IPC socket (socket: %m)");
        } else if (status == KFMON_IPC_CONNECT_FAILURE) {
            NM_RETURN_ERR("KFMon IPC is down (connect: %m)");
        } else if (status == KFMON_IPC_POLL_FAILURE) {
            // NOTE: Let's hope close() won't mangle errno...
            NM_RETURN_ERR("poll: %m");
        } else if (status == KFMON_IPC_CALLOC_FAILURE) {
            NM_RETURN_ERR("calloc: %m");
        } else if (status == KFMON_IPC_REPLY_READ_FAILURE) {
            // NOTE: Let's hope close() won't mangle errno...
            NM_RETURN_ERR("Failed to read KFMon's reply (%m)");
        } else if (status == KFMON_IPC_LIST_PARSE_FAILURE) {
            NM_RETURN_ERR("Failed to parse the list of watches (no separator found)");
        } else if (status == KFMON_IPC_ERR_INVALID_ID) {
            NM_RETURN_ERR("Requested to start an invalid watch index");
        } else if (status == KFMON_IPC_ERR_INVALID_NAME) {
            NM_RETURN_ERR("Requested to trigger an invalid watch filename (this is the image filename with, not the config) (maybe you forget the extension?)");
        } else if (status == KFMON_IPC_WARN_ALREADY_RUNNING) {
            NM_RETURN_ERR("Requested watch is already running");
        } else if (status == KFMON_IPC_WARN_SPAWN_BLOCKED) {
            NM_RETURN_ERR("A spawn blocker is currently running");
        } else if (status == KFMON_IPC_WARN_SPAWN_INHIBITED) {
            NM_RETURN_ERR("Spawns are currently inhibited");
        } else if (status == KFMON_IPC_ERR_REALLY_MALFORMED_CMD) {
            NM_RETURN_ERR("KFMon couldn't parse our command");
        } else if (status == KFMON_IPC_ERR_MALFORMED_CMD) {
            NM_RETURN_ERR("Bad command syntax");
        } else if (status == KFMON_IPC_ERR_INVALID_CMD) {
            NM_RETURN_ERR("Command wasn't recognized by KFMon");
        } else if (status == KFMON_IPC_UNKNOWN_REPLY) {
            NM_RETURN_ERR("We couldn't make sense of KFMon's reply");
        } else {
            // Should never happen
            NM_RETURN_ERR("Something went wrong");
        }
    } else {
        NM_RETURN_OK(nm_action_result_silent());
    }

    #undef NM_ERR_RET
}
