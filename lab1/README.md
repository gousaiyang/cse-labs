# Lab 1: Inode Manager

## Introduction
This lab implements an inode manager to support your file system (YFS). It covers the lower layers of the whole FS.

## Hints
- You can design the disk layout as you wish.
- Some macros provided in `inode_manager.h` are incorrect. You should correct them by yourself (according to your design of disk layout).
- Be careful to use the correct function to manipulate data. For example, the constructor `std::string(s)` will regard `s` as a null-terminated string, while `std::string(s, length)` will read data of the given length starting at `s`.
