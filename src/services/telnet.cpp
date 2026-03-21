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
        registerCommand_logon();
        registerCommand_reboot();
        registerCommand_network();
        registerCommand_ntp();
        registerCommand_ver();
        registerCommand_memory();
        registerCommand_storage();
        registerCommand_ping();
        registerCommand_telnet();
        registerCommand_webserver();
        registerCommand_mqtt();
        registerCommand_comp();
        
        devTelnetServer->begin();
        devLog->Write("Telnet Server: Enabled on port " + String(Settings.TelnetServer.Port()), LOGLEVEL_INFO);
    } else {
        devLog->Write("Telnet Server: Disabled", LOGLEVEL_INFO);
    }
}

void Telnet::registerCommand_logon() {
    devTelnetServer->onCommand("logon", "Log into the system with specific credential\r\n\r\nlogon [username] [password]", [&](AsyncClient* client, String* parameter) {
        String result;
        
        if (parameter[0].isEmpty() || parameter[1].isEmpty()) {
            result += "Logon          | Missing username and password.\r\n";
        } else {
            UserReturn ret = Settings.Users.Authenticate(parameter[0], parameter[1]);

            switch (ret) {
                case UserReturn::Authenticated : {
                    AsyncTelnetSession* session = devTelnetServer->CurrentSession(client);
                    
                    if (session != nullptr) {
                        session->User = parameter[0];
                        session->Admin = Settings.Users.IsAdmin(session->User);
                    }
                    
                    result += "Logon          | Logon successful for user " + parameter[0] + ".\r\n";
                    devLog->Write("Telnet Server: Logon successful for " + parameter[0] + "@" + client->remoteIP().toString() + ":" + String(client->remotePort()), LOGLEVEL_INFO);
                }
                break;

                case UserReturn::InvalidCredentials : {
                    result += "Logon          | Logon failed for user " + parameter[0] + " - Invalid credentials.\r\n";
                    devLog->Write("Telnet Server: Logon failed for " + parameter[0] + "@" + client->remoteIP().toString() + ":" + String(client->remotePort()) + " - Invalid credentials", LOGLEVEL_WARNING);
                }
                break;

                case UserReturn::UserNotFound : {
                    result += "Logon          | Logon failed for user " + parameter[0] + " - user not found.\r\n";
                    devLog->Write("Telnet Server: Logon failed for " + parameter[0] + "@" + client->remoteIP().toString() + ":" + String(client->remotePort()) + " - User not found", LOGLEVEL_WARNING);
                    return;
                }
                break;
            
                default:
                break;
            }
        }
        client->write(result.c_str());
    });
}
void Telnet::registerCommand_reboot() {
    devTelnetServer->onCommand("reboot", "Reboot the device\r\n\r\nreboot", [&](AsyncClient* client, String* parameter) {
        DeviceRestart();
    }, true);
}
void Telnet::registerCommand_network() {
    devTelnetServer->onCommand("network", "Show or change network configuration\r\n\r\nnetwork [options]", [&](AsyncClient* client, String* parameter) {
        String result;
        bool changed = false;

        if (parameter[0].isEmpty()) {
            result += "Network        | " + LimitString("SSID: " + devNetwork->SSID(), 30, true) + LimitString(Settings.Network.SSID(), 30, true) + "\r\n";
            result += "               | " + LimitString("Hostname: " + devNetwork->Hostname(), 30, true) + LimitString(Settings.Network.Hostname(), 30, true) + "\r\n";
            result += "               | " + LimitString("DHCP: " + String(devNetwork->DHCP_Client() ? "Yes" : "No"), 30, true) + LimitString(Settings.Network.DHCPClient() ? "Yes" : "No", 30, true) + "\r\n";
            result += "               | " + LimitString("IP: " + devNetwork->IP_Address().toString(), 30, true) + LimitString(Settings.Network.IP_Address().toString(), 30, true) + "\r\n";
            result += "               | " + LimitString("Gateway: " + devNetwork->Gateway().toString(), 30, true) + LimitString(Settings.Network.Gateway().toString(), 30, true) + "\r\n";
            result += "               | " + LimitString("Mask: " + devNetwork->Netmask().toString(), 30, true) + LimitString(Settings.Network.Netmask().toString(), 30, true) + "\r\n\r\n";
            result += "DNS            | " + LimitString("Primary: " + devNetwork->DNS_Server(0).toString(), 30, true) + LimitString(Settings.Network.DNS(0).toString(), 30, true) + "\r\n";
            result += "               | " + LimitString("Secondary: " + devNetwork->DNS_Server(1).toString(), 30, true) + LimitString(Settings.Network.DNS(1).toString(), 30, true) + "\r\n\r\n";
            result += "MAC            | Address: " + devNetwork->MAC_Address() + "\r\n";
        } else {
            if (parameter[0].equalsIgnoreCase("hostname")) {
                if (!parameter[1].isEmpty() && !Settings.Network.Hostname().equalsIgnoreCase(parameter[1])) {
                    Settings.Network.Hostname(parameter[1]);
                    changed = true;
                }
                result += "Network        | " + LimitString("Hostname: " + devNetwork->Hostname(), 30, true) + LimitString(Settings.Network.Hostname(), 30, true) + "\r\n";
            } else if (parameter[0].equalsIgnoreCase("dhcp")) {
                if (parameter[1].equalsIgnoreCase("true") || parameter[1].equalsIgnoreCase("on") || parameter[1].equalsIgnoreCase("yes")) {
                    if (!Settings.Network.DHCPClient()) {
                        Settings.Network.DHCPClient(true);
                        changed = true;
                    }
                } else if (parameter[1].equalsIgnoreCase("false") || parameter[1].equalsIgnoreCase("off") || parameter[1].equalsIgnoreCase("no")) {
                    if (Settings.Network.DHCPClient()) {
                        Settings.Network.DHCPClient(false);
                        changed = true;
                    }
                }
                result += "Network        | " + LimitString("DHCP: " + devNetwork->DHCP_Client() ? "Yes" : "No", 30, true) + LimitString(Settings.Network.DHCPClient() ? "Yes" : "No", 30, true) + "\r\n";
            } else if (parameter[0].equalsIgnoreCase("ip")) {
                if (!parameter[1].isEmpty() && !Settings.Network.IP_Address().toString().equalsIgnoreCase(parameter[1])) {
                    Settings.Network.IP_Address(parameter[1]);
                    changed = true;
                }
                result += "Network        | " + LimitString("IP: " + devNetwork->IP_Address().toString(), 30, true) + LimitString(Settings.Network.IP_Address().toString(), 30, true) + "\r\n";
            } else if (parameter[0].equalsIgnoreCase("gateway")) {
                if (!parameter[1].isEmpty() && !Settings.Network.Gateway().toString().equalsIgnoreCase(parameter[1])) {
                    Settings.Network.Gateway(parameter[1]);
                    changed = true;
                }
                result += "Network        | " + LimitString("Gateway: " + devNetwork->Gateway().toString(), 30, true) + LimitString(Settings.Network.Gateway().toString(), 30, true) + "\r\n";
            } else if (parameter[0].equalsIgnoreCase("mask")) {
                if (!parameter[1].isEmpty() && !Settings.Network.Netmask().toString().equalsIgnoreCase(parameter[1])) {
                    Settings.Network.Netmask(parameter[1]);
                    changed = true;
                }
                result += "Network        | " + LimitString("Mask: " + devNetwork->Netmask().toString(), 30, true) + LimitString(Settings.Network.Netmask().toString(), 30, true) + "\r\n";
            } else if (parameter[0].equalsIgnoreCase("dns1")) {
                if (!parameter[1].isEmpty() && !Settings.Network.DNS(0).toString().equalsIgnoreCase(parameter[1])) {
                    Settings.Network.DNS(0, parameter[1]);
                    changed = true;
                }
                result += "DNS            | " + LimitString("Primary: " + devNetwork->DNS_Server(0).toString(), 30, true) + LimitString(Settings.Network.DNS(0).toString(), 30, true) + "\r\n";
            } else if (parameter[0].equalsIgnoreCase("dns2")) {
                if (!parameter[1].isEmpty() && !Settings.Network.DNS(1).toString().equalsIgnoreCase(parameter[1])) {
                    Settings.Network.DNS(1, parameter[1]);
                    changed = true;
                }
                result += "DNS            | " + LimitString("Secondary: " + devNetwork->DNS_Server(1).toString(), 30, true) + LimitString(Settings.Network.DNS(1).toString(), 30, true) + "\r\n";
            } else if (parameter[0].equalsIgnoreCase("mac")) {
                result += "MAC            | Address: " + devNetwork->MAC_Address() + "\r\n";
            }  else {
                result += "Network        | Error: Invalid network parameter.\r\n";
            }
        }
        
        if (changed) {
            result += "\r\n               | Settings changed - you must reboot to apply changes.\r\n";
            Settings.Save();
        }
        client->write(result.c_str());
    }, true);
}
void Telnet::registerCommand_ntp() {
    devTelnetServer->onCommand("ntp", "Show or change NTP configuration\r\n\r\nntp [options]", [&](AsyncClient* client, String* parameter) {
        String result;
        bool changed = false;

        if (parameter[0].isEmpty()) {
            result += "NTP            | Enabled: " + String(Settings.General.NTPUpdate() ? "Yes" : "No") + "\r\n";
            result += "               | Server: " + Settings.General.NTPServer() + "\r\n";
        } else if (parameter[0].equalsIgnoreCase("enabled")) {
            if (parameter[1].equalsIgnoreCase("true") || parameter[1].equalsIgnoreCase("on") || parameter[1].equalsIgnoreCase("yes")) {
                if (!Settings.General.NTPUpdate()) {
                    Settings.General.NTPUpdate(true);
                    changed = true;
                }
            } else if (parameter[1].equalsIgnoreCase("false") || parameter[1].equalsIgnoreCase("off") || parameter[1].equalsIgnoreCase("no")) {
                if (Settings.General.NTPUpdate()) {
                    Settings.General.NTPUpdate(false);
                    changed = true;
                }
            }
            result += "NTP            | Enabled: " + String(Settings.General.NTPUpdate() ? "Yes" : "No") + "\r\n";
        } else if (parameter[0].equalsIgnoreCase("server")) {
            if (!parameter[1].isEmpty() && !Settings.General.NTPServer().equalsIgnoreCase(parameter[1])) {
                Settings.General.NTPServer(parameter[1]);
                changed = true;
            }
            result += "NTP            | Server: " + Settings.General.NTPServer() + "\r\n";
        } else {
            result += "NTP            | Invalid ntp parameter.\r\n";
        }
        
        if (changed) {
            result += "\r\n               | Settings changed - you must reboot to apply changes.\r\n";
            Settings.Save();
        }
        client->write(result.c_str());
    }, true);
}
void Telnet::registerCommand_ver() {
    devTelnetServer->onCommand("ver", "Show device version info\r\n\r\nver", [&](AsyncClient* client, String* parameter) {
        String result;

        result += "Version        | Product: " + Version.ProductName + "\r\n";
        result += "               | Family: " + Version.ProductFamily + "\r\n";
        result += "               | Hardware: " + Version.Hardware.Info() + "\r\n";
        result += "               | Software: " + Version.Software.Info() + "\r\n";

        client->write(result.c_str());
    });
}
void Telnet::registerCommand_memory() {
    devTelnetServer->onCommand("memory", "Show device memory information\r\n\r\nmemory", [&](AsyncClient* client, String* parameter) {
        String result;
        
        uint32_t heapFree = ESP.getFreeHeap();
        uint32_t heapTotal = ESP.getHeapSize();
        uint32_t heapMin = ESP.getMinFreeHeap();
        uint32_t heapMax = ESP.getMaxAllocHeap();
        uint32_t internalFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        uint32_t internalTotal = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
        uint32_t internalMin = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        uint32_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        uint32_t psramTotal = ESP.getPsramSize();

        float heapPct = (heapTotal > 0) ? ((float)heapFree / heapTotal) * 100.0 : 0;
        float internalPct = (internalTotal > 0) ? ((float)internalFree / internalTotal) * 100.0 : 0;
        float psramPct = (psramTotal > 0) ? ((float)psramFree / psramTotal) * 100.0 : 0;

        result += "Heap           | Free: " + String(heapFree) + "/" + String(heapTotal) + " bytes (" + String(heapPct, 1) + "%)\r\n";
        result += "               | Min free: " + String(heapMin) + " bytes\r\n";
        result += "               | Max alloc: " + String(heapMax) + " bytes\r\n\r\n";
        result += "Internal RAM   | Free: " + String(internalFree) + "/" + String(internalTotal) + " bytes (" + String(internalPct, 1) + "%)\r\n";
        result += "               | Min free: " + String(internalMin) + " bytes\r\n\r\n";
        result += "PSRAM          | Free: " + String(psramFree) + "/" + String(psramTotal) + " bytes (" + String(psramPct, 1) + "%)\r\n";

        client->write(result.c_str());
    });
}
void Telnet::registerCommand_storage() {
    devTelnetServer->onCommand("storage", "Show device storage information\r\n\r\nstorage", [&](AsyncClient* client, String* parameter) {
        String result;

        uint32_t flashChipSize = ESP.getFlashChipSize();
        uint32_t flashChipSpeed = ESP.getFlashChipSpeed();
        uint32_t flashSketchSize = ESP.getSketchSize();
        uint32_t flashFreeSketch = ESP.getFreeSketchSpace();

        size_t fsTotal = 0, fsUsed  = 0;

        if (LittleFS.begin(true)) {
            fsTotal = LittleFS.totalBytes();
            fsUsed  = LittleFS.usedBytes();
        }

        float flashSketchPct = (flashChipSize > 0) ? ((float)flashSketchSize / flashChipSize) * 100.0f : 0.0f;
        float flashFreePct = (flashChipSize > 0) ? ((float)flashFreeSketch / flashChipSize) * 100.0f : 0.0f;
        float fsUsedPct = (fsTotal > 0) ? ((float)fsUsed / fsTotal) * 100.0f : 0.0f;
        float fsFreePct = (fsTotal > 0) ? ((float)(fsTotal - fsUsed) / fsTotal) * 100.0f : 0.0f;

        result += "Storage        | Chip size: " + String(flashChipSize) + " bytes\r\n";
        result += "               | Chip speed: " + String(flashChipSpeed) + " Hz\r\n";
        result += "               | Sketch size: " + String(flashSketchSize) + " / " + String(flashChipSize) + " bytes (" + String(flashSketchPct, 1) + "%)\r\n";
        result += "               | Free sketch space: " + String(flashFreeSketch) + " / " + String(flashChipSize) + " bytes (" + String(flashFreePct, 1) + "%)\r\n\r\n";
        result += "File System    | Used: " + String(fsUsed) + " / " + String(fsTotal) + " bytes (" + String(fsUsedPct, 1) + "%)\r\n";
        result += "               | Free: " + String(fsTotal - fsUsed) + " / " + String(fsTotal) + " bytes (" + String(fsFreePct, 1) + "%)\r\n";

        client->write(result.c_str());
    });
}
void Telnet::registerCommand_ping() {
    devTelnetServer->onCommand("ping", "Ping IP or host\r\n\r\nping [destination] [-n ntimes]", [&](AsyncClient* client, String* parameter) {
        String result;

        if (parameter[0].isEmpty()) {
            result += "Ping           | Error: Missing destination\r\n";
        } else {
            String destination = parameter[0];
            struct hostent* host = gethostbyname(destination.c_str());

            if (!host) {
                result += "Ping           | Error: " + destination + " - name or service not known\r\n";
            } else {
                struct sockaddr_in target;
                memset(&target, 0, sizeof(target));
                target.sin_family = AF_INET;
                target.sin_port = 0;
                target.sin_addr.s_addr = *(uint32_t*)host->h_addr;

                char ip[16];
                inet_ntoa_r(target.sin_addr, ip, sizeof(ip));

                bool isDnsName = true;
                ip_addr_t tmpAddr;
                if (ipaddr_aton(destination.c_str(), &tmpAddr)) isDnsName = false;

                if (isDnsName) result += "Ping           | Destination: " + String(ip) + " - 32 bytes of data.\r\n\r\n";
                else result += "Ping           | Destination: " + destination + " - 32 bytes of data.\r\n\r\n";

                int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
                if (sock < 0) {
                    result += "               | Error: Cannot create socket\r\n\r\n";
                } else {    
                    client->write(result.c_str()); result.clear();

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
                            client->write(String("               | From " + String(ip) + " icmp_seq " + String(i + 1) + " send failed\r\n").c_str());
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

                            client->write(String("               | " + String(sizeof(packet)) + " bytes from " + String(fromIp) + ": icmp_seq = " + String(i + 1) + " ttl = 64 time = " + String(elapsed) + " ms\r\n").c_str());
                        } else {
                            client->write(String("               | Request timeout for icmp_seq " + String(i + 1) + "\r\n").c_str());
                        }

                        delay(1000);
                    }

                    close(sock);

                    uint32_t loss = 0;
                    if (sentCount > 0) loss = ((sentCount - recvCount) * 100UL) / sentCount;

                    result += String("\r\nStatistcs      | Destination: " + destination + "\r\n");
                    result += String("               | Transmitted: " + String(sentCount) + " packets\r\n               | Received: " + String(recvCount) + " packets\r\n               | Lost: " + String(loss) + "% packet loss\r\n");

                    if (recvCount > 0) {
                        uint32_t avgTime = totalTime / recvCount;
                        result += String("               | RTT min/avg/max: " + String(minTime) + "/" + String(avgTime) + "/" + String(maxTime) + " ms\r\n");
                    }
                }
            }
        }

        client->write(result.c_str());
    }, true);
}
void Telnet::registerCommand_telnet() {
    devTelnetServer->onCommand("telnet", "Show or change Telnet configuration\r\n\r\ntelnet [options]", [&](AsyncClient* client, String* parameter) {
        String result;
        bool changed = false;

        if (parameter[0].isEmpty()) {
            result += "Telnet         | Enabled: " + String(Settings.TelnetServer.Enabled() ? "Yes" : "No") + "\r\n";
            result += "               | Port: " + String(Settings.TelnetServer.Port()) + "\r\n";
         } else if (parameter[0].equalsIgnoreCase("enabled")) {
            if (parameter[1].equalsIgnoreCase("true") || parameter[1].equalsIgnoreCase("on") || parameter[1].equalsIgnoreCase("yes")) {
                if (!Settings.TelnetServer.Enabled()) {
                    Settings.TelnetServer.Enabled(true);
                    changed = true;
                }
            } else if (parameter[1].equalsIgnoreCase("false") || parameter[1].equalsIgnoreCase("off") || parameter[1].equalsIgnoreCase("no")) {
                if (Settings.TelnetServer.Enabled()) {
                    Settings.TelnetServer.Enabled(false);
                    changed = true;
                }
            } 
            result += "Telnet         | Enabled: " + String(Settings.TelnetServer.Enabled() ? "Yes" : "No") + "\r\n";
            Settings.Save();
        } else if (parameter[0].equalsIgnoreCase("port")) {
            if (!parameter[1].isEmpty() && (Settings.TelnetServer.Port() != parameter[1].toInt())) {
                Settings.TelnetServer.Port(parameter[1].toInt());
                changed = true;
            }
            result += "Telnet         | Port: " + String(Settings.TelnetServer.Port()) + "\r\n";
        } else {
            result += "Telnet         | Invalid telnet parameter.\r\n";
        }

        if (changed) {
            result += "\r\n               | Settings changed - you must reboot to apply changes.\r\n";
            Settings.Save();
        }
        client->write(result.c_str());
    }, true);
}
void Telnet::registerCommand_webserver() {
    devTelnetServer->onCommand("webserver", "Show or change WerServer configuration\r\n\r\nwebserver [options]", [&](AsyncClient* client, String* parameter) {
        String result;
        bool changed = false;

        if (parameter[0].isEmpty()) {
            result += "WebServer      | Enabled: " + String(Settings.WebServer.Enabled() ? "Yes" : "No") + "\r\n";
            result += "               | Port: " + String(Settings.WebServer.Port()) + "\r\n";
            result += "               | Token: " + Settings.WebServer.WebHooksToken() + "\r\n";
         } else if (parameter[0].equalsIgnoreCase("enabled")) {
            if (parameter[1].equalsIgnoreCase("true") || parameter[1].equalsIgnoreCase("on") || parameter[1].equalsIgnoreCase("yes")) {
                if (!Settings.WebServer.Enabled()) {
                    Settings.WebServer.Enabled(true);
                    changed = true;
                }
            } else if (parameter[1].equalsIgnoreCase("false") || parameter[1].equalsIgnoreCase("off") || parameter[1].equalsIgnoreCase("no")) {
                if (Settings.WebServer.Enabled()) {
                    Settings.WebServer.Enabled(false);
                    changed = true;
                }
            } 
            result += "WebServer      | Enabled: " + String(Settings.WebServer.Enabled() ? "Yes" : "No") + "\r\n";
            Settings.Save();
        } else if (parameter[0].equalsIgnoreCase("port")) {
            if (!parameter[1].isEmpty() && (Settings.WebServer.Port() != parameter[1].toInt())) {
                Settings.WebServer.Port(parameter[1].toInt());
                changed = true;
            }
            result += "WebServer      | Port: " + String(Settings.WebServer.Port()) + "\r\n";
        } else if (parameter[0].equalsIgnoreCase("token")) {
            if (!parameter[1].isEmpty() && (!Settings.WebServer.WebHooksToken().equalsIgnoreCase(parameter[1]))) {
                Settings.WebServer.WebHooksToken(parameter[1]);
                changed = true;
            }
            result += "WebServer      | Token: " + Settings.WebServer.WebHooksToken() + "\r\n";
        } else {
            result += "WebServer      | Invalid webserver parameter.\r\n";
        }

        if (changed) {
            result += "\r\n               | Settings changed - you must reboot to apply changes.\r\n";
            Settings.Save();
        }
        client->write(result.c_str());
    }, true);
}
void Telnet::registerCommand_mqtt() {
    devTelnetServer->onCommand("mqtt", "Show or change mqtt configuration\r\n\r\nmqtt [options]", [&](AsyncClient* client, String* parameter) {
        String result;
        bool changed = false;

        if (parameter[0].isEmpty()) {
            result += "MQTT           | Enabled: " + String(Settings.MQTT.Enabled() ? "Yes" : "No") + "\r\n";
            result += "               | Broker: " + Settings.MQTT.Broker() + "\r\n";
            result += "               | Port: " + String(Settings.MQTT.Port()) + "\r\n";
            result += "               | User: " + Settings.MQTT.User() + "\r\n";
            result += "               | Password: " + Settings.MQTT.Password() + "\r\n";
         } else if (parameter[0].equalsIgnoreCase("enabled")) {
            if (parameter[1].equalsIgnoreCase("true") || parameter[1].equalsIgnoreCase("on") || parameter[1].equalsIgnoreCase("yes")) {
                if (!Settings.MQTT.Enabled()) {
                    Settings.MQTT.Enabled(true);
                    changed = true;
                }
            } else if (parameter[1].equalsIgnoreCase("false") || parameter[1].equalsIgnoreCase("off") || parameter[1].equalsIgnoreCase("no")) {
                if (Settings.MQTT.Enabled()) {
                    Settings.MQTT.Enabled(false);
                    changed = true;
                }
            } 
            result += "MQTT           | Enabled: " + String(Settings.MQTT.Enabled() ? "Yes" : "No") + "\r\n";
            Settings.Save();
        } else if (parameter[0].equalsIgnoreCase("broker")) {
            if (!parameter[1].isEmpty() && (!Settings.MQTT.Broker().equalsIgnoreCase(parameter[1]))) {
                Settings.MQTT.Broker(parameter[1]);
                changed = true;
            }
            result += "MQTT           | Broker: " + Settings.MQTT.Broker() + "\r\n";
        } else if (parameter[0].equalsIgnoreCase("port")) {
            if (!parameter[1].isEmpty() && (Settings.MQTT.Port() != parameter[1].toInt())) {
                Settings.MQTT.Port(parameter[1].toInt());
                changed = true;
            }
            result += "MQTT           | Port: " + String(Settings.MQTT.Port()) + "\r\n";
        } else if (parameter[0].equalsIgnoreCase("user")) {
            if (!parameter[1].isEmpty() && (!Settings.MQTT.User().equalsIgnoreCase(parameter[1]))) {
                Settings.MQTT.User(parameter[1]);
                changed = true;
            }
            result += "MQTT           | User: " + Settings.MQTT.User() + "\r\n";
        } else if (parameter[0].equalsIgnoreCase("password")) {
            if (!parameter[1].isEmpty() && (!Settings.MQTT.Password().equalsIgnoreCase(parameter[1]))) {
                Settings.MQTT.Password(parameter[1]);
                changed = true;
            }
            result += "MQTT           | Password: " + Settings.MQTT.Password() + "\r\n";
        } else {
            result += "MQTT           | Invalid webserver parameter.\r\n";
        }

        if (changed) {
            result += "\r\n               | Settings changed - you must reboot to apply changes.\r\n";
            Settings.Save();
        }
        client->write(result.c_str());
    }, true);
}
void Telnet::registerCommand_comp() {
    devTelnetServer->onCommand("comp", "Manage components\r\n\r\ncomp", [&](AsyncClient* client, String* parameter) {
        String result;
        bool changed = false;

        if (parameter[0].equalsIgnoreCase("list")) {
            result += "Components     | Count: " + String(Settings.Components.Count()) + "\r\n";
            for (auto m : Settings.Components) {
                result += "               | " + EnumToString(AvailableComponentClasses, m->Class()) + ": " + m->Name() + "\r\n";
            }
        } else if (parameter[0].equalsIgnoreCase("bus")) {
            result += "Buses          | Count: " + String(AvailableComponentBuses.size()) + "\r\n\r\n";
            for (auto m : AvailableComponentBuses) {
                result += "               | " + m.first + ":" + String(m.second) + "\r\n";
            }
        } else if (parameter[0].equalsIgnoreCase("class")) {
            result += "Classes        | Count: " + String(AvailableComponentClasses.size()) + "\r\n\r\n";
            for (auto m : AvailableComponentClasses) {
                result += "               | " + m.first + ":" + String(m.second) + "\r\n";
            }
        } else if (parameter[0].equalsIgnoreCase("remove")) {
            if (!parameter[1].isEmpty()) {
                if (Settings.Components.Remove(parameter[1]) != -1) {
                    result += "Components     | Component '" + parameter[1] + "' removed.\r\n";
                    changed = true;
                } else {
                    result += "Components     | Error removing component '" + parameter[1] + "'.\r\n";
                }
            }
        } else if (parameter[0].equalsIgnoreCase("add")) {
            if (!parameter[1].isEmpty() && !parameter[2].isEmpty() && !parameter[3].isEmpty() && !parameter[4].isEmpty() && !parameter[5].isEmpty()) {
                String comp_name = parameter[1];

                if (Settings.Components.IndexOf(comp_name) != -1) {
                    result += "Components     | Component '" + comp_name + "' already exists.\r\n";
                } else {
                    String comp_class = parameter[2];
                    String comp_bus = parameter[3];
                    uint8_t comp_address = parameter[4].toInt();
                    String comp_option = parameter[5];

                    bool added = false;
                    
                    Generic *NewComponent = nullptr;

                    auto it_class = AvailableComponentClasses.find(comp_class);
                    if (it_class == AvailableComponentClasses.end()) {
                        result += "Components     | Invalid class '" + comp_class + "'.\r\n";
                    } else {
                        Classes c = it_class->second;

                        auto it_bus = AvailableComponentBuses.find(comp_bus);
                        if (it_bus == AvailableComponentBuses.end()) {
                            result += "Components     | Invalid bus '" + comp_bus + "'.\r\n";
                        } else {
                            switch (c) {
                                case CLASS_BLINDS: {

                                } break;

                                case CLASS_BUTTON: {
                                    NewComponent = new Button(comp_name, Settings.Components.Count() + 1, AvailableComponentBuses.at(comp_bus), comp_address, (comp_option.equalsIgnoreCase("EdgesOnly") ? ButtonReportModes::BUTTONREPORTMODE_EDGESONLY : ButtonReportModes::BUTTONREPORTMODE_CLICKSONLY));
                                    added = true;
                                } break;

                                case CLASS_CONTACTSENSOR: {
                                    NewComponent = new ContactSensor(comp_name, Settings.Components.Count() + 1, AvailableComponentBuses.at(comp_bus), comp_address, (comp_option.equalsIgnoreCase("Invert") ? true : false));
                                    added = true;
                                } break;

                                case CLASS_CURRENTMETER: {
                                    NewComponent = new Currentmeter(comp_name, Settings.Components.Count() + 1, AvailableComponentBuses.at(comp_bus), comp_address);
                                    added = true;
                                } break;

                                case CLASS_DOORBELL: {
                                    NewComponent = new Doorbell(comp_name, Settings.Components.Count() + 1, AvailableComponentBuses.at(comp_bus), comp_address);
                                    added = true;
                                } break;

                                case CLASS_PIR: {
                                    NewComponent = new PIR(comp_name, Settings.Components.Count() + 1, AvailableComponentBuses.at(comp_bus), comp_address);
                                    added = true;
                                } break;

                                case CLASS_RELAY: {
                                    NewComponent = new Relay(comp_name, Settings.Components.Count() + 1, AvailableComponentBuses.at(comp_bus), comp_address, (comp_option.equalsIgnoreCase("NormallyOpened") ? RelayTypes::RELAYTYPE_NORMALLYOPENED : RelayTypes::RELAYTYPE_NORMALLYCLOSED));
                                    added = true;
                                } break;

                                case CLASS_THERMOMETER: {
                                    ThermometerTypes comp_type = THERMOMETERTYPE_DHT11;

                                    if (comp_option.equalsIgnoreCase("DHT11")) comp_type = THERMOMETERTYPE_DHT11;
                                    else if (comp_option.equalsIgnoreCase("DHT12")) comp_type = THERMOMETERTYPE_DHT12;
                                    else if (comp_option.equalsIgnoreCase("DHT21")) comp_type = THERMOMETERTYPE_DHT21;
                                    else if (comp_option.equalsIgnoreCase("DHT22")) comp_type = THERMOMETERTYPE_DHT22;
                                    else if (comp_option.equalsIgnoreCase("DS18B20")) comp_type = THERMOMETERTYPE_DS18B20;

                                    NewComponent = new Thermometer(comp_name, Settings.Components.Count() + 1, AvailableComponentBuses.at(comp_bus), comp_address, comp_type);
                                    added = true;
                                } break;

                                default:
                                    break;
                            }

                            if (added) {
                                Settings.Components.Add(NewComponent);
                                changed = true;

                                result += "Components     | New component '" + comp_name + "' added\r\n";
                                result += "               | Class: " + comp_class + "\r\n";
                            } else {
                                result += "Components     | Error while adding component '" + comp_name + "'.\r\n";
                            }
                        }
                    }
                }
            } else {
                result += "Components     | Missing new component parameters.\r\n";
                result += "               | comp add [name] [class] [bus] [address] [options]\r\n";
            }
        } else {
            result += "Components     | Invalid comp parameter.\r\n";
        }

        if (changed) {
            result += "\r\n               | Settings changed - you must reboot to apply changes.\r\n";
            Settings.Save();
        }
        client->write(result.c_str());
    }, true);
}