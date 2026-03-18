#include "telnet.h"

void Telnet::Begin() {
    if (Settings.TelnetServer.Enabled() == true) {
        devTelnetServer = new AsyncTelnetServer(Settings.TelnetServer.Port());

        devTelnetServer->WelcomeMessage = Version.ProductFamily + " " + Settings.Network.Hostname() + " - Welcome";
        
        devTelnetServer->onSessionBegin = [&](AsyncClient* client, AsyncTelnetSession* session) {
            devLog->Write("Telnet Server: Session started " + String(session->User + "@" + session->RemoteIP.toString() + ":" + session->RemotePort), LOGLEVEL_INFO);
        };
        devTelnetServer->onSessionEnd = [&](AsyncClient* client, AsyncTelnetSession* session) {
            devLog->Write("Telnet Server: Session ended " + String(session->User + "@" + session->RemoteIP.toString() + ":" + session->RemotePort), LOGLEVEL_INFO);
        };

        //Commands

        // ver
        devTelnetServer->onCommand("ver", "Show device version info\r\n\r\nver", [&](AsyncClient* client, String* parameter) {
            client->write(String("Version      | " + Version.ProductFamily + " " + Version.Software.Info() + "\r\n").c_str());
        });

        devTelnetServer->onCommand("memory", "Show device memory information\r\n\r\nmemory", [&](AsyncClient* client, String* parameter) {
            uint32_t heapFree  = ESP.getFreeHeap();
            uint32_t heapTotal = ESP.getHeapSize();
            uint32_t heapMin   = ESP.getMinFreeHeap();
            uint32_t heapMax   = ESP.getMaxAllocHeap();

            uint32_t internalFree  = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
            uint32_t internalTotal = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
            uint32_t internalMin   = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);

            uint32_t psramFree  = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
            uint32_t psramTotal = ESP.getPsramSize();

            float heapPct     = (heapTotal > 0) ? ((float)heapFree / heapTotal) * 100.0 : 0;
            float internalPct = (internalTotal > 0) ? ((float)internalFree / internalTotal) * 100.0 : 0;
            float psramPct    = (psramTotal > 0) ? ((float)psramFree / psramTotal) * 100.0 : 0;

            String result;

            result += "Heap         | Free: " + String(heapFree) + " / " + String(heapTotal) + " bytes (" + String(heapPct, 1) + "%)\r\n";
            result += "             | Minimum free: " + String(heapMin) + " bytes\r\n";
            result += "             | Max allocatable block: " + String(heapMax) + " bytes\r\n\r\n";

            result += "Internal RAM | Free: " + String(internalFree) + " / " + String(internalTotal) + " bytes (" + String(internalPct, 1) + "%)\r\n";
            result += "             | Minimum free: " + String(internalMin) + " bytes\r\n\r\n";
            
            result += "PSRAM        | Free: " + String(psramFree) + " / " + String(psramTotal) + " bytes (" + String(psramPct, 1) + "%)\r\n";

            client->write(result.c_str());
        });

        devTelnetServer->onCommand("storage", "Show device storage information\r\n\r\nstorage", [&](AsyncClient* client, String* parameter) {
            uint32_t flashChipSize   = ESP.getFlashChipSize();
            uint32_t flashChipSpeed  = ESP.getFlashChipSpeed();
            uint32_t flashSketchSize = ESP.getSketchSize();
            uint32_t flashFreeSketch = ESP.getFreeSketchSpace();

            size_t fsTotal = 0, fsUsed  = 0;

            if (LittleFS.begin(true)) {
                fsTotal = LittleFS.totalBytes();
                fsUsed  = LittleFS.usedBytes();
            }

            float flashSketchPct = (flashChipSize > 0) ? ((float)flashSketchSize / flashChipSize) * 100.0f : 0.0f;
            float flashFreePct   = (flashChipSize > 0) ? ((float)flashFreeSketch / flashChipSize) * 100.0f : 0.0f;
            float fsUsedPct      = (fsTotal > 0) ? ((float)fsUsed / fsTotal) * 100.0f : 0.0f;
            float fsFreePct      = (fsTotal > 0) ? ((float)(fsTotal - fsUsed) / fsTotal) * 100.0f : 0.0f;

            String result;

            result += "Flash        | Chip size: " + String(flashChipSize) + " bytes\r\n";
            result += "             | Chip speed: " + String(flashChipSpeed) + " Hz\r\n";
            result += "             | Sketch size: " + String(flashSketchSize) + " / " + String(flashChipSize) + " bytes (" + String(flashSketchPct, 1) + "%)\r\n";
            result += "             | Free sketch space: " + String(flashFreeSketch) + " / " + String(flashChipSize) + " bytes (" + String(flashFreePct, 1) + "%)\r\n\r\n";
            
            result += "File System  | Used: " + String(fsUsed) + " / " + String(fsTotal) + " bytes (" + String(fsUsedPct, 1) + "%)\r\n";
            result += "             | Free: " + String(fsTotal - fsUsed) + " / " + String(fsTotal) + " bytes (" + String(fsFreePct, 1) + "%)\r\n";

            client->write(result.c_str());
        });

        devTelnetServer->onCommand("logon", "Log into the system with specific credential\r\n\r\nlogon [username] [password]", [&](AsyncClient* client, String* parameter) {
            if (parameter[0].isEmpty() || parameter[1].isEmpty()) {
                client->write("Missing username and password.\r\n");
            } else {
                UserReturn ret = Settings.Users.Authenticate(parameter[0], parameter[1]);

                switch (ret) {
                    case UserReturn::Authenticated : {
                        AsyncTelnetSession* session = devTelnetServer->CurrentSession(client);
                        
                        if (session != nullptr) {
                            session->User = parameter[0];
                            session->Admin = Settings.Users.IsAdmin(session->User);
                        }
                        
                        client->write(String("Logon successful for user " + parameter[0] + ".\r\n").c_str());
                        devLog->Write("Telnet Server: Logon successful for " + parameter[0] + "@" + client->remoteIP().toString() + ":" + String(client->remotePort()), LOGLEVEL_INFO);
                        return;
                    }
                    break;

                    case UserReturn::InvalidCredentials : {
                        client->write(String("Logon failed for user " + parameter[0] + " - Invalid credentials.\r\n").c_str());
                        devLog->Write("Telnet Server: Logon failed for " + parameter[0] + "@" + client->remoteIP().toString() + ":" + String(client->remotePort()) + " - Invalid credentials", LOGLEVEL_WARNING);
                        return;
                    }
                    break;

                    case UserReturn::UserNotFound : {
                        client->write(String("Logon failed for user " + parameter[0] + " - user not found.\r\n").c_str());
                        devLog->Write("Telnet Server: Logon failed for " + parameter[0] + "@" + client->remoteIP().toString() + ":" + String(client->remotePort()) + " - User not found", LOGLEVEL_WARNING);
                        return;
                    }
                    break;
                
                    default:
                    break;
                }
            }
        });

        devTelnetServer->onCommand("reboot", "Reboot the device\r\n\r\nreboot", [&](AsyncClient* client, String* parameter) {
            DeviceRestart();
        }, true);

        devTelnetServer->onCommand("cmp", "Manage components\r\n\r\ncmp", [&](AsyncClient* client, String* parameter) {
            if (parameter[0].isEmpty()) {
                client->write("Missing parameters.\r\n");
            } else {
                if (parameter[0].equalsIgnoreCase("list")) {
                    String result;
                    result += "Components   | Count: " + String(Settings.Components.Count()) + "\r\n";
                    for (auto m : Settings.Components) {
                        result += "             | " + EnumToString(AvailableComponentClasses, m->Class()) + ": " + m->Name() + "\r\n";
                    }
                    client->write(result.c_str());
                }
            }
        });

        devTelnetServer->onCommand("ping", "Ping IP or host\r\n\r\nping [destination] [-n ntimes]", [&](AsyncClient* client, String* parameter) {
            if (parameter[0].isEmpty()) {
                client->write("Missing destination.\r\n");
                return;
            }

            String destination = parameter[0];

            struct hostent* host = gethostbyname(destination.c_str());
            if (!host) {
                client->write(String("ping: " + destination + ": Name or service not known\r\n").c_str());
                return;
            }

            struct sockaddr_in target;
            memset(&target, 0, sizeof(target));
            target.sin_family = AF_INET;
            target.sin_port = 0;
            target.sin_addr.s_addr = *(uint32_t*)host->h_addr;

            char ip[16];
            inet_ntoa_r(target.sin_addr, ip, sizeof(ip));

            bool isDnsName = true;
            ip_addr_t tmpAddr;
            if (ipaddr_aton(destination.c_str(), &tmpAddr)) {
                isDnsName = false;
            }

            if (isDnsName)
                client->write(String("PING " + destination + " (" + String(ip) + ") 32 bytes of data.\r\n").c_str());
            else
                client->write(String("PING " + destination + " 32 bytes of data.\r\n").c_str());

            int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
            if (sock < 0) {
                client->write("ping: cannot create socket\r\n");
                return;
            }

            struct timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

            uint16_t identifier = (uint16_t)(ESP.getEfuseMac() & 0xFFFF);

            uint32_t sentCount = 0;
            uint32_t recvCount = 0;
            uint32_t minTime = 0xFFFFFFFF;
            uint32_t maxTime = 0;
            uint32_t totalTime = 0;

            int ntimes = 4;
            if (parameter[1].equalsIgnoreCase("-n")) ntimes = parameter[2].toInt();

            for (int i = 0; i < ntimes; i++) {
                struct icmp_echo_hdr icmp;
                memset(&icmp, 0, sizeof(icmp));

                icmp.type = ICMP_ECHO;
                icmp.code = 0;
                icmp.id = htons(identifier);
                icmp.seqno = htons((uint16_t)i);

                uint8_t packet[32];
                memset(packet, 0, sizeof(packet));
                memcpy(packet, &icmp, sizeof(icmp));

                for (size_t j = sizeof(icmp); j < sizeof(packet); j++) packet[j] = (uint8_t)j;

                ((struct icmp_echo_hdr*)packet)->chksum = 0;
                ((struct icmp_echo_hdr*)packet)->chksum = inet_chksum(packet, sizeof(packet));

                uint32_t start = millis();

                int sent = sendto(sock, packet, sizeof(packet), 0, (struct sockaddr*)&target, sizeof(target));
                sentCount++;

                if (sent < 0) {
                    client->write(String("From " + String(ip) + " icmp_seq=" + String(i + 1) + " send failed\r\n").c_str());
                    delay(1000);
                    continue;
                }

                uint8_t recvbuf[128];
                struct sockaddr_in from;
                socklen_t fromlen = sizeof(from);

                int len = recvfrom(sock, recvbuf, sizeof(recvbuf), 0, (struct sockaddr*)&from, &fromlen);
                uint32_t elapsed = millis() - start;

                if (len > 0) {
                    recvCount++;
                    totalTime += elapsed;
                    if (elapsed < minTime) minTime = elapsed;
                    if (elapsed > maxTime) maxTime = elapsed;

                    char fromIp[16];
                    inet_ntoa_r(from.sin_addr, fromIp, sizeof(fromIp));

                    client->write(String(String(sizeof(packet)) + " bytes from " + String(fromIp) + ": icmp_seq=" + String(i + 1) + " ttl=64 time=" + String(elapsed) + " ms\r\n").c_str());
                } else {
                    client->write(String("Request timeout for icmp_seq " + String(i + 1) + "\r\n").c_str());
                }

                delay(1000);
            }

            close(sock);

            uint32_t loss = 0;
            if (sentCount > 0) loss = ((sentCount - recvCount) * 100UL) / sentCount;

            client->write("\r\n");
            client->write(String("--- " + destination + " ping statistics ---\r\n").c_str());
            client->write(String(String(sentCount) + " packets transmitted, " + String(recvCount) + " received, " + String(loss) + "% packet loss\r\n").c_str());

            if (recvCount > 0) {
                uint32_t avgTime = totalTime / recvCount;
                client->write(String("rtt min/avg/max = " + String(minTime) + "/" + String(avgTime) + "/" + String(maxTime) + " ms\r\n").c_str());
            }
        }, true);

        // network
        registerCommand_network();

        devTelnetServer->onCommand("ntp", "Show or change NTP configuration\r\n\r\nntp [options]", [&](AsyncClient* client, String* parameter) {
            if (!parameter[0].isEmpty()) {
                if (parameter[0].equalsIgnoreCase("enable") || parameter[0].equalsIgnoreCase("true") || parameter[0].equalsIgnoreCase("on") || parameter[0].equalsIgnoreCase("yes")) {
                    Settings.General.NTPUpdate(true);
                } else if (parameter[0].equalsIgnoreCase("disable") || parameter[0].equalsIgnoreCase("false") || parameter[0].equalsIgnoreCase("off") || parameter[0].equalsIgnoreCase("no")) {
                    Settings.General.NTPUpdate(false);
                } else {
                    Settings.General.NTPServer(parameter[0]);
                }

                Settings.Save();
            }

            String result;
            result += "NTP          | Enabled: " + String(Settings.General.NTPUpdate() ? "Yes" : "No") + "\r\n";
            result += "             | Server: " + Settings.General.NTPServer() + "\r\n";
            client->write(result.c_str());
        }, true);

        devTelnetServer->onCommand("mqtt", "Show or change MQTT configuration\r\n\r\nmqtt [options]", [&](AsyncClient* client, String* parameter) {
            if (!parameter[0].isEmpty()) {
                if (parameter[0].equalsIgnoreCase("enable")) {
                    Settings.MQTT.Enabled(true);
                } else if (parameter[0].equalsIgnoreCase("disable")) {
                    Settings.MQTT.Enabled(false);
                } else if (parameter[0].equalsIgnoreCase("broker")) {
                    Settings.MQTT.Broker(parameter[1]);
                } else if (parameter[0].equalsIgnoreCase("port")) {
                    Settings.MQTT.Port(parameter[1].toInt());
                } else if (parameter[0].equalsIgnoreCase("user")) {
                    Settings.MQTT.User(parameter[1]);
                } else if (parameter[0].equalsIgnoreCase("password")) {
                    Settings.MQTT.Password(parameter[1]);
                }

                Settings.Save();
            }

            String result;
            result += "MQTT         | Enabled: " + String(Settings.MQTT.Enabled() ? "Yes" : "No") + "\r\n";
            result += "             | Broker: " + Settings.MQTT.Broker() + "\r\n";
            result += "             | Port: " + String(Settings.MQTT.Port()) + "\r\n";
            result += "             | User: " + Settings.MQTT.User() + "\r\n";
            result += "             | Password: " + Settings.MQTT.Password() + "\r\n";
            client->write(result.c_str());
        }, true);

        devTelnetServer->onCommand("webserver", "Show or change WebServer configuration\r\n\r\nwebserver [options]", [&](AsyncClient* client, String* parameter) {
            if (!parameter[0].isEmpty()) {
                if (parameter[0].equalsIgnoreCase("enable")) {
                    Settings.WebServer.Enabled(true);
                } else if (parameter[0].equalsIgnoreCase("disable")) {
                    Settings.WebServer.Enabled(false);
                } else if (parameter[0].equalsIgnoreCase("port")) {
                    Settings.WebServer.Port(parameter[1].toInt());
                } else if (parameter[0].equalsIgnoreCase("token")) {
                    Settings.WebServer.WebHooksToken(parameter[1]);
                }

                Settings.Save();
            }

            String result;
            result += "Web Server   | Enabled: " + String(Settings.WebServer.Enabled() ? "Yes" : "No") + "\r\n";
            result += "             | Port: " + String(Settings.WebServer.Port()) + "\r\n";
            result += "             | Token: " + Settings.WebServer.WebHooksToken() + "\r\n";
            client->write(result.c_str());
        }, true);

        devTelnetServer->onCommand("telnet", "Show or change Telnet configuration\r\n\r\ntelnet [options]", [&](AsyncClient* client, String* parameter) {
            if (!parameter[0].isEmpty()) {
                if (parameter[0].equalsIgnoreCase("enable")) {
                    Settings.TelnetServer.Enabled(true);
                } else if (parameter[0].equalsIgnoreCase("disable")) {
                    Settings.TelnetServer.Enabled(false);
                } else if (parameter[0].equalsIgnoreCase("port")) {
                    Settings.TelnetServer.Port(parameter[1].toInt());
                }

                Settings.Save();
            }

            String result;
            result += "Telnet       | Enabled: " + String(Settings.TelnetServer.Enabled() ? "Yes" : "No") + "\r\n";
            result += "             | Port: " + String(Settings.TelnetServer.Port()) + "\r\n";
            client->write(result.c_str());
        }, true);

        devTelnetServer->onCommand("datetime", "Show current date and time\r\n\r\ndatetime", [&](AsyncClient* client, String* parameter) {
            client->write(String(devClock->CurrentDateTime() + "\r\n").c_str());
        });

        devTelnetServer->begin();
        devLog->Write("Telnet Server: Enabled on port " + String(Settings.TelnetServer.Port()), LOGLEVEL_INFO);
    } else {
        devLog->Write("Telnet Server: Disabled", LOGLEVEL_INFO);
    }
}

void Telnet::registerCommand_network() {
    devTelnetServer->onCommand("network", "Show or change network configuration\r\n\r\nnetwork [options]", [&](AsyncClient* client, String* parameter) {
        String result;
        bool changed = false;

        if (parameter[0].isEmpty()) {
            result += "                          Actual Status                 Saved Settings\r\n\r\n";
            result += "Network      | SSID:      " + LimitString(devNetwork->SSID(), 30, true) + LimitString(Settings.Network.SSID(), 30, true) + "\r\n";
            result += "             | Hostname:  " + LimitString(devNetwork->Hostname(), 30, true) + LimitString(Settings.Network.Hostname(), 30, true) + "\r\n";
            result += "             | DHCP:      " + LimitString(devNetwork->DHCP_Client() ? "Yes" : "No", 30, true) + LimitString(Settings.Network.DHCPClient() ? "Yes" : "No", 30, true) + "\r\n";
            result += "             | IP:        " + LimitString(devNetwork->IP_Address().toString(), 30, true) + LimitString(Settings.Network.IP_Address().toString(), 30, true) + "\r\n";
            result += "             | Gateway:   " + LimitString(devNetwork->Gateway().toString(), 30, true) + LimitString(Settings.Network.Gateway().toString(), 30, true) + "\r\n";
            result += "             | Mask:      " + LimitString(devNetwork->Netmask().toString(), 30, true) + LimitString(Settings.Network.Netmask().toString(), 30, true) + "\r\n\r\n";
            result += "DNS          | Primary:   " + LimitString(devNetwork->DNS_Server(0).toString(), 30, true) + LimitString(Settings.Network.DNS(0).toString(), 30, true) + "\r\n";
            result += "             | Secondary: " + LimitString(devNetwork->DNS_Server(1).toString(), 30, true) + LimitString(Settings.Network.DNS(1).toString(), 30, true) + "\r\n\r\n";
            result += "MAC          | Address:   " + devNetwork->MAC_Address() + "\r\n";
        } else {
            result += "                          Actual Status                 Saved Settings\r\n\r\n";
            if (parameter[0].equalsIgnoreCase("hostname")) {
                if (!parameter[1].isEmpty() && !Settings.Network.Hostname().equalsIgnoreCase(parameter[1])) {
                    Settings.Network.Hostname(parameter[1]);
                    changed = true;
                }
                result += "Network      | Hostname:  " + LimitString(devNetwork->Hostname(), 30, true) + LimitString(Settings.Network.Hostname(), 30, true) + "\r\n";
            } else if (parameter[0].equalsIgnoreCase("dhcp")) {
                if (parameter[1].equalsIgnoreCase("enable") || parameter[1].equalsIgnoreCase("true") || parameter[1].equalsIgnoreCase("on") || parameter[1].equalsIgnoreCase("yes")) {
                    if (!Settings.Network.DHCPClient()) {
                        Settings.Network.DHCPClient(true);
                        changed = true;
                    }
                } else if (parameter[1].equalsIgnoreCase("disable") || parameter[1].equalsIgnoreCase("false") || parameter[1].equalsIgnoreCase("off") || parameter[1].equalsIgnoreCase("no")) {
                    if (Settings.Network.DHCPClient()) {
                        Settings.Network.DHCPClient(false);
                        changed = true;
                    }
                } 

                result += "Network      | DHCP:      " + LimitString(devNetwork->DHCP_Client() ? "Yes" : "No", 30, true) + LimitString(Settings.Network.DHCPClient() ? "Yes" : "No", 30, true) + "\r\n";
            } else if (parameter[0].equalsIgnoreCase("ip")) {
                if (!parameter[1].isEmpty() && !Settings.Network.IP_Address().toString().equalsIgnoreCase(parameter[1])) {
                    Settings.Network.IP_Address(parameter[1]);
                    changed = true;
                }
                result += "Network      | IP:        " + LimitString(devNetwork->IP_Address().toString(), 30, true) + LimitString(Settings.Network.IP_Address().toString(), 30, true) + "\r\n";
            } else if (parameter[0].equalsIgnoreCase("gateway")) {
                if (!parameter[1].isEmpty() && !Settings.Network.Gateway().toString().equalsIgnoreCase(parameter[1])) {
                    Settings.Network.Gateway(parameter[1]);
                    changed = true;
                }
                result += "Network      | Gateway:   " + LimitString(devNetwork->Gateway().toString(), 30, true) + LimitString(Settings.Network.Gateway().toString(), 30, true) + "\r\n";
            } else if (parameter[0].equalsIgnoreCase("mask")) {
                if (!parameter[1].isEmpty() && !Settings.Network.Netmask().toString().equalsIgnoreCase(parameter[1])) {
                    Settings.Network.Netmask(parameter[1]);
                    changed = true;
                }
                result += "Network      | Mask:      " + LimitString(devNetwork->Netmask().toString(), 30, true) + LimitString(Settings.Network.Netmask().toString(), 30, true) + "\r\n";
            } else if (parameter[0].equalsIgnoreCase("dns1")) {
                if (!parameter[1].isEmpty() && !Settings.Network.DNS(0).toString().equalsIgnoreCase(parameter[1])) {
                    Settings.Network.DNS(0, parameter[1]);
                    changed = true;
                }
                result += "DNS          | Primary:   " + LimitString(devNetwork->DNS_Server(0).toString(), 30, true) + LimitString(Settings.Network.DNS(0).toString(), 30, true) + "\r\n";
            } else if (parameter[0].equalsIgnoreCase("dns2")) {
                if (!parameter[1].isEmpty() && !Settings.Network.DNS(1).toString().equalsIgnoreCase(parameter[1])) {
                    Settings.Network.DNS(1, parameter[1]);
                    changed = true;
                }
                result += "DNS          | Secondary: " + LimitString(devNetwork->DNS_Server(1).toString(), 30, true) + LimitString(Settings.Network.DNS(1).toString(), 30, true) + "\r\n";
            } else if (parameter[0].equalsIgnoreCase("mac")) {
                result += "MAC          | Address:   " + devNetwork->MAC_Address() + "\r\n";
            }  else {
                result += "Invalid network parameter.\r\n";
            }
        }
        
        if (changed) {
            result += "\r\nSettings changed - you must reboot to apply changes.\r\n";
            Settings.Save();
        }
        client->write(result.c_str());
    }, true);
}