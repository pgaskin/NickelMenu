<h1 align="center">NickelHook</h1>

A robust library for creating Nickel mods.

**Features:**
- Generates a Qt plugin designed for use with Nickel.
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
  - clangd compile_flags.json generation.
  - KoboRoot.tgz generation with custom files.
  - Can also install to a DESTDIR instead.
  - Supports custom CPPFLAGS/CFLAGS/CXXFLAGS/LDFLAGS.
  - gitignore generation.
  - Git versioning (but can be overridden).
  - Supports custom targets.

## Installation
Copy this folder to your project, or include it as a git submodule.

<!-- TODO: usage -->
