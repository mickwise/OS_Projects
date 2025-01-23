// Includes
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>

// Struct defs
typedef struct QueueElem{
    void *item;
    struct QueueElem *next;
} QueueElem;

typedef struct {
    QueueElem *head;
    QueueElem *tail;
    int queue_size;
    size_t visited_items;
} Queue;

typedef struct ThreadNode {
    cnd_t *condition;
    struct ThreadNode *next;
} ThreadNode;

typedef struct {
    ThreadNode *head;
    ThreadNode *tail;
    int queue_size;
}ThreadQueue;


// Function declaration
void initQueue(void);
void enqueue(void*);
void* dequeue(void);
bool tryDequeue(void**);
size_t visited(void);
void destroyQueue(void);
static void init_item_queue();
static void init_thread_queue();
static QueueElem* init_item(void*);
static void add_element_to_item_queue(QueueElem*);
static void check_for_waiting_threads();
static void deal_with_empty_queue();
static void* item_dequeue_impl();
static void thread_enqueue(cnd_t *);
static ThreadNode* init_thread_node(cnd_t*);
static void add_element_to_thread_queue(ThreadNode*); 
static cnd_t* thread_dequeue();
static QueueElem* dequeue_first_non_acquired_element(int);


// Global variables declarations
static Queue *item_queue;
static ThreadQueue *thread_queue;
static mtx_t queue_lock;

/*Interface methods*/

/**
 * @brief Initializes the item queue, thread queue and lock.
 */
void initQueue(void) {

    init_item_queue();

    init_thread_queue();

    mtx_init(&queue_lock, mtx_plain);
}

/**
 * @brief Enqueues an item into the item queue and wakes up a waiting thread if one exists.
 *
 * The method starts by an attempt at locking, then uses two helper methods to add an element into 
 * the queue and check (and wake up if exists) for waiting threads.
 *
 * @param element_to_enqueue  A pointer to the element the user wants to enqueue.
 *
 */
void enqueue(void *element_to_enqueue) {
    // Variable declaration
    QueueElem *new_element;

    new_element = init_item(element_to_enqueue);
    mtx_lock(&queue_lock);

    add_element_to_item_queue(new_element);
    check_for_waiting_threads();

    mtx_unlock(&queue_lock);
}

/**
 * @brief Tries to dequeue an element from the item queue. If queue is empty, sleeps until an element
 * is added to the queue and its turn arrives.
 *
 * The method calls a helper method to see if the queue is currently empty and deals with it by enqueueing 
 * itself into the thread queue with a unique CV. This insures that it will be woken up when the item queue
 * is not empty and it is the first in line to dequeue it.
 *
 * @return The item field of the dequeued queue element.
 *
 */
void* dequeue(void) {
    // Variable declaration
    void *item_to_return;

    mtx_lock(&queue_lock);

    deal_with_empty_queue();
    item_to_return = item_dequeue_impl();

    mtx_unlock(&queue_lock);

    return item_to_return;
}

/**
 * @brief Tries to dequeue an element with out blocking. If possible saves it into the provided pointer
 * and returns true, otherwise returns false.
 *
 * The method checks if there are more enqueued elements then threads waiting. If there are it dequeues the first element
 * that does not have a thread waiting to dequeue it (the first element in a larger position then the amount of waiting 
 * threads). If there are no waiting threads it just dequeues the head of the queue normally.
 *
 * @param item_of_element_to_dequeue A pointer to the location in memory in which to save the dequeued item, if able to.
 * ...
 * @return True if an element was successfully dequeued, other wise false.
 *
 */
bool tryDequeue(void **item_of_element_to_dequeue) {
    // Variable declaration
    QueueElem *dequeued_element;

    mtx_lock(&queue_lock);

    if (item_queue->queue_size > thread_queue->queue_size) {
        if (thread_queue->queue_size == 0) {
            *item_of_element_to_dequeue = item_dequeue_impl();
        }
        else {
            dequeued_element = dequeue_first_non_acquired_element(thread_queue->queue_size);
            *item_of_element_to_dequeue = dequeued_element->item;
            free(dequeued_element);
        }
        mtx_unlock(&queue_lock);
        return true;
    }
    mtx_unlock(&queue_lock);
    return false;
}

/**
 * @brief Returns the amount of elements that where enqueued and then dequeued.
 *
 * @return  Returns the amount of elements that where enqueued and then dequeued saved in the 
 * visisted field of the Queue struct.
 * 
 */
size_t visited(void) {
    return item_queue->visited_items;
}

/**
 * @brief Destroys the item queue, thread queue and lock. 
 *
 * Iterates through both queues and dequeues all elements, then frees them, the queues themselves and the lock.
 *
 */
void destroyQueue(void) {
    // Variable declaration
    int i;
    int amount_of_items; 
    int amount_of_threads;


    mtx_lock(&queue_lock);

    amount_of_items = item_queue->queue_size;
    for (i = 0; i < amount_of_items; i++){
        item_dequeue_impl();
    }
    
    amount_of_threads = thread_queue->queue_size;
    for (i = 0; i < amount_of_threads; i++){
        thread_dequeue();
    }

    free(item_queue);
    free(thread_queue);

    mtx_unlock(&queue_lock);
    mtx_destroy(&queue_lock);
}

/*Private methods*/

/**
 * @brief Initializes the global item queue instance.
 *
 * @note Used by initQueue.
 *
 */
static void init_item_queue() {
    item_queue = malloc(sizeof(Queue));
    item_queue->head = NULL;
    item_queue->tail = NULL;
    item_queue->queue_size = 0;   
    item_queue->visited_items = 0; 
}

/**
 * @brief Initializes the global thread queue instance.
 *
 * @note Used by initQueue.
 *
 */
static void init_thread_queue() {
    thread_queue = malloc(sizeof(ThreadQueue));
    thread_queue->head = NULL;
    thread_queue->tail = NULL;
    thread_queue->queue_size = 0;   
}

/**
 * @brief Initializes a QueueElem instance with the given element as the item.
 *
 * @param element_to_enqueue A pointer to the element to be enqueued.
 * ...
 * @return A QueueElem struct instance with element_to_enqueue as the item.
 *
 * @note Used by enqueue.
 *
 */
static QueueElem* init_item(void *element_to_enqueue) {
    // Variable declaration
    QueueElem *new_element;

    new_element = malloc(sizeof(QueueElem));
    new_element->item = element_to_enqueue;
    new_element->next = NULL;

    return new_element;
}

/**
 * @brief Adds an element to the appropriate position in the item queue.
 * 
 * Adds an element created by init_item to the head of the queue if it is empty, 
 * other wise to the tail.
 *
 * @param element_to_add A pointer to the QueueElem instance generated by init_item.
 *
 * @note Used by enqueue.
 *
 */
static void add_element_to_item_queue(QueueElem *element_to_add) {
    if (item_queue->queue_size == 0) {
        item_queue->head = element_to_add;
        item_queue->tail = element_to_add;
        item_queue->queue_size++;
    }

    else {
        item_queue->tail->next = element_to_add;
        item_queue->tail = element_to_add;
        item_queue->queue_size++;
    }
}

/**
 * @brief Checks if a thread is waiting to dequeue an elmenet from the item queue and wakes it up if it is.
 * 
 * @return A QueueElem struct instance with element_to_enqueue as the item.
 *
 * @note Used by enqueue.
 *
 */
static void check_for_waiting_threads() {
    // Variable declaration
    cnd_t *thread_to_wake;

    if (thread_queue->queue_size > 0) { 
        thread_to_wake = thread_dequeue(); 
        cnd_signal(thread_to_wake);
    }
}

/**
 * @brief Checks if the queue is empty, adds a condition variable to the thread queue and calls wait on it.
 * 
 * The method first checks if the queue is empty. If it is it initializes a unique CV for the current thread and 
 * initializes it. Then it enqueues it into the thread queue using a helper method and calls wait on it. Lastly
 * it destroys the CV upon wake up. This insures threads wake up in FIFO order since their unique CVs are
 * kept in a queue.
 *
 * @note Used by dequeue.
 *
 */
static void deal_with_empty_queue() {
    // Variable declaration
    cnd_t current_thread_cond;

    if (item_queue->queue_size == 0) {
        cnd_init(&current_thread_cond);
        thread_enqueue(&current_thread_cond);
        cnd_wait(&current_thread_cond, &queue_lock);
        cnd_destroy(&current_thread_cond);
    }

}

/**
 * @brief Dequeues an element from the item queue, frees its wrapper QueueElem and returns it.
 * 
 * This method dequeues a QueueElem instance from the item queue. Then it chekcs if the queue is empty and, if it
 * is, it sets its tail to NULL. then it frees the wrapper QueueElem and returns its item field.
 *
 * @return The item filed of the dequeued element.
 *
 * @note Used by dequeue, tryDequeue and destroyQueue.
 *
 */
static void* item_dequeue_impl() {
    // Variable declaration
    QueueElem *dequeued_element;
    void *item_to_return;

    dequeued_element = item_queue->head;
    item_queue->head = dequeued_element->next; 
    item_queue->queue_size--;
    item_queue->visited_items++;

    if (item_queue->queue_size == 0) {
        item_queue->tail = NULL;
    }
    item_to_return = dequeued_element->item;
    free(dequeued_element);
    return item_to_return;
}

/**
 * @brief Enqueues a threads unique CV into the thread queue.
 * 
 * Uses a helper method to wrap the CV into a ThreadNode struct instance, then uses another one to 
 * add it to the thread queue.
 *
 * @param condition A pointer to the condition variable unique to the current thread.
 *
 * @note Used by deal_with_empty_queue.
 */
static void thread_enqueue(cnd_t *condition) {
    // Variable declaration
    ThreadNode *new_element;

    new_element = init_thread_node(condition);
    add_element_to_thread_queue(new_element);
}

/**
 * @brief Initializes a ThreadNode struct instance with the provided CV in the condition field.
 *
 * @param condition A pointer to the condition variable unique to the current thread.
 * 
 * @return A ThreadNode instance with condition in its condition field.
 *
 * @note Used by thread_enqueue.
 *
 */
static ThreadNode* init_thread_node(cnd_t *condition) {
    // Variable declaration
    ThreadNode *new_element;

    new_element = malloc(sizeof(ThreadNode));
    new_element->condition = condition;
    new_element->next = NULL;

    return new_element;
}

/**
 * @brief Adds a ThreadNode to the appropriate position in the thread queue.
 * 
 * Adds an ThreadNode created by init_thread_node to the head of the thread queue if it is empty, 
 * other wise to the tail.
 *
 * @param element_to_add A pointer to the ThreadNode instance generated by init_item.
 *
 * @note Used by thread_enqueue.
 *
 */
static void add_element_to_thread_queue(ThreadNode *element_to_add) {
    if (thread_queue->queue_size == 0) {
        thread_queue->head = element_to_add;
        thread_queue->tail = element_to_add;
        thread_queue->queue_size++;
    }

    else {
        thread_queue->tail->next = element_to_add;
        thread_queue->tail = element_to_add;
        thread_queue->queue_size++;
    }
}

/**
 * @brief Dequeues an element from the thread queue, frees its wrapper ThreadNode and returns it.
 * 
 * This method dequeues a ThreadNode instance from the thread queue. Then it chekcs if the queue is empty and, if it
 * is, it sets its tail to NULL. then it frees the wrapper ThreadNode and returns its condition field.
 *
 * @return The condition held in the ThreadNode wrappers condition field.
 *
 * @note Used by check_for_waiting_threads and destroyQueue.
 *
 */
static cnd_t* thread_dequeue() {
    // Variable declaration
    ThreadNode *dequeued_element;
    cnd_t *item_to_return;

    dequeued_element = thread_queue->head;
    thread_queue->head = dequeued_element->next; 
    thread_queue->queue_size--;

    // Check if queue is empty 
    if (thread_queue->queue_size == 0) {
        thread_queue->tail = NULL;
    }
    item_to_return = dequeued_element->condition;
    free(dequeued_element);
    return item_to_return;
}

/**
 * @brief Iterates through the item queue until finding the element in the index + 1 position. Then dequeues and
 * returns it.
 * 
 *
 * @param index The index of the position of the first "non-paired" element in the item queue (not paired
 * to a waiting thread that is).
 * 
 * @return The first QueueElem not paired to a waiting thread.
 *
 * @note Used by tryDequeue.
 *
 */
static QueueElem* dequeue_first_non_acquired_element(int index) {
    // Variable declaration
    int i;
    QueueElem *current_elem = item_queue->head;
    QueueElem *element_to_return;

    for (i = 1; i < index; i++) {
        current_elem = current_elem->next;
    }

    element_to_return = current_elem->next;
    current_elem->next = element_to_return->next;
    item_queue->queue_size--;
    item_queue->visited_items++;

    return element_to_return;
}

/* Used sources
    1. C booleans: https://www.w3schools.com/c/c_booleans.php
    2. Concurrency methods: https://en.cppreference.com/w/c/thread
*/