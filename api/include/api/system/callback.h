#pragma once
#include "api/common/datetime.h"

class Callback;

struct CallbackArgs
{
    Callback *sender = NULL;
    DateTime time;
    int flags = 0;
};

class Callback
{
public:
    virtual void OnCallback(const CallbackArgs &args) = 0;
};

class CallbackHandler
{
protected:
    Byte last;
    List<DateTime> times;
    List<Callback *> callbacks;
    List<int> flags;
    CallbackArgs args;

public:
    CallbackHandler()
    {
        this->last = DateTime::Now().Second();
    }

    ~CallbackHandler()
    {
    }

    void Add(Callback *callback, DateTime time, int flag = 0)
    {
        this->callbacks.Add(callback);
        this->times.Add(time);
        this->flags.Add(flag);
        printf("Added callback!\n");
    }

    void Remove(Callback *callback)
    {
        for (int i = 0; i < this->callbacks.Count(); i++)
        {
            if (this->callbacks[i] == callback)
            {
                this->callbacks.RemoveAt(i);
                this->times.RemoveAt(i);
                this->flags.RemoveAt(i);
                break;
            }
        }
    }

    void Invoke(const DateTime &now)
    {
        //Only invoke if has callbacks and the time second has changed
        if (!this->callbacks.Count() || now.Second() == this->last)
            return;

        this->last = now.Second();

        for (unsigned short i = 0; i < this->callbacks.Count();)
        {
            //printf("Now sec: %i    Times[i] sec: %i\n", now.Second(), times[i].Second());
            if (now >= times[i])
            {   
                this->args.time = now;
                this->args.sender = this->callbacks[i];
                this->args.flags = this->flags[i];
                this->callbacks[i]->OnCallback(this->args);
                this->Remove(this->callbacks[i]);
            }else{
                ++i;
            }
        }
    }
};