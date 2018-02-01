# Lab 4: Lock Server

## Introduction
Implement a server that manages locks, and use the lock server to add locking to your YFS client to ensure correctness of concurrent file operations.

## Hints
- You can either choose to acquire a lock for every inode, or acquire a big lock for every FS operation (will hinder benign concurrency). If you choose to acquire a lock for every inode, you should also make `alloc_inode()` and `alloc_block()` operations atomic in the inode manager.
- Be sure to release every lock acquired, and avoid deadlock.
