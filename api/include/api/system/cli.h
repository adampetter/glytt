#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

using CliHandler = std::function<std::string(const std::string &args)>;

struct CliCommandInfo
{
    std::string name;
    std::string description;
};

class GenericCli
{
private:
    struct RegisteredCommand
    {
        std::string description;
        CliHandler handler;
    };

    std::unordered_map<std::string, RegisteredCommand> commands;

    static void splitInput(const std::string &input, std::string *command, std::string *args);

public:
    GenericCli() = default;

    bool Register(const std::string &name, const std::string &description, CliHandler handler);
    bool Unregister(const std::string &name);
    bool Contains(const std::string &name) const;
    void Clear();

    std::string Dispatch(const std::string &input) const;
    std::vector<CliCommandInfo> List() const;
};
