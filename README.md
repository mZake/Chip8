# CHIP-8 Interpreter

Chip8 is a single-file, simple and minimalist implementation of the CHIP-8 virtual machine written in modern C++ with minimal dependencies.

## Features
- **Single-file:** The interpreter is implemented in a single source file.
- **Modern C++:** Written in portable C++17.
- **Minimalist:** Focused on essential functionality.
- **Low Dependency:** SDL3 is the only dependency.

## How to Build

Currently there are no releases available, so you need to build from source. But no need to worry, Chip8 was designed to be straightforward to build!

**Prerequisites**
- CMake 3.15+
- C++ Compiler (GCC, Clang, MSVC)

Clone the respository and its submodules:
```sh
git clone https://github.com/mZake/Chip8.git --recursive
cd Chip8
```

Configure and build using CMake:
```sh
cmake -S . -B build
cmake --build build
```

The executable is placed in the `build/$<CONFIGURATION>` directory (`build/Debug` or `build/Release`).

## References

- [Cowgod's CHIP-8 Technical Reference](http://devernay.free.fr/hacks/chip8/C8TECH10.HTM)
- [CHIP-8 Wikipedia](https://en.wikipedia.org/wiki/CHIP-8)
