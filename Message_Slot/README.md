# Message Slot Kernel Module

## Overview
This project implements a kernel module that provides a new IPC mechanism called "Message Slot."

## Files
- `message_slot.c` and `message_slot.h`: Kernel module implementation.
- `message_sender.c`: User-space program to send messages.
- `message_reader.c`: User-space program to read messages.

## Usage
1. Load the kernel module:
   ```bash
   sudo insmod message_slot.ko
2. Create a message slot file:
   ```bash
   sudo mknod /dev/slot0 c 235 0
3. Change file permissions:
   ```bash
   sudo chmod 666 /dev/slot0
4. Run the message sender
   ```bash
   ./message_sender /dev/slot0 1 "Hello, world!"
5. Run the message reader
   ```bash
   ./message_reader /dev/slot0 1

## Compilation
1. Use the Makefile provided:
   ```bash
   make
