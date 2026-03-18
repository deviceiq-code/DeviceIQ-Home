#pragma once

#include <Arduino.h>
#include <AsyncTCP.h>
#include "AsyncTelnetServer.h"

#include "Globals.h"

class Telnet {
    public:
        static void Begin();
    private:
        static void registerCommand_network();
};