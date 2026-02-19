#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class Mutex
{
private:
    SemaphoreHandle_t handle;

public:
    Mutex()
    {
        handle = xSemaphoreCreateMutex();
    }

    ~Mutex()
    {
        if (handle != nullptr)
            vSemaphoreDelete(handle);
    }

    bool Take(unsigned int timeout = 0, bool wait = true)
    {
        if (handle == nullptr)
            return false;

        TickType_t ticks = wait ? pdMS_TO_TICKS(timeout) : 0;
        return xSemaphoreTake(handle, ticks) == pdTRUE;
    }

    void Release()
    {
        if (handle != nullptr)
            xSemaphoreGive(handle);
    }
};
