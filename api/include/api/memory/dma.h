#pragma once
#include "api/common/types.h"
#include "esp_heap_caps.h"

class DMA final
{
public:
    static Byte *Malloc(unsigned int size)
    {
        return (Byte *)heap_caps_malloc(size, MALLOC_CAP_DMA);
    };
    
    static void Free(Byte *ptr)
    {
        heap_caps_free(ptr);
    };
};