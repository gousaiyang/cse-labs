# Lab 7: Erasure Coding

## Introduction
This lab provides YFS with the ability of fault tolerance (to be more specific, the ability to detect and fix bit flips).

## Hints
- You can implement fault tolerance by redundant encoding (like hamming code) or replication, or even hybrid methods, as long as you can tolerate the test cases. You can reassign disk blocks as long as it does not break the rules. You may also adjust FS parameters in `inode_manager.h`.
- My implementation uses redundant encoding. When dealing with file metadata, extra code is put into some designated data blocks. This is not so elegant, and fault tolerance of indirect blocks is not addressed (actually not tested by the test scripts). [An awesome implementation](https://github.com/TerCZ/CSE-labs-2017/blob/master/lab7/inode_manager.cc) from @TerCZ implements a mapping from abstract (virtual) blocks to actual (physical) blocks by applying encoding and decoding in `block_manager::write_block()` and `block_manager::read_block()` methods, which deals with all things elegantly.
