#pragma once

#include <functional>
#include <string>

#include "api/system/cli.h"
#include "api/system/loop.h"

struct CliConfig{
   Byte rate = 0; // 0 for no loop, otherwise loop with specified rate in max fps
};

class CLI : public Loop
{
private:
    CliConfig config;
    Cli cli;

public:
    CLI(const CliConfig& config);
    ~CLI();

    void Execute(const FrameTime &time);
    bool Register(const std::string &name, const std::string &description, CliHandler handler);
    bool Unregister(const std::string &name);
    std::string Dispatch(const std::string &input) const;
    std::string Help() const;

};