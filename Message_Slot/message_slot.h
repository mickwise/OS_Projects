#ifndef MESSAGE_SLOT_H
#define MESSAGE_SLOT_H

#ifdef __KERNEL__
#include <linux/rbtree.h>
#include <linux/fs.h>       
#include <linux/uaccess.h>   
#else
#include <sys/ioctl.h>
#endif

// Constants
#define MAJOR_NUMBER 235
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUMBER, 0, unsigned long)
#define SUCCESS 0
#define BUFF_SIZE 128

#ifdef __KERNEL__
// Driver Methods
/**
 * device_open - Opens a message slot device.
 * @inode: Pointer to the inode object.
 * @file: Pointer to the file object.
 *
 * Returns 0 on success or a negative error code on failure.
 */
int device_open(struct inode *inode, struct file *file);

/**
 * device_ioctl - Associates a specified channel ID with the file descriptor.
 * @file: Pointer to the file object.
 * @command_code: Indicates the ioctl command; expected to be MSG_SLOT_CHANNEL.
 * @channel_id: The channel ID to associate with this file descriptor.
 *
 * Sets up the file to use the specified channel ID for subsequent read/write 
 * operations. Returns 0 on success or a negative error code on failure.
 */
long device_ioctl(struct file *file, unsigned int command_code, unsigned long channel_id);


/**
 * device_write - Writes data to the currently associated channel.
 * @file: Pointer to the file object.
 * @user_message: Pointer to the user-space buffer containing the data to write.
 * @message_len: Number of bytes to write.
 * @offset: File offset (unused in this context).
 *
 * Copies @message_len bytes from @user_message into the channel's buffer. 
 * Returns the number of bytes written on success or a negative error code on failure.
 */
ssize_t device_write(struct file *file, const char __user* user_message, size_t message_len, loff_t *offset);

/**
 * device_read - Reads data from the currently associated channel.
 * @file: Pointer to the file object.
 * @user_buffer: Pointer to the user-space buffer to store the read data.
 * @buffer_len: Size of the user-space buffer.
 * @offset: File offset (unused in this context).
 *
 * Copies the channel's stored data into @user_buffer, up to @buffer_len bytes. 
 * Returns the number of bytes read on success or a negative error code on failure.
 */
ssize_t device_read(struct file *file, char __user* user_buffer, size_t buffer_len, loff_t *offset);

/**
 * device_release - Releases the message slot device.
 * @inode: Pointer to the inode object.
 * @file: Pointer to the file object.
 *
 * Cleans up or detaches the currently associated channel if necessary and frees 
 * any resources if this is the last reference to the slot. Returns 0 on success 
 * or a negative error code on failure.
 */
int device_release(struct inode *inode, struct file *file);

#endif

#endif