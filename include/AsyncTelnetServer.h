#ifndef AsyncTelnetServer_h
#define AsyncTelnetServer_h

#pragma once

#include <Arduino.h>
#include <AsyncTCP.h>
#include <vector>
#include <functional>

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

class AsyncTelnetSession;

typedef std::function<void(AsyncClient* client)> telnet_callback_t;
typedef std::function<void(AsyncClient* client, AsyncTelnetSession* session)> telnet_session_callback_t;

enum AsyncTelnetMode {
    ASYNCTELNETMODE_CHAR,
    ASYNCTELNETMODE_LINE
};

static struct protcmd {
    const uint8_t SE   = 240;
    const uint8_t NOP  = 241;
    const uint8_t DM   = 242;
    const uint8_t BRK  = 243;
    const uint8_t IP   = 244;
    const uint8_t AO   = 245;
    const uint8_t AYT  = 246;
    const uint8_t EC   = 247;
    const uint8_t EL   = 248;
    const uint8_t GA   = 249;
    const uint8_t SB   = 250;
    const uint8_t WILL = 251;
    const uint8_t WONT = 252;
    const uint8_t DO   = 253;
    const uint8_t DONT = 254;
    const uint8_t IAC  = 255;
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
        String InputBuffer;
        String LastInput;
};

class AsyncTelnetServer {
    private:
        AsyncServer* mAsyncServer = nullptr;
        AsyncTelnetMode mMode = ASYNCTELNETMODE_LINE;
        uint16_t mPort;

        std::vector<AsyncTelnetCommand*> mCommandList;
        std::vector<AsyncTelnetSession*> mSessions;
        std::vector<String> mUsers;

        AsyncTelnetSession* FindSession(AsyncClient* client);
        void RemoveSession(AsyncClient* client);
        void HandleNegotiation(AsyncClient* client, const uint8_t* data, size_t len, size_t& index);
        void ProcessLine(AsyncClient* client, const String& line);

    public:
        AsyncTelnetServer(uint16_t port);
        ~AsyncTelnetServer();

        String Prompt = ASYNCTELNETSERVER_DEFAULTPROMPT;
        String WelcomeMessage = ASYNCTELNETSERVER_DEFAULTWELCOMEMESSAGE;

        telnet_session_callback_t onSessionBegin = nullptr;
        telnet_session_callback_t onSessionEnd = nullptr;

        inline uint16_t Port() { return mPort; }

        AsyncTelnetSession* CurrentSession(AsyncClient* client);
        uint8_t SessionID(AsyncClient* client);

        inline void onCommand(String command, String helpmessage, telnet_callback_t callback, bool admincommand = false) {
            mCommandList.push_back(new AsyncTelnetCommand({command, helpmessage, callback, admincommand}));
        }

        inline void begin() {
            if (mAsyncServer != nullptr) mAsyncServer->begin();
        }

        inline void end() {
            if (mAsyncServer != nullptr) mAsyncServer->end();
        }
};

#endif