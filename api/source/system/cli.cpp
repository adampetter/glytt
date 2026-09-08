#include "api/system/cli.h"

#include <algorithm>

void Cli::splitInput(const std::string &input, std::string *command, std::string *args)
{
    if (command == nullptr || args == nullptr)
        return;

    command->clear();
    args->clear();

    size_t start = input.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return;

    size_t separator = input.find_first_of(" \t\r\n", start);
    if (separator == std::string::npos)
    {
        *command = input.substr(start);
        return;
    }

    *command = input.substr(start, separator - start);

    size_t argsStart = input.find_first_not_of(" \t\r\n", separator);
    if (argsStart != std::string::npos)
        *args = input.substr(argsStart);
}

bool Cli::Register(const std::string &name, const std::string &description, CliHandler handler)
{
    if (name.empty() || !handler)
        return false;

    this->commands[name] = RegisteredCommand{
        .description = description,
        .handler = handler};

    return true;
}

bool Cli::Unregister(const std::string &name)
{
    return this->commands.erase(name) > 0;
}

bool Cli::Contains(const std::string &name) const
{
    return this->commands.find(name) != this->commands.end();
}

void Cli::Clear()
{
    this->commands.clear();
}

std::string Cli::Dispatch(const std::string &input) const
{
    std::string command;
    std::string args;
    splitInput(input, &command, &args);

    if (command.empty())
        return "";

    auto it = this->commands.find(command);
    if (it == this->commands.end())
        return "ERR: unknown command";

    return it->second.handler(args);
}

std::vector<CliCommandInfo> Cli::List() const
{
    std::vector<CliCommandInfo> items;
    items.reserve(this->commands.size());

    for (const auto &pair : this->commands)
    {
        items.push_back(CliCommandInfo{
            .name = pair.first,
            .description = pair.second.description});
    }

    std::sort(items.begin(), items.end(), [](const CliCommandInfo &a, const CliCommandInfo &b) {
        return a.name < b.name;
    });

    return items;
}
