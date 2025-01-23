#include <stdlib.h>
#include <stdio.h>
#include <string.h> 
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include "message_slot.h"

#define CORRECT_NUMBER_OF_ARGUMENTS 3
#define FAILURE 1

int main(int argc, char *argv[]) {   
    // Variable declaration 
    char message[BUFF_SIZE];
    ssize_t message_len;
    int file_descriptor;
    unsigned long channel_id;


    if (argc != CORRECT_NUMBER_OF_ARGUMENTS){
        perror("Wrong number of arguments.");
        exit(FAILURE);
    }

    channel_id = (unsigned long)atoi(argv[2]);

    // Open device with read only access
    file_descriptor = open(argv[1], O_RDONLY);
    if (file_descriptor < 0) {
        perror("An error has occurred when trying to open the message slot.");
        exit(FAILURE);
    }

    // Attach a channel with the specified channel ID to the device 
    if (ioctl(file_descriptor, MSG_SLOT_CHANNEL, channel_id) < 0) {
        perror("An error has occurred when trying to connect the device to a channel.");
        close(file_descriptor);
        exit(FAILURE);
    }
    message_len = read(file_descriptor, message, BUFF_SIZE);
    // Read the message to the requested channel
    if (message_len < 0) {
        perror("An error has occurred when trying to read the message from the specified channel.");
        close(file_descriptor);
        exit(FAILURE);
    }

    // Write message to the standard output
    if (write(STDOUT_FILENO, message, message_len) != message_len) {
        perror("An error has occurred while trying to print the message.");
        close(file_descriptor);
        exit(FAILURE);
    }

    // Close the device and exit
    if (close(file_descriptor) < 0) {
        perror("An error has occurred when trying to close the device.");
        exit(FAILURE);
    }
    exit(SUCCESS);
}
