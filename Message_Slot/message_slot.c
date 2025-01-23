// Includes
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/string.h>
#include "message_slot.h"

// License
MODULE_LICENSE("GPL");

// Globals
#define DEVICE_RANGE_NAME "message_slot_manager"
#define DEVICE_FILE_NAME "message_slot"
#define MAX_MESSAGE_SLOT_AMOUNT 256


// Struct defs
struct file_operations Fops = {
    .owner = THIS_MODULE,
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .unlocked_ioctl = device_ioctl,
    .release = device_release,
};

typedef struct Channel {
    struct rb_node channel_node;
    char message[BUFF_SIZE];
    size_t size_of_message;
    unsigned int channel_id;
} Channel;

typedef struct MessageSlot {
    int minor_number;
    int channel_amount;
    struct rb_root channels;
} MessageSlot;

typedef struct MessageSlotManager {
    MessageSlot *message_slots[MAX_MESSAGE_SLOT_AMOUNT];
} MessageSlotManager;


// Statics
static MessageSlotManager manager;

// Function declaration
static int create_message_slot(struct inode*);
static Channel *create_channel(unsigned int);
static ssize_t copy_user_message(Channel*, const char __user*, size_t);
static ssize_t copy_channel_message(Channel*, const char __user*);
static MessageSlot* get_files_message_slot(struct file *);
static Channel* find_channel(struct rb_root*, unsigned int);
static int insert_channel(struct rb_root*, unsigned int);
static void cleanup_tree(struct rb_root*);

/*
    Driver methods
*/

static int __init message_slot_init(void) {
    // Variable declaration
    int register_res;

    // Register device and check for success
    register_res = register_chrdev(MAJOR_NUMBER, DEVICE_RANGE_NAME, &Fops);
    if (register_res < 0) {
        printk(KERN_ALERT "%s registration failed for %d.\n",
        DEVICE_RANGE_NAME, MAJOR_NUMBER);
        return register_res;
    }
    printk("Device registered successfully.");
    return SUCCESS;
}

int device_open(struct inode *inode , struct file *file) {
    // Variable declaration
    int minor_number;

    minor_number = create_message_slot(inode);
    if (minor_number < 0) {
        return minor_number;
    }
    file->private_data = (void *)(uintptr_t)0;
    return SUCCESS;
}

long device_ioctl(struct file *file, unsigned int command_code, unsigned long channel_id) {
    // Variable declaration
    unsigned int integer_channel_id = (unsigned int) channel_id;
    int result;
    MessageSlot *given_files_message_slot;

    if (command_code != MSG_SLOT_CHANNEL || integer_channel_id == 0) {
        return -EINVAL;
    }

    given_files_message_slot = get_files_message_slot(file);

    // Check if the requested channel already exists
    if (find_channel(&given_files_message_slot->channels, integer_channel_id) != NULL) {
        file->private_data = (void *)(uintptr_t)integer_channel_id;
        return SUCCESS;
    }

    // Insert a new channel into a MessageSlot that already has channels
    result = insert_channel(&given_files_message_slot->channels, integer_channel_id);
    if (result < 0) {
        printk("Error with memory allocation.\n");
        return result;
    }
    file->private_data = (void *)(uintptr_t)channel_id;
    given_files_message_slot->channel_amount++;
    return SUCCESS;
}

ssize_t device_write(struct file *file, const char __user* user_message, size_t message_len, loff_t *offset) {
    // Variable declaration
    ssize_t result;
    unsigned int given_files_channel_id = (unsigned int)(uintptr_t)file->private_data;
    struct rb_root *root;
    Channel *given_files_channel;
    

    if (message_len == 0 || message_len > BUFF_SIZE) {
        printk("Unsupported message length.\n");
        return -EMSGSIZE;
    }

    if (given_files_channel_id == 0){
        printk("No channel has been set for this file.\n");
        return -EINVAL;
    }

    root = &get_files_message_slot(file)->channels;

    given_files_channel = find_channel(root, given_files_channel_id);
    result = copy_user_message(given_files_channel, user_message, message_len);
    if (result  < 0) {
        printk("Error during message copying process.\n");
    }
    return result;
}

ssize_t device_read(struct file *file, char __user* user_buffer, size_t buffer_len, loff_t *offset) {
    // Variable declaration
    unsigned int given_files_channel_id = (unsigned int)(uintptr_t)file->private_data;
    struct rb_root *root;
    Channel *given_files_channel;
    size_t current_message_len;
    ssize_t result;

    if (given_files_channel_id == 0){
        printk("No channel has been set for this file.\n");
        return -EINVAL;
    }

    root = &get_files_message_slot(file)->channels;

    given_files_channel = find_channel(root, given_files_channel_id);
    current_message_len = given_files_channel->size_of_message;
    if (current_message_len == 0) {
        printk("No message exists in this channel.\n");
        return -EWOULDBLOCK;
    }

    if (given_files_channel->size_of_message > buffer_len) {
        printk("Buffer too small to hold the message.\n");
        return -ENOSPC;
    }

    result = copy_channel_message(given_files_channel, user_buffer);
    if (result < 0) {
        printk("Error during message copying process.\n");
    }
    return result;
}

int device_release(struct inode *inode, struct file *file) {
    // Variable declaration
    int minor_number;
    unsigned int file_channel_id = (unsigned int)(uintptr_t)file->private_data;
    MessageSlot *given_files_message_slot;

    minor_number = iminor(inode);
    given_files_message_slot = manager.message_slots[minor_number];

    if (file_channel_id == 0) {
        file->private_data = NULL;
        printk("Channel closed.\n");
        return SUCCESS;
    }

    file->private_data = NULL;
    printk("Channel closed.\n");
    return SUCCESS;
}

static void __exit message_slot_exit(void) {
    // Varaible declaration
    int i;
    MessageSlot *current_slot;
    struct rb_root *current_root;

    for (i = 0; i < MAX_MESSAGE_SLOT_AMOUNT; i++) {
        current_slot = manager.message_slots[i];
        if (current_slot != NULL) {
            current_root = &current_slot->channels;
            cleanup_tree(current_root);
            kfree(current_slot);
            manager.message_slots[i] = NULL;
        }
    }

    unregister_chrdev(MAJOR_NUMBER, DEVICE_RANGE_NAME);
    printk("Module unloaded successfully.\n");
}   

/*
    Driver helper functions
*/
static int create_message_slot(struct inode *inode) {
    // Variable declaration
    MessageSlot *new_slot;
    int minor_number;

    // Check for existance
    minor_number = iminor(inode);
    if (manager.message_slots[minor_number] != NULL){
        return minor_number;
    }

    new_slot = kmalloc(sizeof(MessageSlot), GFP_KERNEL);
    if (new_slot == NULL) {
        printk("failed to allocate memory.\n");
        return -ENOMEM;
    }
    new_slot->minor_number = minor_number;
    new_slot->channel_amount = 0;
    new_slot->channels = RB_ROOT;
    manager.message_slots[minor_number] = new_slot;
    return minor_number;
}

static Channel* create_channel(unsigned int channel_id) {
    // Variable declaration
    Channel *new_channel;

    new_channel = kmalloc(sizeof(Channel),GFP_KERNEL);
    if (new_channel == NULL) {
        return NULL;
    }

    RB_CLEAR_NODE(&new_channel->channel_node);
    new_channel->channel_id = channel_id;
    memset(new_channel->message, 0, BUFF_SIZE);
    new_channel->size_of_message = 0;
    return new_channel;
}

static ssize_t copy_user_message(Channel *channel, const char __user*  user_message, size_t message_len) {
    // Variable declaration
    int result;
    int i;
    char temp_message_arr[BUFF_SIZE];

    memset(temp_message_arr, 0, BUFF_SIZE);
    for (i = 0; i < message_len; i++) {
        result = get_user(temp_message_arr[i], &user_message[i]);
        if (result < 0) {
            return result;
        }
    }

    memcpy(channel->message, temp_message_arr, message_len);
    channel->size_of_message = message_len;
    return (ssize_t)message_len;
}

static ssize_t copy_channel_message(Channel *channel, const char __user*  user_buffer) {
    // Variable declaration
    int result;
    int i;
    int channel_message_len = channel->size_of_message;

    for (i = 0; i < channel_message_len; i++) {
        result = put_user(channel->message[i], &user_buffer[i]);
        if (result < 0) {
            return result;
        }
    }
    return (ssize_t)channel_message_len;
}

static MessageSlot* get_files_message_slot(struct file *file) {
    // Variable declaration
    int minor_number;

    minor_number = iminor(file->f_inode);
    return manager.message_slots[minor_number];
}

/*
    Red black tree methods
*/

static Channel* find_channel(struct rb_root *root, unsigned int search_id) {
    // Variable declaration
    struct rb_node *current_node = root->rb_node;
    Channel *current_channel;

    while(current_node) {
        current_channel = container_of(current_node, Channel, channel_node);
        
        if (current_channel->channel_id < search_id) {
            current_node = current_node->rb_right;
        }

        else if (current_channel->channel_id > search_id) {
            current_node = current_node->rb_left;
        }

        else{
            return current_channel;
        }
    }
    return NULL;
}

static int insert_channel (struct rb_root *root, unsigned int search_id) {
    // Variable declaration
    struct rb_node **current_node = &(root->rb_node);
    struct rb_node *parent = NULL;
    Channel *current_channel;

    while(*current_node) {
        current_channel = container_of(*current_node, Channel, channel_node);
        parent = *current_node;
        if (current_channel->channel_id  < search_id) {
            current_node = &(*current_node)->rb_right;
        }

        else if (current_channel->channel_id  > search_id) {
            current_node = &(*current_node)->rb_left;
        }

        else {
            return -EEXIST;
        }
    }

    current_channel = create_channel(search_id);
    if (current_channel == NULL) {
        return -ENOMEM;
    }
    rb_link_node(&current_channel->channel_node, parent, current_node);
    rb_insert_color(&current_channel->channel_node, root);
    return SUCCESS;
}

static void cleanup_tree(struct rb_root *root) {
    // Varaible declaration
    Channel *current_channel;
    Channel *next_channel;

    if (RB_EMPTY_ROOT(root)) {
        return;
    }

    rbtree_postorder_for_each_entry_safe(current_channel, next_channel, root, channel_node) {
        rb_erase(&current_channel->channel_node, root);
        kfree(current_channel);
    }
}

module_init(message_slot_init);
module_exit(message_slot_exit);


/* Used sources
    1. Linux kernel API: https://www.kernel.org/doc/html/v4.13/core-api/kernel-api.html
    2. Error codes: https://man7.org/linux/man-pages/man3/errno.3.html
    3. Red Black trees: https://en.wikipedia.org/wiki/Red%E2%80%93black_tree
    4. Red Black trees - linux kernel: https://en.wikipedia.org/wiki/Red%E2%80%93black_tree
    5. Red Black trees - guide: https://www.kernel.org/doc/html/v5.9/core-api/rbtree.html
    6. General driver code: https://docs.oracle.com/cd/E26502_01/html/E29051/loading-112.html
*/