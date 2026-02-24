#include "api/system/loop.h"
#include <stdio.h>

Loop::Loop(Byte rate)
{
    this->enabled = false;
    this->rate = rate;                                     // fps
    this->target_ms = rate > 0 ? MS_PER_SECOND / rate : 0; // in milliseconds
    this->target_sec = (float)this->target_ms / MS_PER_SECOND;
    this->duration = 0;
    this->elapsed = 0;
}

Loop::~Loop()
{
}

void Loop::Run()
{
    this->enabled = true;
    this->frametime.now = DateTime::Now();
    this->frametime.elapsed = 0;
    this->duration = 0;
    this->elapsed = 0;

    while (this->enabled)
    {
        // Loop with framerate
        if (this->target_ms)
        {
            if (this->elapsed >= MS_PER_SECOND)
            {
                this->frametime.now = DateTime::Now();
                this->elapsed = 0;
            }

            this->timer.Start();
            this->Execute(this->frametime);
            this->timer.Stop();

            this->duration = this->timer.Duration();

            if (this->duration < this->target_ms)
            {
                this->frametime.elapsedMs = this->target_ms;
                this->frametime.elapsed = this->target_sec;
                this->elapsed += this->target_ms;
                Byte wait = this->target_ms - this->duration;
                delay(wait);
            }
            else
            {
                this->frametime.elapsedMs = this->duration;
                this->frametime.elapsed = (float)this->duration / MS_PER_SECOND; // Convert milliseconds to seconds
                this->elapsed += this->duration;
            }
        }
        else
            // Loop as fast as possible
            this->Execute(this->frametime); // frametime.now update not implemented for targetless updates
    }
}

void Loop::Terminate()
{
    this->enabled = false;
}

unsigned int Loop::Target()
{
    return this->target_ms;
}

Byte Loop::Rate()
{
    return this->rate;
}

void Loop::Debug()
{
    printf("--- Loop ---\n");
    printf("Target: %lims / %fsec\n", this->target_ms, this->target_sec);
    printf("Rate: %ifps\n", this->rate);
    printf("------------\n");
}