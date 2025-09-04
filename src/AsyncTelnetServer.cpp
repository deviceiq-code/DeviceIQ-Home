#include "AsyncTelnetServer.h"

AsyncTelnetServer::AsyncTelnetServer(uint16_t port) : mPort(port) {    
    onCommand(ASYNCTELNETSERVER_CMD_CLEAR, ASYNCTELNETSERVER_HLP_CLEAR, [&](AsyncClient* client) {
        client->write(String("\x1B[2J\x1B[H").c_str());
    });

    onCommand(ASYNCTELNETSERVER_CMD_EXIT, ASYNCTELNETSERVER_HLP_EXIT, [&](AsyncClient* client) {
        mSessions.erase(mSessions.begin() + (SessionID(client) - 1));
        client->write("Session closed.\r\n\r\n");
        client->stop();
    });

    onCommand(ASYNCTELNETSERVER_CMD_SESSIONS, ASYNCTELNETSERVER_HLP_SESSIONS, [&](AsyncClient* client) {
        client->write("Current sessions:\r\n\r\n");
        for (AsyncTelnetSession* session : mSessions) {
            client->write(String(session->User + "@" + session->RemoteIP.toString() + ":" + String(session->RemotePort) + "\r\n").c_str());
        }
    });

    onCommand(ASYNCTELNETSERVER_CMD_WHOAMI, ASYNCTELNETSERVER_HLP_WHOAMI, [&](AsyncClient* client) {
        client->write(String(CurrentSession(client)->User + "@" + client->remoteIP().toString() + ":" + String(client->remotePort()) + "\r\n").c_str());
    });

    mAsyncServer = new AsyncServer(mPort);

    mAsyncServer->onClient([&](void* s, AsyncClient* client) {
        mSessions.push_back(new AsyncTelnetSession({client->remoteIP(), client->remotePort(), ASYNCTELNETSERVER_GUESTUSER}));

        char cmd[] = {
            PROTCMD.IAC, PROTCMD.DO, 34
        };
        
        client->write(cmd, sizeof(cmd));
        
        client->write(String(WelcomeMessage + "\r\n\r\n" + Prompt).c_str());

        client->onData([&](void* arg, AsyncClient* client, void* data, size_t len) {
            char tmp[len];
            memcpy(tmp, (char*)data, len);

            // if (tmp[0] == PROTCMD.IAC) { // Negociation
            //     // for (uint32_t i = 0; i < len; i++) {
            //     //     if ((tmp[i] == PROTCMD.IAC) && (tmp[i + 1] == PROTCMD.WONT) && (tmp[i + 2] == 34)) {
            //     //         mMode = ASYNCTELNETMODE_CHAR;
            //     //     } else {
            //     //         mMode = ASYNCTELNETMODE_LINE;
            //     //     }
            //     // }
            //     return;
            // }
            
            if (client->space() > 32 && client->canSend()) {
                // if (mMode == ASYNCTELNETMODE_LINE) {
                    mIncomeData = String((char*)data).substring(0, len);
                // } else if (mMode == ASYNCTELNETMODE_CHAR) {
                //     char c = tmp[0];
                    
                //     if (((uint8_t)c == 10) || ((uint8_t)c == 13)) {
                //         client->write(String("\r\n").c_str());
                //     } else {
                //         mIncomeData += c;
                //         client->write(String(c).c_str());
                //         return;
                //     }
                // }

                mIncomeData.trim();

                if (mIncomeData.equals("."))
                    mIncomeData = mLastIncomeData; // Restore last command and parammeters
                else
                    if (mIncomeData.length() > 0) mLastIncomeData = mIncomeData; // Save last command and parammeters

                mCommand = mIncomeData.substring(0, mIncomeData.indexOf(' '));
                mIncomeData = mIncomeData.substring(mCommand.length() + 1, mIncomeData.length());

                for (uint8_t i = 0; i < ASYNCTELNETSERVER_MAXCOMMANDPARAMETERS; i++) {
                    if (mIncomeData.isEmpty() == false) {
                        mParameter[i] = mIncomeData.substring(0, mIncomeData.indexOf(' '));
                        mIncomeData = mIncomeData.substring(mParameter[i].length() + 1, mIncomeData.length());
                    } else {
                        mParameter[i].clear();
                    }
                }

                mIncomeData.clear();

                if (mCommand.isEmpty() == false) {
                    bool ValidCommand = false;

                    for (AsyncTelnetCommand* cmd : mCommandList) {
                        if (mCommand == cmd->Command) {
                            if ((mParameter[0].equalsIgnoreCase("-h")) || (mParameter[0].equalsIgnoreCase("-?")) || (mParameter[0].equalsIgnoreCase("--help"))) {
                                client->write(String(cmd->HelpMessage + "\r\n\r\n").c_str());
                            } else {
                                cmd->Callback(client);
                                if(cmd->Command.equals(ASYNCTELNETSERVER_CMD_CLEAR) == false) client->write("\r\n");
                            }

                            ValidCommand = true;
                            break;
                        }
                    }

                    if (ValidCommand == false) client->write(String(mCommand + " - " + ASYNCTELNETSERVER_INVALIDCOMMAND + "\r\n\r\n").c_str());
                }
                
                client->write(Prompt.c_str());
            }
        });

        client->onDisconnect([&](void* arg, AsyncClient* client) {}, NULL);
        client->onError([&](void* arg, AsyncClient* client, int8_t error) {}, NULL);
        client->onTimeout([&](void* arg, AsyncClient* client, uint32_t time) {}, NULL);
    }, NULL);
}

uint8_t AsyncTelnetServer::SessionID(AsyncClient* client) {
    uint8_t ret = 0;
    for (AsyncTelnetSession* session : mSessions) {
        ret++;
        if ((session->RemoteIP == client->remoteIP()) && (session->RemotePort == client->remotePort())) return ret;
    }
    return 0;
}