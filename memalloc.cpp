#include <unistd.h>
#include <string.h>
#include <pthread.h>
/* Only for the debug printf */
#include <stdio.h>
union header
{
    struct
    {
        size_t size;
        unsigned is_free;
        union header *next;
    } s;
    /* force the header to be aligned to 16 bytes */
    long x;
};

typedef union header header_t;

pthread_mutex_t global_malloc_lock;
header_t *head, *tail;

header_t *get_free_block(size_t size) // try to find a free block of memory
{
    header_t *curr = head; // start from the head of the list
    while (curr)           // traverse the list
    {
        if (curr->s.is_free && curr->s.size >= size) // if the block is free and has enough space
            return curr;                             // return the pointer to the block
        curr = curr->s.next;                         // move to the next block if we didn't find a useable block
    }
    return NULL;
}

void *malloc(size_t size)
{
    size_t total_size; // size of the block, including the header and the requested size
    void *block;       // pointer to the new block
    header_t *header;  // pointer to the header of the new block
    if (!size)         // if size is 0, return NULL
        return NULL;
    pthread_mutex_lock(&global_malloc_lock); // lock the mutex
    header = get_free_block(size);           // get a free block
    if (header)                              // if a free block is found
    {
        header->s.is_free = 0; // mark it as not free
        pthread_mutex_unlock(&global_malloc_lock);
        return (void *)(header + 1); // return the address of the block after the header
    }
    // if no free block is found, allocate a new block
    total_size = sizeof(header_t) + size; // calculate the total size needed
    block = sbrk(total_size);
    // Brk returns a pointer to the new end of memory if successful; otherwise -1 with errno set to indicate why the allocation failed.
    if (block == (void *)-1) // pointer of -1 means that the request for memory failed
    {
        pthread_mutex_unlock(&global_malloc_lock); // unlock the mutex
        return NULL;
    }
    // if the request for memory was successful, create a new header for the block
    header = (header_t *)block;
    header->s.size = size;
    header->s.is_free = 0;
    header->s.next = NULL;
    if (!head)                 // if the list is empty
        head = header;         // set the head to the new block
    if (tail)                  // if the list is not empty
        tail->s.next = header; // set the next block of the tail to the new block
    tail = header;             // if the list is empty, set the tail to the new block
    pthread_mutex_unlock(&global_malloc_lock);
    return (void *)(header + 1); // return the address of the block after the header
}
