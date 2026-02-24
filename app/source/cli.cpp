#include "app/cli.h"

CLI::CLI(const CliConfig& config) : Loop(0), config(config) {

}

CLI::~CLI()
{
}

void CLI::Execute(const FrameTime &time)
{
}