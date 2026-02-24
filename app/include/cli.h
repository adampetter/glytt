#pragma once

#include <functional>
#include "api/common/dictionary.h"
#include "api/common/list.h"
#include "commands/command.h"
#include "api/system/loop.h"

struct CliConfig{
   Byte rate = 0; // 0 for no loop, otherwise loop with specified rate in max fps
};

class CLI : public Loop
{
private:
    CliConfig config;

public:
    CLI(const CliConfig& config);
    ~CLI();

    void Execute(const FrameTime &time);

    //void Register(Command* command);
    //void Unregister(Command* command);
};