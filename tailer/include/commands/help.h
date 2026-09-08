#pragma once

#include "commands/command.h"
#include "api/common/list.h"

class HelpCommand : public Command
{
private:
    List<const char*>* command_names;
    List<Command*>* command_objects;

public:
    HelpCommand(List<const char*>* names, List<Command*>* objects);
    const char* Execute(const char* args) override;
    const char* GetDescription() const override;
};