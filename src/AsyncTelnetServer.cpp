#include "AsyncTelnetServer.h"

AsyncTelnetServer::AsyncTelnetServer(uint16_t port) : mPort(port) {
    onCommand(ASYNCTELNETSERVER_CMD_CLEAR, ASYNCTELNETSERVER_HLP_CLEAR, [&](AsyncClient* client, String* parameter) {
        client->write("\x1B[2J\x1B[H");
    });

    onCommand(ASYNCTELNETSERVER_CMD_EXIT, ASYNCTELNETSERVER_HLP_EXIT, [&](AsyncClient* client, String* parameter) {
        client->write("Session closed.\r\n\r\n");
        RemoveSession(client);
        client->stop();
    });

    onCommand(ASYNCTELNETSERVER_CMD_SESSIONS, ASYNCTELNETSERVER_HLP_SESSIONS, [&](AsyncClient* client, String* parameter) {
        client->write("Current sessions:\r\n\r\n");
        for (AsyncTelnetSession* session : mSessions) {
            client->write(String(session->User + "@" + session->RemoteIP.toString() + ":" + String(session->RemotePort) + "\r\n").c_str());
        }
    });

    onCommand(ASYNCTELNETSERVER_CMD_WHOAMI, ASYNCTELNETSERVER_HLP_WHOAMI, [&](AsyncClient* client, String* parameter) {
        AsyncTelnetSession* session = CurrentSession(client);
        if (session != nullptr) {
            client->write(String(session->User + "@" + client->remoteIP().toString() + ":" + String(client->remotePort()) + "\r\n").c_str());
        } else {
            client->write("Unknown session\r\n");
        }
    });

    onCommand(ASYNCTELNETSERVER_CMD_HELP, ASYNCTELNETSERVER_HLP_HELP, [&](AsyncClient* client, String* parameter) {
        if (parameter[0].isEmpty()) {
            client->write(String("Available commands:\r\n\r\n").c_str());

            String out;
            size_t maxLen = 0;
            for (AsyncTelnetCommand* cmd : mCommandList) if (cmd->Command.length() > maxLen) maxLen = cmd->Command.length();

            size_t colWidth = maxLen + 4;
            uint16_t colCount = ASYNCTELNETSERVER_HELPCOMMANDSPERLINE;
            uint16_t currentCol = 0;

            for (AsyncTelnetCommand* cmd : mCommandList) {
                out = cmd->Command;

                while (out.length() < colWidth) out += ' ';
                client->write(out.c_str());

                currentCol++;

                if (currentCol >= colCount) {
                    client->write("\r\n");
                    currentCol = 0;
                }
            }

            if (currentCol != 0) client->write("\r\n");
            
            out = "\r\n\r\nUse . to repeat last command.\r\n";
            client->write(out.c_str());
        } else {
            bool ValidCommand = false;
            for (AsyncTelnetCommand* cmd : mCommandList) {
                if (parameter[0] == cmd->Command) {
                    client->write(String(cmd->HelpMessage + "\r\n").c_str());
                    ValidCommand = true;
                    break;
                }
            }

            if (!ValidCommand) client->write(String(parameter[0] + " - Invalid command.\r\n").c_str());
        }
    });

    onCommand("echo", "Echo text\r\n\r\necho text", [&](AsyncClient* client, String* parameter) {
        String Text;
        for (uint8_t n = 0; n < ASYNCTELNETSERVER_MAXCOMMANDPARAMETERS; n++)
            if (!parameter[n].isEmpty()) Text += (Text.isEmpty() ? "" : " ") + parameter[n];

        client->write(String(Text + "\r\n").c_str());
    });

    mAsyncServer = new AsyncServer(mPort);

    mAsyncServer->onClient([&](void* arg, AsyncClient* client) {
        AsyncTelnetSession* session = new AsyncTelnetSession{
            client->remoteIP(),
            client->remotePort(),
            ASYNCTELNETSERVER_GUESTUSER,
            "",
            ""
        };

        mSessions.push_back(session);

        if (onSessionBegin != nullptr) {
            onSessionBegin(client, session);
        }

        client->write(String("\r\n" + WelcomeMessage + "\r\n\r\n" + ASYNCTELNETSERVER_GUESTUSER + String(" ") + ASYNCTELNETSERVER_DEFAULTPROMPT).c_str());

        client->onData([&](void* arg, AsyncClient* client, void* data, size_t len) {
            AsyncTelnetSession* session = FindSession(client);
            if (session == nullptr || data == nullptr || len == 0) return;

            const uint8_t* bytes = static_cast<const uint8_t*>(data);

            for (size_t i = 0; i < len; i++) {
                uint8_t b = bytes[i];

                if (b == PROTCMD.IAC) {
                    HandleNegotiation(client, bytes, len, i);
                    continue;
                }

                if (b == 8 || b == 127) {
                    if (session->InputBuffer.length() > 0) {
                        session->InputBuffer.remove(session->InputBuffer.length() - 1);
                    }
                    continue;
                }

                if (b == '\r' || b == '\n') {
                    if (session->InputBuffer.length() > 0) {
                        client->write("\r\n");
                        String line = session->InputBuffer;
                        session->InputBuffer.clear();
                        ProcessLine(client, line);
                    } else {
                        client->write("\r\n");
                        client->write(String(session->User + String(" ") + ASYNCTELNETSERVER_DEFAULTPROMPT).c_str());
                    }

                    if (b == '\r' && (i + 1 < len) && bytes[i + 1] == '\n') {
                        i++;
                    }

                    continue;
                }

                if (b >= 32 && b <= 126) {
                    session->InputBuffer += static_cast<char>(b);
                }
            }
        }, nullptr);

        client->onDisconnect([&](void* arg, AsyncClient* client) {
            RemoveSession(client);
        }, nullptr);

        client->onError([&](void* arg, AsyncClient* client, int8_t error) {
            RemoveSession(client);
        }, nullptr);

        client->onTimeout([&](void* arg, AsyncClient* client, uint32_t time) {
            RemoveSession(client);
            client->stop();
        }, nullptr);
    }, nullptr);
}

AsyncTelnetServer::~AsyncTelnetServer() {
    if (mAsyncServer != nullptr) {
        mAsyncServer->end();
        delete mAsyncServer;
        mAsyncServer = nullptr;
    }

    for (AsyncTelnetCommand* cmd : mCommandList) {
        delete cmd;
    }
    mCommandList.clear();

    for (AsyncTelnetSession* session : mSessions) {
        delete session;
    }
    mSessions.clear();
}

AsyncTelnetSession* AsyncTelnetServer::FindSession(AsyncClient* client) {
    if (client == nullptr) return nullptr;

    for (AsyncTelnetSession* session : mSessions) {
        if ((session->RemoteIP == client->remoteIP()) &&
            (session->RemotePort == client->remotePort())) {
            return session;
        }
    }

    return nullptr;
}

AsyncTelnetSession* AsyncTelnetServer::CurrentSession(AsyncClient* client) {
    return FindSession(client);
}

void AsyncTelnetServer::RemoveSession(AsyncClient* client) {
    if (client == nullptr) return;

    for (auto it = mSessions.begin(); it != mSessions.end(); ++it) {
        AsyncTelnetSession* session = *it;

        if ((session->RemoteIP == client->remoteIP()) &&
            (session->RemotePort == client->remotePort())) {

            if (onSessionEnd != nullptr) {
                onSessionEnd(client, session);
            }

            delete session;
            mSessions.erase(it);
            break;
        }
    }
}

void AsyncTelnetServer::HandleNegotiation(AsyncClient* client, const uint8_t* data, size_t len, size_t& index) {
    if (client == nullptr || data == nullptr) return;
    if (index + 1 >= len) return;

    uint8_t cmd = data[index + 1];

    if (cmd == PROTCMD.IAC) {
        index += 1;
        return;
    }

    if (cmd == PROTCMD.DO || cmd == PROTCMD.DONT || cmd == PROTCMD.WILL || cmd == PROTCMD.WONT) {
        if (index + 2 >= len) return;

        uint8_t opt = data[index + 2];
        uint8_t reply[3] = { PROTCMD.IAC, 0, opt };

        if (cmd == PROTCMD.WILL) {
            reply[1] = PROTCMD.DONT;
            client->write(reinterpret_cast<const char*>(reply), 3);
        } else if (cmd == PROTCMD.DO) {
            reply[1] = PROTCMD.WONT;
            client->write(reinterpret_cast<const char*>(reply), 3);
        }

        index += 2;
        return;
    }

    if (cmd == PROTCMD.SB) {
        size_t j = index + 2;
        while (j + 1 < len) {
            if (data[j] == PROTCMD.IAC && data[j + 1] == PROTCMD.SE) {
                index = j + 1;
                return;
            }
            j++;
        }

        index = len - 1;
        return;
    }

    index += 1;
}

void AsyncTelnetServer::ProcessLine(AsyncClient* client, const String& line) {
    AsyncTelnetSession* session = FindSession(client);
    if (session == nullptr) return;

    String incomeData = line;
    incomeData.trim();

    if (incomeData.equals(".")) {
        incomeData = session->LastInput;
    } else if (incomeData.length() > 0) {
        session->LastInput = incomeData;
    }

    if (incomeData.isEmpty()) {
        client->write(String(session->User + String(" ") + ASYNCTELNETSERVER_DEFAULTPROMPT).c_str());
        return;
    }

    String command;
    String parameters;
    String parameter[ASYNCTELNETSERVER_MAXCOMMANDPARAMETERS];

    int firstSpace = incomeData.indexOf(' ');
    if (firstSpace < 0) {
        command = incomeData;
        parameters = "";
    } else {
        command = incomeData.substring(0, firstSpace);
        parameters = incomeData.substring(firstSpace + 1);
        parameters.trim();
    }

    for (uint8_t i = 0; i < ASYNCTELNETSERVER_MAXCOMMANDPARAMETERS; i++) {
        if (parameters.isEmpty()) {
            parameter[i] = "";
            continue;
        }

        int nextSpace = parameters.indexOf(' ');
        if (nextSpace < 0) {
            parameter[i] = parameters;
            parameters = "";
        } else {
            parameter[i] = parameters.substring(0, nextSpace);
            parameters = parameters.substring(nextSpace + 1);
            parameters.trim();
        }
    }

    bool validCommand = false;

    for (AsyncTelnetCommand* cmd : mCommandList) {
        if (command == cmd->Command) {
            if (parameter[0].equalsIgnoreCase("-h") || parameter[0].equalsIgnoreCase("-?") || parameter[0].equalsIgnoreCase("--help")) {
                client->write(String(cmd->HelpMessage + "\r\n\r\n").c_str());
            } else {
                cmd->Callback(client, parameter);
                if (cmd->Command != ASYNCTELNETSERVER_CMD_CLEAR) {
                    client->write("\r\n");
                }
            }

            validCommand = true;
            break;
        }
    }

    if (!validCommand) {
        client->write(String(command + " - " + ASYNCTELNETSERVER_INVALIDCOMMAND + "\r\n\r\n").c_str());
    }

    client->write(String(session->User + String(" ") + ASYNCTELNETSERVER_DEFAULTPROMPT).c_str());
}

uint8_t AsyncTelnetServer::SessionID(AsyncClient* client) {
    if (client == nullptr) return 0;

    uint8_t ret = 0;
    for (AsyncTelnetSession* session : mSessions) {
        ret++;
        if ((session->RemoteIP == client->remoteIP()) &&
            (session->RemotePort == client->remotePort())) {
            return ret;
        }
    }

    return 0;
}
