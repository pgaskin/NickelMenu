#ifndef NM_KFMON_H
#define NM_KFMON_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "action.h"

// Path to KFMon's IPC Unix socket
#define KFMON_IPC_SOCKET "/tmp/kfmon-ipc.ctl"

// Flags for the failure bingo
typedef enum {
    // Not an error ;p
    KFMON_IPC_OK = EXIT_SUCCESS,
    // NOTE: Start > 256 to stay clear of errno
    KFMON_IPC_ETIMEDOUT = 512,
    KFMON_IPC_EPIPE,
    KFMON_IPC_ENODATA,
    // syscall failures
    KFMON_IPC_READ_FAILURE,
    KFMON_IPC_SEND_FAILURE,
    KFMON_IPC_SOCKET_FAILURE,
    KFMON_IPC_CONNECT_FAILURE,
    KFMON_IPC_POLL_FAILURE,
    KFMON_IPC_CALLOC_FAILURE,
    KFMON_IPC_REPLY_READ_FAILURE,
    KFMON_IPC_LIST_PARSE_FAILURE,
    // Those match the actual string sent over the wire
    KFMON_IPC_ERR_INVALID_ID,
    KFMON_IPC_ERR_INVALID_NAME,
    KFMON_IPC_WARN_ALREADY_RUNNING,
    KFMON_IPC_WARN_SPAWN_BLOCKED,
    KFMON_IPC_WARN_SPAWN_INHIBITED,
    KFMON_IPC_ERR_REALLY_MALFORMED_CMD,
    KFMON_IPC_ERR_MALFORMED_CMD,
    KFMON_IPC_ERR_INVALID_CMD,
    KFMON_IPC_UNKNOWN_REPLY,
    // Not an error either, means we have more to read...
    KFMON_IPC_EAGAIN,
} kfmon_ipc_errno_e;

// A single watch item
typedef struct {
    uint8_t idx;
    char *filename;
    char *label;
} kfmon_watch_t;

// A node in a linked list of watches
typedef struct kfmon_watch_node {
    kfmon_watch_t watch;
    struct kfmon_watch_node *next;
} kfmon_watch_node_t;

// A control structure to keep track of a list of watches
typedef struct {
    size_t count;
    kfmon_watch_node_t *head;
    kfmon_watch_node_t *tail;
} kfmon_watch_list_t;

// Used as the reply handler in our polling loops.
// Second argument is an opaque pointer used for storage in a linked list
// (e.g., a pointer to a kfmon_watch_list_t, or NULL if no storage is needed).
typedef int (*ipc_handler_t)(int, void *);

// Free all resources allocated by a list and its nodes
void kfmon_teardown_list(kfmon_watch_list_t *list);
// Allocate a single new node to the list
int kfmon_grow_list(kfmon_watch_list_t *list);

// If status is success, false is returned. Otherwise, true is returned and
// nm_err is set.
bool nm_kfmon_error_handler(kfmon_ipc_errno_e status);

// Given one of the error codes listed above, return properly from an action.
nm_action_result_t *nm_kfmon_return_handler(kfmon_ipc_errno_e status);

// Send a simple KFMon IPC request, one where the reply is only used for its diagnostic value.
int nm_kfmon_simple_request(const char *restrict ipc_cmd, const char *restrict ipc_arg);

// Handle a list request for the KFMon generator
int nm_kfmon_list_request(const char *restrict ipc_cmd, kfmon_watch_list_t *list);

#ifdef __cplusplus
}
#endif
#endif
