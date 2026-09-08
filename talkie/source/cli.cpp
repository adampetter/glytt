#include "cli.h"

#include <vector>

TalkieCLI::TalkieCLI(const TalkieCliConfig &config)
    : Loop(config.rate), config(config)
{
    this->Register("help", "List available commands", [this](const std::string &args) {
        (void)args;
        return this->Help();
    });
}

TalkieCLI::~TalkieCLI()
{
}

void TalkieCLI::Execute(const FrameTime &time)
{
    (void)time;
}

bool TalkieCLI::Register(const std::string &name, const std::string &description, CliHandler handler)
{
    return this->cli.Register(name, description, handler);
}

bool TalkieCLI::Unregister(const std::string &name)
{
    return this->cli.Unregister(name);
}

std::string TalkieCLI::Dispatch(const std::string &input) const
{
    return this->cli.Dispatch(input);
}

std::string TalkieCLI::Help() const
{
    std::string response = "Available commands:";
    std::vector<CliCommandInfo> commands = this->cli.List();

    for (size_t i = 0; i < commands.size(); i++)
    {
        response += "\n- " + commands[i].name;
        if (!commands[i].description.empty())
            response += " : " + commands[i].description;
    }

    return response;
}
