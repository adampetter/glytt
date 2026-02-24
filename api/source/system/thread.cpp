#include "api/system/thread.h"
#include "api/common/time.h"

Mutex::Mutex()
{
    this->mutex = xSemaphoreCreateMutex();
}

Mutex::~Mutex()
{
}

bool Mutex::Take(unsigned int timeout, bool wait)
{
    if (wait)
    {
        while (!xSemaphoreTake(this->mutex, msToTicks(timeout)))
            delayTicks(1);

        return true;
    }
    else
        return xSemaphoreTake(this->mutex, msToTicks(timeout));
}

void Mutex::Release()
{
    xSemaphoreGive(this->mutex);
}

Threadable::Threadable(Byte rate)
    : Loop(rate)
{
    this->enabled = false;
    this->threaded = false;
}

Threadable::~Threadable()
{
}

void Threadable::execute(void *parameters)
{
    Threadable *t = (Threadable *)parameters;
    ((Loop*)t)->Run();
}

void Threadable::Run(ThreadCore pinCore, unsigned int stackSizeBytes)
{
    if (this->enabled)
        return;

    this->threaded = true;

    if (pinCore == ThreadCore::ThreadCore_Undefined)
        xTaskCreate(Threadable::execute, "Threadable", stackSizeBytes, this, 1, NULL);
    else
        xTaskCreatePinnedToCore(Threadable::execute, "Threadable", stackSizeBytes, this, 1, NULL, pinCore);
}

void Threadable::Terminate()
{
    Loop::Terminate();
    this->threaded = false;
}

bool Threadable::Threaded()
{
    return this->threaded;
}

void Threadable::Lock()
{
    throw "Work in progress. Mutex not implemented";
}

void Threadable::Unlock()
{
    throw "Work in progress. Mutex not implemented";
}

void Threadable::Debug()
{
    UBaseType_t stackHighWaterMarkWords = uxTaskGetStackHighWaterMark(NULL);
    size_t stackHighWaterMarkBytes = stackHighWaterMarkWords * sizeof(StackType_t);
    printf("Threadable stack high watermark: %i bytes, (%iKb)\n", stackHighWaterMarkBytes, stackHighWaterMarkBytes / 1024);
}