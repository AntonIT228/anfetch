# anfetch

A simple, modular system fetch tool written in C++ for Arch Linux.

## Features

- Modular architecture — reorder modules by moving lines in build_modules()
- Colors — labels are colored, logo is not
- All real filesystems with type (ext4, btrfs, ntfs3, ...)
- Clean GPU names (no (rev a1), no Corporation)
- Correct OS age from pacman.log
- Shell version detection
- CLI: --show-<module>, --no-<module>, --help
- 55+ distro logos

## Build

g++ -O2 -std=c++17 main.cpp -o anfetch

## Usage

./anfetch
./anfetch --no-cpu
./anfetch --show-disks
./anfetch --help

## Configuration

There is no config file. Configuration is the code itself.
To change module order — edit build_modules() in main.cpp and recompile.

## License

Unlicense
