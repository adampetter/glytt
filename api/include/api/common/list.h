#pragma once
#include <cstring>
#include <ctime>
#include <stdio.h> //printf
#include "api/memory/memorypool.h"

template <typename T>
class List
{
protected:
    MemoryPool<T> *pool = NULL;
    T *data = 0;
    int size = 0;
    int count = 0;
    int step = 0;

    bool resize(int step)
    {
        if (step <= 0 && this->count > this->size + step)
            return false;

        int newSize = this->size + step;
        T *newdata = NULL;

        if (this->pool)
            newdata = this->pool->Allocate(newSize);
        else
            newdata = new T[newSize];

        memset((void *)newdata, 0, sizeof(T) * newSize);

        if (this->data)
        {
            memcpy((void *)newdata, (void *)this->data, sizeof(T) * this->count);

            if (this->pool)
                this->pool->Deallocate(this->data);
            else
                delete[] this->data;
        }

        this->size = newSize;
        this->data = newdata;
        return true;
    }

public:
    List(unsigned int initialdataSize = 2, unsigned int step = 10, MemoryPool<T> *pool = NULL)
    {
        this->pool = pool;
        this->size = initialdataSize;
        this->count = 0;
        this->step = step;

        if (this->size)
        {
            if (this->pool)
                this->data = this->pool->Allocate(initialdataSize);
            else
                this->data = new T[initialdataSize];
        }
        else
            this->data = NULL;
    }

    ~List()
    {
        if (this->data)
        {
            if (this->pool)
                this->pool->Deallocate(this->data);
            else
                delete[] this->data;
        }
    }

    void Add(const T &item)
    {
        if (this->count >= this->size)
            if (!this->resize(this->step))
                return;

        this->data[this->count++] = item;
    }

    void Add(T *item, unsigned int length)
    {
        /*if (this->count >= this->size || (this->count + length) >= size)
            if (!this->resize(this->step))
                return;*/

        for (int i = 0; i < length; i++) // TODO: Use memcpy instead
            this->Add(item[i]);
    }

    void RemoveAt(int index)
    {
        if (!this->data)
            return;

        if (this->count <= this->size - this->step)
            this->resize(this->step * -1);

        if (index + 1 > this->size)
            return;
        else if (index < this->count - 1)
            memcpy((void *)(this->data + index), (void *)(this->data + index + 1), sizeof(T) * (this->count - index - 1));

        count--;
    }

    void Remove(T value)
    {
        int index = this->IndexOf(value);

        if (index >= 0)
            this->RemoveAt(index);
    }

    int Count()
    {
        return this->count;
    }

    int Size(){
        return this->size;
    }

    void Clear(int size = 2)
    {
        this->count = 0;
        this->size = size;

        if (this->pool)
        {
            if (this->data)
                this->pool->Deallocate(this->data);

            this->data = this->pool->Allocate(this->size);
        }
        else
        {
            if (this->data)
                delete[] this->data;

            this->data = new T[this->size];
        }
    }

    void Reset()
    {
        this->count = 0;
    }

    T &At(int i)
    {
        if(!this->data)
            throw "Exception: Out of index";

        return data[i];
    }

    T *Ptr(int i = 0)
    {
        if (i >= this->count)
            return NULL;
        else
            return this->data + i;
    }

    T &operator[](int i)
    {
        if(!this->data)
            throw "Exception: Out of index";

        return data[i];
    }

    void Shuffle()
    {
        T t;
        int r;
        srand(time(0));

        for (int i = 0; i < this->count; i++)
        {
            r = rand() % this->count;
            t = this->data[r];
            this->data[r] = this->data[i];
            this->data[i] = t;
        }
    }

    bool Exists(T value)
    {
        if(!this->data)
            return false;

        for (int i = 0; i < this->count; i++)
            if (this->data[i] == value)
                return true;

        return false;
    }

    int IndexOf(T value)
    {
        if(!this->data)
            return -1;

        for (int i = 0; i < this->count; i++)
            if (this->data[i] == value)
                return i;

        return -1;
    }

    bool Contains(T value)
    {
        return this->IndexOf(value) >= 0;
    }

    int Middle()
    {
        if (!this->count)
            return -1;

        return ((this->count * 10 / 2) + 5) / 10 - 1;
    }
};