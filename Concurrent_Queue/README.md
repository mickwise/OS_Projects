# Concurrent Queue

## Overview
This project implements a generic, thread-safe, FIFO queue library that supports concurrent enqueue and dequeue operations.

## Functions
- `initQueue()`: Initializes the queue.
- `destroyQueue()`: Cleans up resources.
- `enqueue(void*)`: Adds an item to the queue.
- `dequeue(void*)`: Removes an item, blocking if the queue is empty.
- `tryDequeue(void**)`: Attempts to dequeue without blocking.
- `visited()`: Returns the total number of items processed.

## Compilation
Compile the code using:
```bash
gcc -O3 -D_POSIX_C_SOURCE=200809 -Wall -std=c11 -pthread -c queue.c

