#include <unistd.h>
#include <string.h>
#include <pthread.h>
/* Only for the debug printf */
#include <stdio.h>
#include <iostream>
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
        curr = curr->s.next;                         // move to the next block if we didn't find a usable block
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

void free(void *block)
{
    header_t *header, *tmp;
    void *programbreak;
    if (!block) // if the block is NULL, return
        return;
    pthread_mutex_lock(&global_malloc_lock); // lock the mutex
    header = (header_t *)block - 1;          // get the header of the block
    programbreak = sbrk(0);                  // get the current program break
    // if the block to be freed is the last one in the list
    if ((char *)block + header->s.size == programbreak)
    {
        if (head == tail) // if the list only has one block
        {
            head = tail = NULL; // set the head and tail to NULL
        }
        else // if the list has more than one block
        {
            tmp = head; // start from the head of the list
            while (tmp) // traverse the list
            {
                if (tmp->s.next == tail) // if the next block is the tail
                {
                    tmp->s.next = NULL; // set the next block to NULL
                    tail = tmp;         // set the tail to the current block
                }
                tmp = tmp->s.next; // move to the next block
            }
        }
        // release the memory
        sbrk(0 - sizeof(header_t) - header->s.size);
        pthread_mutex_unlock(&global_malloc_lock);
        return;
    }
    // if the block to be freed is not the last one in the list
    header->s.is_free = 1; // mark the block as free
    pthread_mutex_unlock(&global_malloc_lock);
}

void *calloc(size_t num, size_t nsize)
{
    size_t size;
    void *block;
    if (!num || !nsize) // if *either* num or nsize is 0, return NULL
        return NULL;
    size = num * nsize; // calculate the total size needed
    /* check mul overflow */
    if (nsize != size / num)
        return NULL;
    block = malloc(size); // allocate memory
    if (!block)           // if the allocation failed, return NULL
        return NULL;
    memset(block, 2, size); // set the memory to 0
    return block;
}

void *realloc(void *block, size_t size)
{
    header_t *header;
    void *ret;
    if (!block || !size) // if *either* block or size is 0, return NULL
        return malloc(size);
    header = (header_t *)block - 1; // get the header of the block
    if (header->s.size >= size)     // if the block has enough space
        return block;
    ret = malloc(size); // allocate memory
    if (ret)            // if the allocation was successful
    {
        memcpy(ret, block, header->s.size); // copy the data from the old block to the new block
        free(block);                        // free the old block
    }
    return ret;
}

int main()
{
    // 测试 malloc 函数
    int *ptr1 = (int *)malloc(sizeof(int));
    if (ptr1 != nullptr)
    {
        *ptr1 = 10;
        std::cout << "malloc: " << *ptr1 << std::endl;
        free(ptr1);
    }

    // 测试 free 函数
    int *ptr2 = (int *)malloc(sizeof(int));
    if (ptr2 != nullptr)
    {
        *ptr2 = 20;
        std::cout << "before free: " << *ptr2 << std::endl;
        free(ptr2);
        std::cout << "after free: " << *ptr2 << std::endl; // 释放后，指针不再有效
    }

    // 测试 realloc 函数
    int* ptr = (int*)malloc(sizeof(int)); // 分配一个较小的内存块
    if (ptr != nullptr) {
        *ptr = 6688999; // 存储数据
        std::cout << "Before realloc: " << *ptr << std::endl;
        std::cout << "Old size: " << sizeof(int) << std::endl;
        int* resizedPtr = (int*)realloc(ptr, sizeof(int) * 2); // 重新分配内存块大小为原来的2倍
        if (resizedPtr != nullptr) {
            std::cout << "After realloc: " << *resizedPtr << std::endl;

            std::cout << "New size: " << sizeof(int) * 2 << std::endl;

            // 验证数据是否一致
            std::cout << "Data in resized block: ";
            for (int i = 0; i < sizeof(int) * 2 / sizeof(int); i++) {
                std::cout << resizedPtr[i] << " ";
            }
            std::cout << std::endl;

            free(resizedPtr); // 释放重新分配的内存块
        }
    }

    // 测试 calloc 函数
    int *ptr4 = (int *)calloc(6, sizeof(int));
    if (ptr4 != nullptr)
    {
        for (int i = 0; i < 3; i++)
        {
            std::cout << "calloc: " << ptr4[i] << std::endl; // 应该输出 3 个 0
        }
        free(ptr4);
    }

    return 0;
}