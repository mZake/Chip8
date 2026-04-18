# CHIP-8 Interpreter

Chip8 is a single-file, simple and minimalist implementation of the CHIP-8 virtual machine written in modern C++ with minimal dependencies.

## Features
- **Single-file:** The interpreter is implemented in a single source file.
- **Modern C++:** Written in portable C++17.
- **Minimalist:** Focused on essential functionality.
- **Low Dependency:** SDL3 is the only dependency.

## Build Instructions

Before proceeding, make sure you have CMake 3.15+ and a C++17 compiler installed.

Clone the repository using Git:
```sh
git clone https://github.com/mZake/Chip8.git --recursive
cd Chip8
```

Configure and build using CMake:
```sh
cmake -S . -B build
cmake --build build
```

By default, the project is built in Debug mode. You can change this by setting the `CMAKE_BUILD_TYPE` option:
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

The build artifacts are placed in the `build/<Config>` directory.

## How To Use
Load CHIP-8 ROM:
```sh
chip8 /path/to/rom
```

While Chip8 is executing, you can use the following options:
- Ctrl+S: Save snapshot of current state
- Ctrl+L: Load latest saved snapshot
- Ctrl+P: Pause/Resume execution

## References

- [Cowgod's CHIP-8 Technical Reference](http://devernay.free.fr/hacks/chip8/C8TECH10.HTM)
- [CHIP-8 Wikipedia](https://en.wikipedia.org/wiki/CHIP-8)
