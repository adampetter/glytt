#pragma once
#include "api/common/types.h"
#include <stdlib.h>
#include <time.h>


#define MS_PER_SECOND 1000
#define TICK_PER_MS portTICK_PERIOD_MS

long int millis(void);
void delay(int ms);
void delayTicks(int ticks);
int msToTicks(int ms);
void standStill(void);
float randomize();
int randomize(int max);

class Timer{
    protected:
        long int start;
        long int end;
        long int duration;

    public:
        Timer();
        ~Timer();

        void Start();
        void Stop();
        void Reset();
        long int Duration();
};