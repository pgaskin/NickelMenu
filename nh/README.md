<h1 align="center">NickelHook</h1>

A robust library for creating Nickel mods.

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

<!-- TODO: usage -->
