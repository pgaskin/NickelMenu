<h1 align="center">NickelHook</h1>

A robust library for creating Nickel mods.

NickelHook started out as part of [NickelMenu](https://github.com/pgaskin/NickelMenu), but was later extracted into this library for easier and safer re-use in other mods.

**Features:**
- Generates a Qt plugin designed for use with Nickel.
- Project generator.
- C library:
  - Failsafe mechanism.
  - Uninstall when a path exists.
  - Uninstall when a path is deleted.
  - PLT hooks.
  - Optional dlsym helper.
  - Error checking.
  - Custom init function.
  - Logging.
  - Extensible.
- Makefile library:
  - Designed for use with [NickelTC](https://github.com/pgaskin/NickelTC).
  - pkg-config integration.
  - Supports C, C++, Qt MOC.
  - compile_flags.json generation for clangd or other tools.
  - KoboRoot.tgz generation with custom files.
  - Can also install to a DESTDIR instead.
  - Supports custom CPPFLAGS/CFLAGS/CXXFLAGS/LDFLAGS.
  - gitignore generation.
  - Git versioning (but can be overridden).
  - Supports custom targets.

## Installation

1. Copy this folder to your project, or include it as a git submodule.
2. Run `make -f ./relative/path/to/NickelHook.mk NAME=MyModName`.
3. Build using `make`. To generate a KoboRoot.tgz, run `make koboroot` after building. To clean, run `make clean`.
4. Whenever you change header files, run `make clean`. If you add/remove source files, run `make gitignore`.
5. To generate a compile_commands.json, use `make clangd CROSS_COMPILE= CC=clang-10 CXX=clang++-10 CFLAGS= CXXFLAGS=`.

## Usage

Although NickelHook is safer and easier than using `LD_PRELOAD` directly or replacing existing init scripts and libraries, it still requires care to use.

NickelHook should only be used for mods which need to interact directly with Nickel, and even then, the advantages/disadvantages should be considered carefully. Mods should also tend to be on the more minimal side. For example, you should not try and build another full-featured application on top of Nickel with it, or use it to make a scripting engine for Nickel (although you could use it to provide an interface to Nickel for an external app, but be careful not to overuse it). Remember, any bugs in your mod will fail silently at best, or cause subtle corruption and crashes in libnickel at worst if you aren't careful.

NickelHook primarily deals with building, loading, and initializing custom mods. Most runtime support functionality (other than logging) is out of scope. NickelHook is developed on a rolling basis and there will not be any formal releases, so include it as a copy (or submodule) in your project and update it as needed.

For example, the dlsym helper is meant to be used for symbols required for the core functionality of the mod only. For an example how [NickelMenu](https://github.com/pgaskin/NickelMenu) does this with menu items and actions. Also have a look at [NickelSeries](https://github.com/pgaskin/kobo-mods/tree/master/NickelSeries) for something smaller. A good guideline is that if you end up trying to figure out how to access your dlsym'd symbols from another file or are finding that NickelHook is limiting your ability to structure your mod cleanly, it is likely that you are trying to get NickelHook to do too much. In addition, the custom init function, if needed, should be as fast and error-free (unless you have to choose between crashing randomly somewhere else or failing in it quickly to trigger the failsafe) as possible because it blocks Nickel's startup process.

When writing mods using NickelHook, avoid using C++ standard library features unless absolutely necessary. Try to stick to Qt or C library functions. It is OK to use C++ *language* features such as classes, but avoid things requiring runtime support. This is because the C++ standard library doesn't have any ABI compatibility guarantees and will also compile certain templates directly into the library, which will cause subtle hard-to-debug issues (which are unlikely to fail immediately and trigger the failsafe) if Kobo ever decides to upgrade it. In addition, if you're using functionality from libnickel, keep in mind binary compatibility. For further reading, KDE's [documentation](https://community.kde.org/Policies/Binary_Compatibility_Issues_With_C%2B%2B) and [examples](https://community.kde.org/Policies/Binary_Compatibility_Examples) are good resources on this subject. Remember, for most mods aimed at general users, stability, backwards and forwards compatibility where possible (and graceful failure if that isn't the case), and error handling should have priority. If failure is unavoidable and you need to decide where and how to do it, consider the balance between doing it in the init and triggering the failsafe (possibly causing other NickelHook mods to remove themselves with it), failing silently with a log message, or crashing during runtime (try to avoid this, as it can be confusing and lead to a bad user experience).

Also, keep in mind thread/memory safety, especially regarding pointers to objects in Nickel (try to limit their use to only within a hook for a Nickel function, or get them some other way) and modifying GUI objects (that should be done through Qt's signal/slots and QObject system). In addition, if you need to allocate memory for Nickel constructors, allocate more than necessary to account for changes in future versions of libnickel (and find a different way to instantiate the object or detect the size if at all possible). Also be careful with vtables for Nickel objects.

Another thing to consider is that your mod will continue to run while the Kobo is turned on. This includes during USB mass storage sessions. As such, you must ensure you aren't using excessive CPU time (this may cause slowness or battery life issues), IO (remember that you're running on a SD card) or memory (most people leave their devices on for weeks or months at a time, and you don't want Nickel to run out of memory). In addition, you should never hold on to file handles on the user storage partition `/mnt/onboard` for any length of time (longer than a few hundred milliseconds), as this is likely cause issues (and possibly corruption) when connecting over USB. The safest time to access files there is in the init function (the partition is guaranteed to be mounted at that point) or during a Qt signal handler, as you'll have complete control at those points.

All NickelHook mods should be built with [NickelTC](https://github.com/pgaskin/NickelTC) for compatibility and stability. It is OK to build using a different compiler targeting the host during development if needed for things like static analysis (ex: you can use `scan-build make clean all CROSS_COMPILE= CFLAGS= CXXFLAGS=`) or for other tooling-related functionality (ex: generating a clangd compilation database for IDE support using `make clangd CROSS_COMPILE= CC=clang CXX=clang++ CFLAGS= CXXFLAGS=`), but keep in mind that NickelTC uses an ancient GCC version and may not features from newer compilers.

When naming mods, ensure your name is unique and consistent. Do not use names under 3 letters (like `libnm.so`), as those are reserved for our mods (ex: [pgaskin/NickelMenu](https://github.com/pgaskin/NickelMenu), [pgaskin/kobo-mods](https://github.com/pgaskin/kobo-mods), [shermp/NickelDBus](https://github.com/shermp/NickelDBus)).

<!-- TODO: more usage? -->
<!-- TODO: tips and tricks? -->
