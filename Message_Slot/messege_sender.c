#include <stdlib.h>
#include <stdio.h>
#include <string.h> 
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include "message_slot.h"

#define CORRECT_NUMBER_OF_ARGUMENTS 4
#define FAILURE 1

int main(int argc, char *argv[]) {   
    // Variable declaration 
    ssize_t message_len;
    int file_descriptor;
    unsigned long channel_id;


    if (argc != CORRECT_NUMBER_OF_ARGUMENTS){
        perror("Wrong number of arguments.");
        exit(FAILURE);
    }

    channel_id = (unsigned long)atoi(argv[2]);

    // Open device with write only access
    file_descriptor = open(argv[1], O_WRONLY);
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

    // Write the message to the requested channel
    message_len = strlen(argv[3]);
    if (write(file_descriptor, argv[3], message_len) != message_len) {
        perror("An error has occurred when trying to write the message to the specified channel.");
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
