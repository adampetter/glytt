#pragma once
#include <iostream>
#include <vector>

template <typename T>
class MemoryPool
{
protected:
    T *pool;               // Memory pool
    unsigned int size;     // Size of memory pool
    std::vector<T *> free; // List of free blocks

public:
    // Constructor
    MemoryPool(unsigned int size)
        : size(size), pool(new T[size]) // Allocate memory for the pool
    {
        free.reserve(size);

        // Populate the free list with pointers to each object in the pool
        for (size_t i = 0; i < size; ++i)
            free.push_back(pool + i);
    }

    // Destructor
    ~MemoryPool()
    {
        // Call destructors for active objects
        for (size_t i = 0; i < size; ++i)
            (pool + i)->~T(); // Explicitly call destructor

        // Free the memory for the pool
        delete[] pool;
    }

    // Allocate an object from the pool
    T *Allocate()
    {
        if (free.empty())
        {
#if DEBUG
            printf("Memory pool exhausted!\n");
#endif
            return NULL;
        }

        // Get a free block from the free list
        T *freeBlock = free.back();
        free.pop_back(); // Remove it from the free list
        return freeBlock;
    }

    T *Allocate(unsigned int size)
    {
        if (free.size() < size)
        {
#if DEBUG
            printf("Not enough memory in pool to allocate block of size %u\n", size);
#endif
            return NULL;
        }
        else if (!size)
        {
            return NULL;
        }

        T *block = free.back();          // Start block from last free adress
        free.resize(free.size() - size); // Remove block size from free list of elements
        return block;
    }

    // Return an object back to the pool
    void Deallocate(T *ptr)
    {
        free.push_back(ptr); // Add it back to the free list
    }

    // Returns the total number of slots in the pool
    unsigned int Size() const
    {
        return size;
    }

    // Returns the number of currently used slots
    unsigned int Count() const
    {
        return size - free.size(); // Used slots are total slots minus free slots
    }
};
