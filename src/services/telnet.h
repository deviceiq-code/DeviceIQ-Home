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
        static void registerCommand_ntp();
        static void registerCommand_ver();
        static void registerCommand_memory();
        static void registerCommand_ping();
};