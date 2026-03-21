#pragma once

#include <Arduino.h>
#include <AsyncTCP.h>
#include "AsyncTelnetServer.h"

#include "Globals.h"

class Telnet {
    public:
        static void Begin();
    private:
        static void registerCommand_dumpcfg();
        static void registerCommand_logon();
        static void registerCommand_reboot();
        static void registerCommand_network();
        static void registerCommand_ntp();
        static void registerCommand_ver();
        static void registerCommand_memory();
        static void registerCommand_storage();
        static void registerCommand_ping();
        static void registerCommand_telnet();
        static void registerCommand_webserver();
        static void registerCommand_mqtt();
        static void registerCommand_comp();
        static void registerCommand_user();
};