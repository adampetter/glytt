#include "api/common/time.h"
#include "esp_timer.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/time.h>

long int millis()
{
    return esp_timer_get_time() / 1000;
}

void delay(int ms)
{
    if (ms <= 0)
        return;

    vTaskDelay(pdMS_TO_TICKS(ms));
}

void delayTicks(int ticks)
{
    if (ticks <= 0)
        return;

    vTaskDelay(ticks);
}

int msToTicks(int ms)
{
    return pdMS_TO_TICKS(ms);
}

void standStill()
{
    while (true)
        delay(1000);
}

float randomize()
{
    return (float)rand() / (RAND_MAX);
}

int randomize(int max)
{
    return rand() % max;
}

Timer::Timer(){
    this->start = 0;
    this->end = 0;
    this->duration = 0;
}

Timer::~Timer(){

}

void Timer::Start(){
    this->start = millis();
    this->duration = 0;
}

void Timer::Stop(){
    this->end = millis();
    this->duration = this->end - this->start;
}

void Timer::Reset(){
    this->duration = 0;
}

long int Timer::Duration(){
    return this->duration;
}