#pragma once

class Command
{
public:
    Command() {}
    ~Command() {}

    virtual const char* Execute(const char* args) = 0;
    virtual const char* Description() const = 0;
};
