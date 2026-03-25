#pragma once

#include <Arduino.h>
#include <AsyncTCP.h>
#include "AsyncTelnetServer.h"

#include "Globals.h"

class Telnet {
    public:
        static void Begin();
    private:
        static void registerCommand_dumpcfg(bool admincmd = true);
        static void registerCommand_logon(bool admincmd = false);
        static void registerCommand_reboot(bool admincmd = true);
        static void registerCommand_network(bool admincmd = true);
        static void registerCommand_ntp(bool admincmd = true);
        static void registerCommand_ver(bool admincmd = false);
        static void registerCommand_memory(bool admincmd = false);
        static void registerCommand_storage(bool admincmd = false);
        static void registerCommand_ping(bool admincmd = true);
        static void registerCommand_telnet(bool admincmd = true);
        static void registerCommand_webserver(bool admincmd = true);
        static void registerCommand_mqtt(bool admincmd = true);
        static void registerCommand_user(bool admincmd = true);
        static void registerCommand_comp(bool admincmd = true);
        static void registerCommand_log(bool admincmd = true);
};