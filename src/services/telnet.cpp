#include "telnet.h"

void Telnet::Begin() {
    if (Settings.TelnetServer.Enabled() == true) {
        devTelnetServer = new AsyncTelnetServer(Settings.TelnetServer.Port());

        devTelnetServer->WelcomeMessage = ":: " + Version.ProductFamily + " " + Settings.Network.Hostname() + " - Welcome";
        
        devTelnetServer->onSessionBegin = [&](AsyncClient* client, AsyncTelnetSession* session) {
            devLog->Write("Telnet Server: Session started " + String(session->User + "@" + session->RemoteIP.toString() + ":" + session->RemotePort), LOGLEVEL_INFO);
        };
        devTelnetServer->onSessionEnd = [&](AsyncClient* client, AsyncTelnetSession* session) {
            devLog->Write("Telnet Server: Session ended " + String(session->User + "@" + session->RemoteIP.toString() + ":" + session->RemotePort), LOGLEVEL_INFO);
        };

        //Commands
        registerCommand_dumpcfg();
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
        registerCommand_user();
        registerCommand_comp();
        registerCommand_log();
        
        devTelnetServer->begin();
        devLog->Write("Telnet Server: Enabled on port " + String(Settings.TelnetServer.Port()), LOGLEVEL_INFO);
    } else {
        devLog->Write("Telnet Server: Disabled", LOGLEVEL_INFO);
    }
}

void Telnet::registerCommand_dumpcfg(bool admincmd) {
    devTelnetServer->onCommand("dumpcfg", "Prints the configuration file\r\n\r\ndumpcfg", [&](AsyncClient* client, String* parameter) {
        String result;
        const String path = Defaults.ConfigFileName;

        File f = devFileSystem->OpenFile(path, "r");
        if (!f) {
            result += "Config         | Error opening config file '" + path + "'.\r\n";
            client->write(result.c_str());
            return;
        }

        result += "Config         | File: " + path + "\r\n\r\n";

        while (f.available()) {
            String line = f.readStringUntil('\n');
            result += line + "\r\n";

            if (result.length() > 2048) {
                client->write(result.c_str());
                result.clear();
            }
        }

        f.close();

        if (!result.isEmpty()) {
            client->write(result.c_str());
        }
    }, admincmd);
}
void Telnet::registerCommand_logon(bool admincmd) {
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
                }
                break;
            
                default:
                break;
            }
        }
        client->write(result.c_str());
    }, admincmd);
}
void Telnet::registerCommand_reboot(bool admincmd) {
    devTelnetServer->onCommand("reboot", "Reboot the device\r\n\r\nreboot", [&](AsyncClient* client, String* parameter) {
        DeviceRestart();
    }, admincmd);
}
void Telnet::registerCommand_network(bool admincmd) {
    devTelnetServer->onCommand("network", "Show or change network configuration\r\n\r\nnetwork [options]", [&](AsyncClient* client, String* parameter) {
        String result;
        bool changed = false;

        if (parameter[0].isEmpty()) {
            result += "Network        | " + LimitString("SSID: " + devNetwork->SSID(), 30, true) + LimitString(Settings.Network.SSID(), 30, true) + "\r\n";
            result += "               | " + LimitString("PSK: " + devNetwork->Passphrase(), 30, true) + LimitString(Settings.Network.Passphrase(), 30, true) + "\r\n";
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
            } else if (parameter[0].equalsIgnoreCase("scan")) {
                client->write("Network        | Scanning WiFi networks...\r\n");

                if (devNetwork->ConnectionMode() == APMode::WifiClient) client->write("               | WARNING: Device is connected as WiFi client. Scan may disrupt connection or cause reboot\r\n");

                WiFi.scanDelete();
                delay(10);

                int n = WiFi.scanNetworks(false, false);

                if (n < 0) {
                    client->write("Network        | Error scanning WiFi networks.\r\n");
                } else if (n == 0) {
                    client->write("Network        | No networks found.\r\n");
                } else {
                    for (int i = 0; i < n; ++i) {
                        String ssid = WiFi.SSID(i);
                        int32_t rssi = WiFi.RSSI(i);
                        int32_t ch = WiFi.channel(i);
                        wifi_auth_mode_t enc = WiFi.encryptionType(i);

                        String enc_str;
                        switch (enc) {
                            case WIFI_AUTH_OPEN: enc_str = "Open"; break;
                            case WIFI_AUTH_WEP: enc_str = "WEP"; break;
                            case WIFI_AUTH_WPA_PSK: enc_str = "WPA"; break;
                            case WIFI_AUTH_WPA2_PSK: enc_str = "WPA2"; break;
                            case WIFI_AUTH_WPA_WPA2_PSK: enc_str = "WPA/WPA2"; break;
                            case WIFI_AUTH_WPA2_ENTERPRISE: enc_str = "WPA2-ENT"; break;
                            case WIFI_AUTH_WPA3_PSK: enc_str = "WPA3"; break;
                            case WIFI_AUTH_WPA2_WPA3_PSK: enc_str = "WPA2/WPA3"; break;
                            default: enc_str = "?"; break;
                        }

                        String line;
                        line.reserve(128);
                        line += "               | ";
                        line += LimitString("[" + String(i + 1) + "] " + ssid, 30, true);
                        line += " RSSI: " + String(rssi);
                        line += " Ch: " + String(ch);
                        line += " " + enc_str;
                        line += "\r\n";

                        client->write(line.c_str());
                        delay(0);
                    }

                    String line;
                    line = "               | Count: " + String(n) + "\r\n";
                    client->write(line.c_str());
                }

                WiFi.scanDelete();
            } else if (parameter[0].equalsIgnoreCase("wifi")) {
                if (parameter[1].isEmpty()) {
                    result += "Network        | " + LimitString("SSID: " + devNetwork->SSID(), 30, true) + LimitString(Settings.Network.SSID(), 30, true) + "\r\n";
                    result += "               | " + LimitString("PSK: " + devNetwork->Passphrase(), 30, true) + LimitString(Settings.Network.Passphrase(), 30, true) + "\r\n";
                } else {
                    String wifi_param = parameter[1];

                    int sep = wifi_param.indexOf(':');

                    if (sep <= 0 || sep >= (int)wifi_param.length() - 1) {
                        result += "Network        | Invalid format.\r\n";
                        result += "               | Usage: network wifi <ssid>:<password>\r\n";
                        result += "               |        network wifi \"SSID\":\"PASSWORD\"\r\n";
                    } else {
                        String ssid = wifi_param.substring(0, sep);
                        String pass = wifi_param.substring(sep + 1);

                        ssid.trim();
                        pass.trim();

                        // remove aspas se existirem
                        if (ssid.startsWith("\"") && ssid.endsWith("\"") && ssid.length() >= 2) {
                            ssid = ssid.substring(1, ssid.length() - 1);
                        }

                        if (pass.startsWith("\"") && pass.endsWith("\"") && pass.length() >= 2) {
                            pass = pass.substring(1, pass.length() - 1);
                        }

                        if (ssid.isEmpty() || pass.isEmpty()) {
                            result += "Network        | SSID or password cannot be empty.\r\n";
                        } else {
                            bool changed_local = false;

                            if (!Settings.Network.SSID().equals(ssid)) {
                                Settings.Network.SSID(ssid);
                                changed_local = true;
                            }

                            if (!Settings.Network.Passphrase().equals(pass)) {
                                Settings.Network.Passphrase(pass);
                                changed_local = true;
                            }

                            result += "Network        | " + LimitString("SSID: " + devNetwork->SSID(), 30, true) + LimitString(Settings.Network.SSID(), 30, true) + "\r\n";
                            result += "               | " + LimitString("PSK: " + devNetwork->Passphrase(), 30, true) + LimitString(Settings.Network.Passphrase(), 30, true) + "\r\n";

                            if (changed_local) {
                                result += "\r\n               | WiFi credentials updated - reboot required.\r\n";
                                Settings.Save();
                                changed = true;
                            }
                        }
                    }
                }
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
            result += "\r\n               | Settings changed - reboot recommended to apply changes.\r\n";
            Settings.Save();
        }
        client->write(result.c_str());
    }, admincmd);
}
void Telnet::registerCommand_ntp(bool admincmd) {
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
            result += "\r\n               | Settings changed - reboot recommended to apply changes.\r\n";
            Settings.Save();
        }
        client->write(result.c_str());
    }, admincmd);
}
void Telnet::registerCommand_ver(bool admincmd) {
    devTelnetServer->onCommand("ver", "Show device version info\r\n\r\nver", [&](AsyncClient* client, String* parameter) {
        String result;

        result += "Version        | Product: " + Version.ProductName + "\r\n";
        result += "               | Family: " + Version.ProductFamily + "\r\n";
        result += "               | Hardware: " + Version.Hardware.Info() + "\r\n";
        result += "               | Software: " + Version.Software.Info() + "\r\n";

        client->write(result.c_str());
    }, admincmd);
}
void Telnet::registerCommand_memory(bool admincmd) {
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
    }, admincmd);
}
void Telnet::registerCommand_storage(bool admincmd) {
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
    }, admincmd);
}
void Telnet::registerCommand_ping(bool admincmd) {
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
    }, admincmd);
}
void Telnet::registerCommand_telnet(bool admincmd) {
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
            result += "\r\n               | Settings changed - reboot recommended to apply changes.\r\n";
            Settings.Save();
        }
        client->write(result.c_str());
    }, admincmd);
}
void Telnet::registerCommand_webserver(bool admincmd) {
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
            result += "\r\n               | Settings changed - reboot recommended to apply changes.\r\n";
            Settings.Save();
        }
        client->write(result.c_str());
    }, admincmd);
}
void Telnet::registerCommand_mqtt(bool admincmd) {
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
            result += "\r\n               | Settings changed - reboot recommended to apply changes.\r\n";
            Settings.Save();
        }
        client->write(result.c_str());
    }, admincmd);
}
void Telnet::registerCommand_user(bool admincmd) {
    devTelnetServer->onCommand("user", "Manage users\r\n\r\nuser", [&](AsyncClient* client, String* parameter) {
        String result;
        bool changed = false;

        if (parameter[0].equalsIgnoreCase("list")) {
            bool first = true;
            for (auto m : Settings.Users) {
                result += String(first ? "Users          | " : "               | ") + LimitString(m.Username(), 30, true) + String(m.Admin() ? "- Admin" : "") + "\r\n";                
                first = false;
            }

            result += "\r\n               | Max Users: " + String(MAX_USERS) + "\r\n";
            result += "               | Current: " + String(Settings.Users.Count()) + " user(s), " + String(Settings.Users.CountAdmins()) + " admin(s)\r\n";
        } else if (parameter[0].equalsIgnoreCase("remove")) {
            if (!parameter[1].isEmpty()) {
                if (Settings.Users.Remove(parameter[1]) == UserReturn::OK) {
                    result += "Users          | User '" + parameter[1] + "' removed.\r\n";
                    changed = true;
                } else {
                    result += "Users          | Error removing user '" + parameter[1] + "'.\r\n";
                }
            }
        } else if (parameter[0].equalsIgnoreCase("password")) {
            if (!parameter[1].isEmpty() && !parameter[2].isEmpty()) {
                String username = parameter[1];
                String newpassword = parameter[2];

                user_t* user = nullptr;

                if (Settings.Users.Find(username, &user) == UserReturn::OK && user != nullptr) {
                    if (user->SetPassword(newpassword)) {
                        result += "Users          | Password updated for user '" + username + "'.\r\n";
                        changed = true;
                    } else {
                        result += "Users          | Error setting password for user '" + username + "'.\r\n";
                    }
                } else {
                    result += "Users          | User '" + username + "' not found.\r\n";
                }
            } else {
                result += "Users          | Missing parameters.\r\n";
                result += "               | Usage: user password <username> <newpassword>\r\n";
            }
        } else if (parameter[0].equalsIgnoreCase("add")) {
            if (!parameter[1].isEmpty() && !parameter[2].isEmpty()) {
                String username = parameter[1];
                String password = parameter[2];
                bool admin = (!parameter[3].isEmpty() && parameter[3].equalsIgnoreCase("admin"));

                if (Settings.Users.Add(username, password, admin) == UserReturn::OK) {
                    result += "Users          | User '" + username + "' added.\r\n";
                    result += "               | Admin: " + String(admin ? "Yes" : "No") + "\r\n";
                    changed = true;
                } else {
                    result += "Users          | Error adding user '" + username + "'.\r\n";
                }
            } else {
                result += "Users          | Missing parameters.\r\n";
                result += "               | Usage: user add <username> <password> [admin]\r\n";
            }
        } else {
            result += "Users          | Invalid user parameter.\r\n";
        }

        if (changed) {
            result += "\r\n               | Settings changed - reboot recommended to apply changes.\r\n";
            Settings.Save();
        }
        client->write(result.c_str());
    }, admincmd);
}
void Telnet::registerCommand_comp(bool admincmd) {
    devTelnetServer->onCommand("comp", "Manage components\r\n\r\ncomp", [&](AsyncClient* client, String* parameter) {
        String result;
        bool changed = false;

        if (parameter[0].equalsIgnoreCase("set")) {
            if (!parameter[1].isEmpty()) {
                Generic* target = Settings.Components[parameter[1]];

                if (target == nullptr) {
                    result += "Components     | Error finding component '" + parameter[1] + "'.\r\n";
                } else {
                    if (!parameter[2].isEmpty()) {
                        String param = parameter[2];

                        int sep = param.indexOf('=');
                        if (sep <= 0 || sep >= param.length() - 1) {
                            result += "Components     | Invalid format. Use [key]=[value]\r\n";
                        } else {
                            String key = param.substring(0, sep);
                            String value = param.substring(sep + 1);

                            key.trim();
                            value.trim();

                            switch (target->Class()) {
                                case Classes::CLASS_RELAY : {
                                    if (key.equalsIgnoreCase("State")) {
                                        if (value.equalsIgnoreCase("On") || value.equalsIgnoreCase("True") || value.equalsIgnoreCase("Yes") || value.equalsIgnoreCase("1")) {
                                            target->as<Relay>()->State(true);
                                        } else if (value.equalsIgnoreCase("Off") || value.equalsIgnoreCase("False") || value.equalsIgnoreCase("No") || value.equalsIgnoreCase("0")) {
                                            target->as<Relay>()->State(false);
                                        } else if (value.equalsIgnoreCase("~") || value.equalsIgnoreCase("Invert")) {
                                            target->as<Relay>()->Invert();
                                        }
                                        
                                        result += "Components     | " + parameter[1] + " set to " + String(target->as<Relay>()->State() ? "ON" : "OFF") + "\r\n";
                                    } else {
                                        result += "Components     | Unknown key '" + key + "'\r\n";
                                    }
                                } break;
                                case Classes::CLASS_BUTTON : {
                                    if (key.equalsIgnoreCase("Do")) {
                                        bool valid = false;

                                        if (value.equalsIgnoreCase("ClickSingle")) {
                                            target->as<Button>()->Do(ClickTypes::CLICKTYPE_SINGLE);
                                            valid = true;
                                        } else if (value.equalsIgnoreCase("ClickDouble")) {
                                            target->as<Button>()->Do(ClickTypes::CLICKTYPE_DOUBLE);
                                            valid = true;
                                        } else if (value.equalsIgnoreCase("ClickTriple")) {
                                            target->as<Button>()->Do(ClickTypes::CLICKTYPE_TRIPLE);
                                            valid = true;
                                        } else if (value.equalsIgnoreCase("ClickLong")) {
                                            target->as<Button>()->Do(ClickTypes::CLICKTYPE_LONG);
                                            valid = true;
                                        }

                                        if (valid) {
                                            result += "Components     | Sent " + value + " to " + parameter[1] +  + "\r\n";
                                        } else {
                                            result += "Components     | Invalid set '" + value + "' to " + parameter[1] +  + "\r\n";
                                        }
                                    } else {
                                        result += "Components     | Unknown key '" + key + "'\r\n";
                                    }
                                } break;

                                case Classes::CLASS_BLINDS : {
                                    if (key.equalsIgnoreCase("State")) {
                                        if (value.equalsIgnoreCase("Open")) {
                                            target->as<Blinds>()->Open();
                                        } else if (value.equalsIgnoreCase("Close")) {
                                            target->as<Blinds>()->Close();
                                        }
                                        
                                        result += "Components     | " + parameter[1] + " set to " + value + "\r\n";
                                    } else if (key.equalsIgnoreCase("TargetPosition")) {
                                        target->as<Blinds>()->Position(value.toInt());
                                        result += "Components     | " + parameter[1] + " set to " + String(constrain(value.toInt(), 0 ,100)) + "\r\n";
                                    } else {
                                        result += "Components     | Unknown key '" + key + "'\r\n";
                                    }
                                } break;
                                
                                default:
                                    break;
                            }
                        }
                    } else {
                        result += "Components     | Missing component value\r\n               | set [componentname] [value]\r\n";
                    }
                }
            } else {
                result += "Components     | Invalid set parameter\r\n               | set [componentname] [value]\r\n";
            }
        } else if (parameter[0].equalsIgnoreCase("get")) {
            if (!parameter[1].isEmpty()) {
                Generic* target = Settings.Components[parameter[1]];

                if (target == nullptr) {
                    result += "Components     | Error finding component '" + parameter[1] + "'.\r\n";
                } else {
                    result += "Components     | " + parameter[1] + "\r\n";
                    switch (target->Class()) {
                        case Classes::CLASS_RELAY : {
                            result += "               | State: " + String(target->as<Relay>()->State() ? "On" : "Off") + "\r\n";
                        } break;
                        case Classes::CLASS_BLINDS : {
                            result += "               | Target Positon: " + String(target->as<Blinds>()->TargetPosition()) + "\r\n";
                            result += "               | Current Positon: " + String(target->as<Blinds>()->CurrentPosition()) + "\r\n";
                            result += "               | State: " + String(target->as<Blinds>()->State() == BLINDSSTATE_DECREASING ? "Closing" : (target->as<Blinds>()->State() == BLINDSSTATE_INCREASING ? "Opening" : "Stopped")) + "\r\n";
                            result += "               | Step Ms: " + String(target->as<Blinds>()->StepMs()) + "\r\n";
                            result += "               | Open Accelerator: " + String(target->as<Blinds>()->OpenAccel()) + "\r\n";
                            result += "               | Close Accelerator: " + String(target->as<Blinds>()->CloseAccel()) + "\r\n";
                            result += "               | Calibration Multiplier: " + String(target->as<Blinds>()->CalibrationMultiplier()) + "\r\n";
                        } break;
                    }
                    result += "               | Enabled: " + String(target->Enabled() ? "Yes" : "No") + "\r\n";
                }
            } else {
                result += "Components     | Invalid set parameter\r\n               | get [componentname]\r\n";
            }
        } else if (parameter[0].equalsIgnoreCase("event")) {
            if (parameter[1].isEmpty()) {
                result += "Components     | Missing component name\r\n";
                result += "               | event [componentname]\r\n";
                result += "               | event [componentname] [eventname] '[script]'\r\n";
            } else {
                Generic* target = Settings.Components[parameter[1]];

                if (target == nullptr) {
                    result += "Components     | Error finding component '" + parameter[1] + "'.\r\n";
                } else {
                    // =========================
                    // LISTAR EVENTOS
                    // =========================
                    if (parameter[2].isEmpty()) {
                        File f = devFileSystem->OpenFile(Defaults.ConfigFileName, "r");
                        if (!f || !f.available()) {
                            if (f) f.close();
                            result += "Components     | Error opening configuration file.\r\n";
                        } else {
                            JsonDocument doc;
                            DeserializationError err = deserializeJson(doc, f);
                            f.close();

                            if (err) {
                                result += "Components     | Error parsing configuration file.\r\n";
                            } else {
                                JsonArray components = doc["Components"].as<JsonArray>();
                                if (components.isNull()) {
                                    result += "Components     | No components found in configuration.\r\n";
                                } else {
                                    bool found = false;

                                    for (JsonObject comp : components) {
                                        String compName = String(comp["Name"] | "");
                                        if (!compName.equalsIgnoreCase(parameter[1])) continue;

                                        found = true;
                                        result += "Components     | Events for '" + compName + "'\r\n\r\n";

                                        JsonObject events = comp["Events"].as<JsonObject>();
                                        if (events.isNull() || events.size() == 0) {
                                            result += "               | No events assigned.\r\n";
                                        } else {
                                            for (JsonPair kv : events) {
                                                String eventName = String(kv.key().c_str());
                                                String eventValue;

                                                if (kv.value().is<const char*>()) {
                                                    eventValue = String(kv.value().as<const char*>());
                                                } else {
                                                    serializeJson(kv.value(), eventValue);
                                                }

                                                result += "               | " + eventName + " = " + eventValue + "\r\n";
                                            }
                                        }

                                        break;
                                    }

                                    if (!found) {
                                        result += "Components     | Component '" + parameter[1] + "' was instantiated but not found in config.\r\n";
                                    }
                                }
                            }
                        }
                    }
                    // =========================
                    // SETAR / LIMPAR EVENTO
                    // =========================
                    else {
                        const String eventName = parameter[2];

                        auto itEvent = target->Event.find(eventName);
                        if (itEvent == target->Event.end()) {
                            result += "Components     | Event '" + eventName + "' is not valid for component '" + parameter[1] + "'.\r\n";

                            if (!target->Event.empty()) {
                                result += "               | Valid events: ";
                                bool first = true;
                                for (const auto& ev : target->Event) {
                                    if (!first) result += ", ";
                                    result += ev.first;
                                    first = false;
                                }
                                result += "\r\n";
                            } else {
                                result += "               | This component does not expose configurable events.\r\n";
                            }
                        } else {
                            File f = devFileSystem->OpenFile(Defaults.ConfigFileName, "r");
                            if (!f || !f.available()) {
                                if (f) f.close();
                                result += "Components     | Error opening configuration file.\r\n";
                            } else {
                                JsonDocument doc;
                                DeserializationError err = deserializeJson(doc, f);
                                f.close();

                                if (err) {
                                    result += "Components     | Error parsing configuration file.\r\n";
                                } else {
                                    JsonArray components = doc["Components"].as<JsonArray>();
                                    if (components.isNull()) {
                                        result += "Components     | No components found in configuration.\r\n";
                                    } else {
                                        bool found = false;

                                        for (JsonObject comp : components) {
                                            String compName = String(comp["Name"] | "");
                                            if (!compName.equalsIgnoreCase(parameter[1])) continue;

                                            found = true;

                                            JsonObject events = comp["Events"].as<JsonObject>();
                                            if (events.isNull()) {
                                                events = comp.createNestedObject("Events");
                                            }

                                            // =========================
                                            // RECONSTRUIR SCRIPT
                                            // =========================
                                            String script;
                                            for (int i = 3; !parameter[i].isEmpty(); ++i) {
                                                if (!script.isEmpty()) script += " ";
                                                script += parameter[i];
                                            }

                                            script.trim();

                                            // remove aspas externas
                                            if (script.length() >= 2 && script.startsWith("'") && script.endsWith("'")) {
                                                script = script.substring(1, script.length() - 1);
                                            } else if (script.length() >= 2 && script.startsWith("\"") && script.endsWith("\"")) {
                                                script = script.substring(1, script.length() - 1);
                                            }

                                            // =========================
                                            // SET / CLEAR
                                            // =========================
                                            if (script.isEmpty()) {
                                                events.remove(eventName);
                                                result += "Components     | Event '" + eventName + "' cleared for component '" + compName + "'.\r\n";
                                            } else {
                                                events[eventName] = script;
                                                result += "Components     | Event '" + eventName + "' set for component '" + compName + "'.\r\n";
                                            }

                                            // =========================
                                            // SALVAR JSON
                                            // =========================
                                            File wf = devFileSystem->OpenFile(Defaults.ConfigFileName, "w");
                                            if (!wf) {
                                                result += "               | Error saving configuration.\r\n";
                                            } else {
                                                size_t written = serializeJsonPretty(doc, wf);
                                                wf.close();

                                                if (written == 0) {
                                                    result += "               | Error saving configuration.\r\n";
                                                } else {
                                                    result += "               | Configuration saved.\r\n";
                                                }
                                            }

                                            break;
                                        }

                                        if (!found) {
                                            result += "Components     | Component '" + parameter[1] + "' was instantiated but not found in config.\r\n";
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } else if (parameter[0].equalsIgnoreCase("list")) {
            result += "Components     | Listing total of " + String(Settings.Components.Count()) + " component(s)\r\n";

            uint8_t mComponent_Count = 0;
            for (auto mClass : AvailableComponentClasses) {
                mComponent_Count = 0;
                bool first = true;

                for (auto* mComponent : Settings.Components) {
                    if (mComponent == nullptr) continue;
                    if (mComponent->Class() != mClass.second) continue;

                    result += "\r\n" + (first ? LimitString(mClass.first, 15, true) : "               ") + "| ";
                    result += "[" + String(mComponent->ID()) + "] " + mComponent->Name() + " - ";

                    if (mComponent->IsVirtual()) {
                        // Tratamento específico por classe virtual
                        switch (mComponent->Class()) {
                            case CLASS_BLINDS: {
                                auto* b = mComponent->as<Blinds>();
                                result += "Group:";

                                result += (b->RelayUp() != nullptr) ? b->RelayUp()->Name() : "?";
                                result += ",";
                                result += (b->RelayDown() != nullptr) ? b->RelayDown()->Name() : "?";
                            } break;

                            default: {
                                result += "virtual";
                            } break;
                        }
                    } else {
                        result += BusToString(mComponent->Bus()) + ":" + String(mComponent->Address());
                    }

                    if (!mComponent->Enabled()) result += " (Disabled)";

                    first = false;
                    mComponent_Count++;
                }

                if (mComponent_Count > 0) {
                    result += "\r\n               | Count: " + String(mComponent_Count) + "\r\n";
                }
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
        }else if (parameter[0].equalsIgnoreCase("remove")) {
            if (!parameter[1].isEmpty()) {
                String comp_name = parameter[1];
                Generic* target = Settings.Components[comp_name];

                if (target == nullptr) {
                    result += "Components     | Error removing component '" + comp_name + "'.\r\n";
                } else {
                    bool in_use_by_virtual = false;
                    String used_by;

                    // Só precisa proteger componente real sendo usado por virtual
                    if (!target->IsVirtual()) {
                        for (auto* existing : Settings.Components) {
                            if (existing == nullptr) continue;
                            if (!existing->IsVirtual()) continue;
                            if (existing == target) continue;

                            switch (existing->Class()) {
                                case CLASS_BLINDS: {
                                    Blinds* b = existing->as<Blinds>();
                                    if (b == nullptr) continue;

                                    if (b->RelayUp() == target || b->RelayDown() == target) {
                                        in_use_by_virtual = true;
                                        used_by = existing->Name();
                                    }
                                } break;

                                default:
                                    break;
                            }

                            if (in_use_by_virtual) break;
                        }
                    }

                    if (in_use_by_virtual) {
                        result += "Components     | Component '" + comp_name + "' is in use by virtual component '" + used_by + "'.\r\n";
                    } else {
                        if (Settings.Components.Remove(comp_name) != -1) {
                            result += "Components     | Component '" + comp_name + "' removed.\r\n";
                            changed = true;
                        } else {
                            result += "Components     | Error removing component '" + comp_name + "'.\r\n";
                        }
                    }
                }
            }
        } else if (parameter[0].equalsIgnoreCase("add")) {
            if (!parameter[1].isEmpty() && !parameter[2].isEmpty()) {
                String comp_name = parameter[1];

                if (Settings.Components.IndexOf(comp_name) != -1) {
                    result += "Components     | Component '" + comp_name + "' already exists.\r\n";
                } else {
                    String comp_target = parameter[2];

                    int at = comp_target.indexOf('@');
                    int sep = comp_target.indexOf(':');

                    if (at <= 0 || sep <= (at + 1) || sep >= (int)comp_target.length() - 1) {
                        result += "Components     | Invalid component target '" + comp_target + "'.\r\n";
                        result += "               | Real component: comp add <name> <class>@<bus>:<address> [option] [enabled]\r\n";
                        result += "               | Virtual component: comp add <name> Blinds@Group:<relay_up>,<relay_down> [enabled]\r\n";
                    } else {
                        String comp_class = comp_target.substring(0, at);
                        String comp_bus = comp_target.substring(at + 1, sep);
                        String comp_addr_str = comp_target.substring(sep + 1);

                        auto it_class = AvailableComponentClasses.find(comp_class);
                        if (it_class == AvailableComponentClasses.end()) {
                            result += "Components     | Invalid class '" + comp_class + "'.\r\n";
                        } else {
                            auto it_bus = AvailableComponentBuses.find(comp_bus);
                            if (it_bus == AvailableComponentBuses.end()) {
                                result += "Components     | Invalid bus '" + comp_bus + "'.\r\n";
                            } else {
                                Classes c = it_class->second;
                                Buses bus_type = it_bus->second;

                                if (c == CLASS_BLINDS) {
                                    if (bus_type != BUS_GROUP) {
                                        result += "Components     | Class 'Blinds' only supports bus 'Group'.\r\n";
                                        result += "               | Usage: comp add <name> Blinds@Group:<relay_up>,<relay_down> <StepMs> [OpenAccel] [CloseAccel] [CalibrationMultiplier] [enabled]\r\n";
                                    } else {
                                        String step_ms_str = parameter[3];

                                        if (step_ms_str.isEmpty()) {
                                            result += "Components     | Missing Blinds StepMs parameter.\r\n";
                                            result += "               | Usage: comp add <name> Blinds@Group:<relay_up>,<relay_down> <StepMs> [OpenAccel] [CloseAccel] [CalibrationMultiplier] [enabled]\r\n";
                                        } else {
                                            bool valid_stepms = true;
                                            for (size_t i = 0; i < step_ms_str.length(); ++i) {
                                                if (!isDigit(step_ms_str[i])) {
                                                    valid_stepms = false;
                                                    break;
                                                }
                                            }

                                            if (!valid_stepms) {
                                                result += "Components     | Invalid StepMs '" + step_ms_str + "'.\r\n";
                                            } else {
                                                uint16_t comp_step_ms = (uint16_t)step_ms_str.toInt();

                                                float comp_open_accel = parameter[4].isEmpty() ? 0.0f : parameter[4].toFloat();
                                                float comp_close_accel = parameter[5].isEmpty() ? 0.0f : parameter[5].toFloat();
                                                uint8_t comp_calibration_multiplier = parameter[6].isEmpty() ? 3 : (uint8_t)parameter[6].toInt();
                                                bool comp_enabled = parameter[7].isEmpty() ? true : parameter[7].equalsIgnoreCase("true");

                                                int comma = comp_addr_str.indexOf(',');

                                                if (comma <= 0 || comma >= (int)comp_addr_str.length() - 1 || comp_addr_str.indexOf(',', comma + 1) != -1) {
                                                    result += "Components     | Invalid Blinds group definition.\r\n";
                                                    result += "               | Usage: comp add <name> Blinds@Group:<relay_up>,<relay_down> <StepMs> [OpenAccel] [CloseAccel] [CalibrationMultiplier] [enabled]\r\n";
                                                } else {
                                                    String relay_up_name = comp_addr_str.substring(0, comma);
                                                    String relay_down_name = comp_addr_str.substring(comma + 1);

                                                    relay_up_name.trim();
                                                    relay_down_name.trim();

                                                    if (relay_up_name.isEmpty() || relay_down_name.isEmpty()) {
                                                        result += "Components     | Invalid Blinds group definition.\r\n";
                                                        result += "               | Usage: comp add <name> Blinds@Group:<relay_up>,<relay_down> <StepMs> [OpenAccel] [CloseAccel] [CalibrationMultiplier] [enabled]\r\n";
                                                    } else if (relay_up_name.equalsIgnoreCase(relay_down_name)) {
                                                        result += "Components     | Blinds group contains repeated components.\r\n";
                                                    } else {
                                                        Generic* relay_up_generic = nullptr;
                                                        Generic* relay_down_generic = nullptr;

                                                        int idx = Settings.Components.IndexOf(relay_up_name);
                                                        if (idx != -1) relay_up_generic = Settings.Components[idx];

                                                        idx = Settings.Components.IndexOf(relay_down_name);
                                                        if (idx != -1) relay_down_generic = Settings.Components[idx];

                                                        if (relay_up_generic == nullptr) {
                                                            result += "Components     | Component '" + relay_up_name + "' not found.\r\n";
                                                        } else if (relay_down_generic == nullptr) {
                                                            result += "Components     | Component '" + relay_down_name + "' not found.\r\n";
                                                        } else if (relay_up_generic->Class() != CLASS_RELAY) {
                                                            result += "Components     | Component '" + relay_up_name + "' must be a Relay.\r\n";
                                                        } else if (relay_down_generic->Class() != CLASS_RELAY) {
                                                            result += "Components     | Component '" + relay_down_name + "' must be a Relay.\r\n";
                                                        } else {
                                                            Relay* relay_up = (Relay*)relay_up_generic;
                                                            Relay* relay_down = (Relay*)relay_down_generic;

                                                            bool relay_pair_in_use = false;

                                                            for (size_t i = 0; i < Settings.Components.Count(); ++i) {
                                                                Generic* existing = Settings.Components[i];
                                                                if (existing == nullptr) continue;
                                                                if (existing->Class() != CLASS_BLINDS) continue;

                                                                Blinds* existing_blinds = (Blinds*)existing;

                                                                if (existing_blinds->RelayUp() == relay_up &&
                                                                    existing_blinds->RelayDown() == relay_down) {
                                                                    relay_pair_in_use = true;
                                                                    break;
                                                                }
                                                            }

                                                            if (relay_pair_in_use) {
                                                                result += "Components     | A Blinds component with relays '" + relay_up_name + "' and '" + relay_down_name + "' already exists.\r\n";
                                                            } else {
                                                                Blinds* NewComponent = new Blinds(
                                                                    comp_name,
                                                                    Settings.Components.Count() + 1,
                                                                    relay_up,
                                                                    relay_down
                                                                );

                                                                NewComponent->StepMs(comp_step_ms);
                                                                NewComponent->OpenAccel(comp_open_accel);
                                                                NewComponent->CloseAccel(comp_close_accel);
                                                                NewComponent->CalibrationMultiplier(comp_calibration_multiplier);

                                                                if (Settings.Components.Add(NewComponent)) {
                                                                    NewComponent->Enabled(comp_enabled);
                                                                    changed = true;

                                                                    result += "Components     | New component '" + comp_name + "' added\r\n";
                                                                    result += "               | Class: " + comp_class + "\r\n";
                                                                    result += "               | Bus: " + comp_bus + "\r\n";
                                                                    result += "               | RelayUp: " + relay_up_name + "\r\n";
                                                                    result += "               | RelayDown: " + relay_down_name + "\r\n";
                                                                    result += "               | StepMs: " + String(NewComponent->StepMs()) + "\r\n";
                                                                    result += "               | OpenAccel: " + String(NewComponent->OpenAccel(), 2) + "\r\n";
                                                                    result += "               | CloseAccel: " + String(NewComponent->CloseAccel(), 2) + "\r\n";
                                                                    result += "               | CalibrationMultiplier: " + String(NewComponent->CalibrationMultiplier()) + "\r\n";
                                                                    result += "               | Enabled: " + String(comp_enabled ? "true" : "false") + "\r\n";
                                                                } else {
                                                                    delete NewComponent;
                                                                    result += "Components     | Error while adding component '" + comp_name + "'.\r\n";
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    String comp_option = parameter[3];
                                    bool comp_enabled = parameter[4].isEmpty() ? true : parameter[4].equalsIgnoreCase("true");

                                    bool valid_number = true;
                                    for (size_t i = 0; i < comp_addr_str.length(); ++i) {
                                        if (!isDigit(comp_addr_str[i])) {
                                            valid_number = false;
                                            break;
                                        }
                                    }

                                    if (comp_addr_str.isEmpty() || !valid_number) {
                                        result += "Components     | Invalid address '" + comp_addr_str + "'.\r\n";
                                    } else {
                                        uint8_t comp_address = (uint8_t)comp_addr_str.toInt();

                                        bool busaddr_in_use = false;
                                        for (size_t i = 0; i < Settings.Components.Count(); ++i) {
                                            Generic* existing = Settings.Components[i];
                                            if (existing == nullptr) continue;

                                            switch (existing->Class()) {
                                                case CLASS_GENERIC:
                                                case CLASS_BLINDS:
                                                    break;

                                                default:
                                                    if ((existing->Bus() == bus_type) && (existing->Address() == comp_address)) {
                                                        busaddr_in_use = true;
                                                    }
                                                    break;
                                            }

                                            if (busaddr_in_use) break;
                                        }

                                        if (busaddr_in_use) {
                                            result += "Components     | Bus/address '" + comp_bus + ":" + String(comp_address) + "' already in use.\r\n";
                                        } else {
                                            bool added = false;
                                            Generic* NewComponent = nullptr;

                                            switch (c) {
                                                case CLASS_BUTTON: {
                                                    NewComponent = new Button(
                                                        comp_name,
                                                        Settings.Components.Count() + 1,
                                                        bus_type,
                                                        comp_address,
                                                        comp_option.equalsIgnoreCase("EdgesOnly")
                                                            ? ButtonReportModes::BUTTONREPORTMODE_EDGESONLY
                                                            : ButtonReportModes::BUTTONREPORTMODE_CLICKSONLY
                                                    );
                                                    added = true;
                                                } break;

                                                case CLASS_CONTACTSENSOR: {
                                                    NewComponent = new ContactSensor(
                                                        comp_name,
                                                        Settings.Components.Count() + 1,
                                                        bus_type,
                                                        comp_address,
                                                        comp_option.equalsIgnoreCase("Invert")
                                                    );
                                                    added = true;
                                                } break;

                                                case CLASS_CURRENTMETER: {
                                                    NewComponent = new Currentmeter(
                                                        comp_name,
                                                        Settings.Components.Count() + 1,
                                                        bus_type,
                                                        comp_address
                                                    );
                                                    added = true;
                                                } break;

                                                case CLASS_DOORBELL: {
                                                    NewComponent = new Doorbell(
                                                        comp_name,
                                                        Settings.Components.Count() + 1,
                                                        bus_type,
                                                        comp_address
                                                    );
                                                    added = true;
                                                } break;

                                                case CLASS_PIR: {
                                                    NewComponent = new PIR(
                                                        comp_name,
                                                        Settings.Components.Count() + 1,
                                                        bus_type,
                                                        comp_address
                                                    );
                                                    added = true;
                                                } break;

                                                case CLASS_RELAY: {
                                                    NewComponent = new Relay(
                                                        comp_name,
                                                        Settings.Components.Count() + 1,
                                                        bus_type,
                                                        comp_address,
                                                        comp_option.equalsIgnoreCase("NormallyOpened")
                                                            ? RelayTypes::RELAYTYPE_NORMALLYOPENED
                                                            : RelayTypes::RELAYTYPE_NORMALLYCLOSED
                                                    );
                                                    added = true;
                                                } break;

                                                case CLASS_THERMOMETER: {
                                                    ThermometerTypes comp_type = THERMOMETERTYPE_DHT11;

                                                    if (comp_option.equalsIgnoreCase("DHT11")) comp_type = THERMOMETERTYPE_DHT11;
                                                    else if (comp_option.equalsIgnoreCase("DHT12")) comp_type = THERMOMETERTYPE_DHT12;
                                                    else if (comp_option.equalsIgnoreCase("DHT21")) comp_type = THERMOMETERTYPE_DHT21;
                                                    else if (comp_option.equalsIgnoreCase("DHT22")) comp_type = THERMOMETERTYPE_DHT22;
                                                    else if (comp_option.equalsIgnoreCase("DS18B20")) comp_type = THERMOMETERTYPE_DS18B20;

                                                    NewComponent = new Thermometer(
                                                        comp_name,
                                                        Settings.Components.Count() + 1,
                                                        bus_type,
                                                        comp_address,
                                                        comp_type
                                                    );
                                                    added = true;
                                                } break;

                                                default:
                                                    break;
                                            }

                                            if (added) {
                                                if (Settings.Components.Add(NewComponent)) {
                                                    NewComponent->Enabled(comp_enabled);
                                                    changed = true;

                                                    result += "Components     | New component '" + comp_name + "' added\r\n";
                                                    result += "               | Class: " + comp_class + "\r\n";
                                                    result += "               | Bus: " + comp_bus + "\r\n";
                                                    result += "               | Address: " + String(comp_address) + "\r\n";
                                                    if (!comp_option.isEmpty())
                                                        result += "               | Option: " + comp_option + "\r\n";
                                                    result += "               | Enabled: " + String(comp_enabled ? "true" : "false") + "\r\n";
                                                } else {
                                                    delete NewComponent;
                                                    result += "Components     | Error while adding component '" + comp_name + "'.\r\n";
                                                }
                                            } else {
                                                result += "Components     | Error while creating component '" + comp_name + "'.\r\n";
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            } else {
                result += "Components     | Missing new component parameters.\r\n";
                result += "               | Real component: comp add <name> <class>@<bus>:<address> [option] [enabled]\r\n";
                result += "               | Virtual component: comp add <name> Blinds@Group:<relay_up>,<relay_down> [enabled]\r\n";
            }
        } else {
            result += "Components     | Invalid comp parameter.\r\n";
        }

        if (changed) {
            result += "\r\n               | Settings changed - reboot recommended to apply changes.\r\n";
            Settings.Save();
        }
        client->write(result.c_str());
    }, admincmd);
}
void Telnet::registerCommand_log(bool admincmd) {
    devTelnetServer->onCommand("log", "Show/clear log file\r\n\r\nlog [nlines][level|clear]", [&](AsyncClient* client, String* parameter) {

        auto writeSafe = [&](const String& s) {
            if (client == nullptr) return;
            client->write(s.c_str());
            yield();
        };

        auto isValidLevel = [&](const String& s) -> bool {
            if (s.length() != 1) return false;
            char c = toupper(s[0]);
            return (c == 'E' || c == 'W' || c == 'I' || c == 'D');
        };

        size_t nlines = Defaults.Log.ShowMaxLines;
        char levelFilter = '\0';

        if (!parameter[0].isEmpty()) {
            if (parameter[0].equalsIgnoreCase("clear")) {
                devLog->Clear();
                writeSafe("Log            | All log entries were cleared: " + String(Defaults.LogFileName) + "\r\n\r\n");
                return;
            }

            if (IsNumber(parameter[0])) {
                nlines = (size_t)parameter[0].toInt();

                if (nlines == 0) {
                    writeSafe("Log            | Invalid number of lines.\r\n");
                    return;
                }

                if (nlines > 100) {
                    nlines = 100;
                }

                if (!parameter[1].isEmpty()) {
                    if (!isValidLevel(parameter[1])) {
                        writeSafe("Log            | Invalid level filter.\r\n");
                        writeSafe("               | Valid filters: E, W, I, D\r\n");
                        writeSafe("               | Usage: log [nlines] [level|clear]\r\n");
                        return;
                    }

                    levelFilter = toupper(parameter[1][0]);
                }
            } else if (isValidLevel(parameter[0])) {
                levelFilter = toupper(parameter[0][0]);

                if (!parameter[1].isEmpty()) {
                    if (IsNumber(parameter[1])) {
                        nlines = (size_t)parameter[1].toInt();

                        if (nlines == 0) {
                            writeSafe("Log            | Invalid number of lines.\r\n");
                            return;
                        }

                        if (nlines > 100) {
                            nlines = 100;
                        }
                    } else {
                        writeSafe("Log            | Invalid parameter.\r\n");
                        writeSafe("               | Usage: log [nlines] [level|clear]\r\n");
                        return;
                    }
                }
            } else {
                writeSafe("Log            | Invalid parameter.\r\n");
                writeSafe("               | Usage: log [nlines] [level|clear]\r\n");
                writeSafe("               | Levels: E, W, I, D\r\n");
                return;
            }
        }

        File f = devFileSystem->OpenFile(Defaults.LogFileName, "r");
        if (!f) {
            writeSafe("Log            | Error opening log file '" + String(Defaults.LogFileName) + "'.\r\n");
            return;
        }

        const size_t fileSize = f.size();
        if (fileSize == 0) {
            f.close();
            writeSafe("Log            | Log file is empty: " + String(Defaults.LogFileName) + "\r\n");
            return;
        }

        String* lines = new String[nlines];
        if (lines == nullptr) {
            f.close();
            writeSafe("Log            | Not enough memory.\r\n");
            return;
        }

        size_t count = 0;
        String current;
        current.reserve(256);

        int32_t pos = (int32_t)fileSize - 1;

        auto matchesLevel = [&](const String& line) -> bool {
            if (levelFilter == '\0') return true;
            return (line.length() > 1 && toupper(line[1]) == levelFilter);
        };

        while (pos >= 0 && count < nlines) {
            f.seek(pos, SeekSet);
            int c = f.read();

            if (c < 0) break;

            if (c == '\n') {
                if (!current.isEmpty()) {
                    String ready;
                    ready.reserve(current.length());

                    for (int i = (int)current.length() - 1; i >= 0; --i) {
                        ready += current[i];
                    }

                    ready.trim();

                    if (matchesLevel(ready)) {
                        lines[count++] = ready;
                    }

                    current.clear();
                }
            } else if (c != '\r') {
                current += (char)c;
            }

            pos--;

            if ((pos & 0x1FF) == 0) {
                yield();
            }
        }

        if (!current.isEmpty() && count < nlines) {
            String ready;
            ready.reserve(current.length());

            for (int i = (int)current.length() - 1; i >= 0; --i) {
                ready += current[i];
            }

            ready.trim();

            if (matchesLevel(ready)) {
                lines[count++] = ready;
            }
        }

        f.close();

        String header = "Log            | Showing last " + String(count) + " log entr" + String(count == 1 ? "y" : "ies");
        if (levelFilter != '\0') {
            header += " [" + String(levelFilter) + "]";
        }
        header += ": " + String(Defaults.LogFileName) + "\r\n\r\n";

        writeSafe(header);

        for (int i = (int)count - 1; i >= 0; --i) {
            writeSafe(lines[i] + "\r\n");
        }

        delete[] lines;

    }, admincmd);
}