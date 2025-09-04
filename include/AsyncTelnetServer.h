#ifndef AsyncTelnetServer_h
#define AsyncTelnetServer_h

#pragma once

#include <Arduino.h>
#include <AsyncTCP.h>
#include <vector>

#define ASYNCTELNETSERVER_MAXCOMMANDPARAMETERS  10
#define ASYNCTELNETSERVER_DEFAULTPROMPT         "> "
#define ASYNCTELNETSERVER_CMD_CLEAR             "clear"
#define ASYNCTELNETSERVER_HLP_CLEAR             "Clear terminal screen\r\n\r\nclear"
#define ASYNCTELNETSERVER_CMD_EXIT              "exit"
#define ASYNCTELNETSERVER_HLP_EXIT              "Close session and exit terminal\r\n\r\nexit"
#define ASYNCTELNETSERVER_CMD_SESSIONS          "sessions"
#define ASYNCTELNETSERVER_HLP_SESSIONS          "Show current Telnet sessions\r\n\r\nsessions"
#define ASYNCTELNETSERVER_CMD_WHOAMI            "whoami"
#define ASYNCTELNETSERVER_HLP_WHOAMI            "Show information about current user\r\n\r\nwhoami"
#define ASYNCTELNETSERVER_DEFAULTWELCOMEMESSAGE "Async Telnet Server - Welcome"
#define ASYNCTELNETSERVER_INVALIDCOMMAND        "Invalid command."
#define ASYNCTELNETSERVER_GUESTUSER             "guest"

typedef std::function<void(AsyncClient* client)> telnet_callback_t;

enum AsyncTelnetMode { ASYNCTELNETMODE_CHAR, ASYNCTELNETMODE_LINE };


static struct protcmd {
    const uint8_t SE = 240;                     // Subnegotiation End
    const uint8_t NOP = 241;                    // No Operation
    const uint8_t DM = 242;                     // Data Mark
    const uint8_t BRK = 243;                    // Break
    const uint8_t IP = 244;                     // Interrupt Process
    const uint8_t AO = 245;                     // Abort Output
    const uint8_t AYT = 246;                    // Are You There
    const uint8_t EC = 247;                     // Erase Character
    const uint8_t EL = 248;                     // Erase Line
    const uint8_t GA = 249;                     // Go Ahead
    const uint8_t SB = 250;                     // Subnegotiation Begin
    const uint8_t WILL = 251;                   // Will
    const uint8_t WONT = 252;                   // Won't
    const uint8_t DO = 253;                     // Do
    const uint8_t DONT = 254;                   // Don't
    const uint8_t IAC = 255;                    // Interpret As Command
} PROTCMD;

class AsyncTelnetCommand {
    public:
        String Command;
        String HelpMessage;
        telnet_callback_t Callback;
        bool AdminCommand;
};

class AsyncTelnetSession {
    public:
        IPAddress RemoteIP;
        uint16_t RemotePort;
        String User;
};

class AsyncTelnetServer {
    private:
        AsyncServer* mAsyncServer;

        AsyncTelnetMode mMode = ASYNCTELNETMODE_LINE;
        
        uint16_t mPort;
        String mIncomeData = "";
        String mLastIncomeData;
        String mCommand;
        String* mParameter = new String[ASYNCTELNETSERVER_MAXCOMMANDPARAMETERS];
        
        std::vector<AsyncTelnetCommand*> mCommandList;
        std::vector<AsyncTelnetSession*> mSessions;
        std::vector<String> mUsers;
    
    public:
        AsyncTelnetServer(uint16_t port);
        ~AsyncTelnetServer() {}
        
        inline uint16_t Port() { return mPort; }
        
        String Prompt = ASYNCTELNETSERVER_DEFAULTPROMPT;
        String WelcomeMessage = ASYNCTELNETSERVER_DEFAULTWELCOMEMESSAGE;
        
        inline AsyncTelnetSession* CurrentSession(AsyncClient* client) { return mSessions[SessionID(client) - 1]; }
        uint8_t SessionID(AsyncClient* client);
        inline void onCommand(String command, String helpmessage, telnet_callback_t callback, bool admincommand = false) { mCommandList.push_back(new AsyncTelnetCommand({command, helpmessage, callback, admincommand})); };

        inline void begin() { if (mAsyncServer != nullptr) mAsyncServer->begin(); }
        inline void end() { if (mAsyncServer != nullptr) mAsyncServer->end(); }
};

#endif