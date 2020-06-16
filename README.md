<h1 align="center">NickelMenu</h1>

The easiest way to launch custom scripts, change hidden settings, and run actions on Kobo eReaders.

See the [thread on MobileRead](https://go.pgaskin.net/kobo/nm) for more details.

## Installation
You can download pre-built packages of the latest stable release from the [releases](https://github.com/geek1011/NickelMenu/releases) page, or you can find bleeding-edge builds of each commit from [here](https://github.com/geek1011/NickelMenu/actions).

After you download the package, copy `KoboRoot.tgz` into the `.kobo` folder of your eReader, then eject it.

After it installs, you will find a new menu item named `NickelMenu` with further instructions which you can also read [here](./res/doc).

To uninstall NickelMenu, just create a new file named `uninstall` in `.adds/nm/`, or trigger the failsafe mechanism by immediately powering off the Kobo after it starts booting.

Most errors, if any, will be displayed as a menu item in the main menu in the top-left corner of the home screen. If no new menu entries appear here after a reboot, try reinstalling NickelMenu. If that still doesn't work, connect over telnet or SSH and check the output of `logread`.

## Compiling
[@NiLuJe](https://github.com/NiLuJe)'s [toolchain](https://github.com/koreader/koxtoolchain) and [sysroot](https://svn.ak-team.com/svn/Configs/trunk/Kindle/Misc/kobo-nickel-sysroot.sh) are required to compile NickelMenu. They can also be found as a [Docker image](https://hub.docker.com/r/geek1011/kobo-toolchain). To compile NickelMenu, just run `make all koboroot` with the toolchain in your PATH, or use `docker run --volume="$PWD:$PWD" --user="$(id --user):$(id --group)" --workdir="$PWD" --env=HOME --entrypoint=make --rm -it geek1011/kobo-toolchain all koboroot` with the Docker image.

<!-- TODO: a lot more stuff -->
