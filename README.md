<h1 align="center">NickelMenu</h1>

The easiest way to launch custom scripts, change hidden settings, and run actions on Kobo eReaders.

See the [website](https://pgaskin.net/NickelMenu) and [thread on MobileRead](https://mobileread.com/forums/showthread.php?t=329525) for screenshots and more details.

## Installation
You can download pre-built packages of the latest stable release from the [releases](https://github.com/pgaskin/NickelMenu/releases) page, or you can find bleeding-edge builds of each commit from [here](https://github.com/pgaskin/NickelMenu/actions).

After you download the package, copy `KoboRoot.tgz` into the `.kobo` folder of your eReader, then eject it.

After it installs, you will find a new menu item named `NickelMenu` with further instructions which you can also read [here](./res/doc).

To uninstall NickelMenu, just create a new file named `uninstall` in `.adds/nm/`, or trigger the failsafe mechanism by immediately powering off the Kobo after it starts booting.

Most errors, if any, will be displayed as a menu item in the main menu. If no new menu entries appear here after a reboot, try reinstalling NickelMenu. If that still doesn't work, connect over telnet or SSH and check the output of `logread`.

## Compiling

NickelMenu is designed to be compiled with [NickelTC](https://github.com/pgaskin/NickelTC). To compile it with Docker/Podman, use `docker run --volume="$PWD:$PWD" --user="$(id --user):$(id --group)" --workdir="$PWD" --env=HOME --entrypoint=make --rm -it ghcr.io/pgaskin/nickeltc:1.0 all koboroot`. To compile it on the host, use `make CROSS_COMPILE=/path/to/nickeltc/bin/arm-nickel-linux-gnueabihf-`.

<!-- TODO: a lot more stuff -->
