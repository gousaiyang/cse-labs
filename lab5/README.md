# Lab 5: Log-based Version Control

## Introduction
Use log to implement version control functions (commit, undo, redo) for YFS.

## Hints
- The RPC library requires that RPC functions have at least one input parameter and one output parameter, so you should use some dummy values to fill them. See [this commit](../../../commit/54e2e0a4037cc9868b6e829200ae9eed50d008c8) for details.
- You should not rely on the existing debug logs as they are disordered and unhelpful. You should develop your own new log file and format.
- You should address concurrency issues of version control operations (although they are not tested by the test script).
- The desired implementation is to log old and new data of every FS operation, however I missed it and put the whole disk data of every commit into the log. Although it deals well with the test script, it is certainly too inefficient for a real FS.
