#pragma once

#include <string>

#include "api/system/cli.h"
#include "api/system/loop.h"

struct TalkieCliConfig
{
    Byte rate = 0;
};

class TalkieCLI : public Loop
{
private:
    TalkieCliConfig config;
    Cli cli;

public:
    TalkieCLI(const TalkieCliConfig &config);
    ~TalkieCLI();

    void Execute(const FrameTime &time);
    bool Register(const std::string &name, const std::string &description, CliHandler handler);
    bool Unregister(const std::string &name);
    std::string Dispatch(const std::string &input) const;
    std::string Help() const;
};
