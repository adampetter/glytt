#include "commands/help_command.h"
#include <cstdio>

HelpCommand::HelpCommand(List<const char*>* names, List<Command*>* objects) : command_names(names), command_objects(objects) {}

const char* HelpCommand::Execute(const char* args) {
    static char buffer[1024];
    snprintf(buffer, sizeof(buffer), "Available commands:\n");
    for (int i = 0; i < command_names->Count() && i < command_objects->Count(); ++i) {
        char temp[256];
        snprintf(temp, sizeof(temp), "%s - %s\n", (*command_names)[i], (*command_objects)[i]->GetDescription());
        strncat(buffer, temp, sizeof(buffer) - strlen(buffer) - 1);
    }
    return buffer;
}

const char* HelpCommand::GetDescription() const {
    return "Show available commands";
}