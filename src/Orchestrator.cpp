#include "Orchestrator.h"

#include "Tools.h"
#include "Version.h"

extern settings_t Settings;

extern Log* devLog;
extern FileSystem* devFileSystem;
extern Network* devNetwork;
extern Clock *devClock;
extern bool g_cmdCheckNow;

Orchestrator& Orchestrator::getInstance() { static Orchestrator instance; return instance; }

const JsonObjectConst Orchestrator::SendUPD(const String &target, const uint16_t port, const JsonObjectConst &payload) {
    s_doc.clear();

    if (WiFi.status() != WL_CONNECTED) return JsonObjectConst(); // vazio

    if (s_sem == nullptr) {
        s_sem = xSemaphoreCreateBinary();
        if (!s_sem) return JsonObjectConst();
    }

    IPAddress ip;
    if (target.equalsIgnoreCase("broadcast")) {
        ip = IPAddress(255, 255, 255, 255);
    } else if (!ip.fromString(target)) {
        if (!WiFi.hostByName(target.c_str(), ip)) return JsonObjectConst();
    }

    s_udp.close();
    if (!s_udp.listen(0)) return JsonObjectConst();

    s_rx = "";
    s_rip = IPAddress();

    s_udp.onPacket([](AsyncUDPPacket p) {
        if (s_rx.isEmpty()) {
            s_rip = p.remoteIP();
            s_rx  = String((const char*)p.data(), p.length());
            xSemaphoreGive(s_sem);
        }
    });

    String out;
    serializeJson(payload, out);

    if (s_udp.writeTo((const uint8_t*)out.c_str(), out.length(), ip, port) == 0) {
        s_udp.close();
        return JsonObjectConst();
    }

    const uint32_t timeout_ms = 1000;
    if (xSemaphoreTake(s_sem, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        s_udp.close();
        return JsonObjectConst();
    }

    DeserializationError err = deserializeJson(s_doc, s_rx.c_str(), s_rx.length());
    if (!err) {
        s_udp.close();
        return s_doc.as<JsonObjectConst>();
    }

    s_doc.clear();
    {
        JsonObject root = s_doc.to<JsonObject>();
        root["Payload"] = s_rx;
        root["Size"]    = (size_t)s_rx.length();

        char ipstr[16];
        snprintf(ipstr, sizeof(ipstr), "%u.%u.%u.%u", s_rip[0], s_rip[1], s_rip[2], s_rip[3]);
        root["From"] = ipstr;
    }

    s_udp.close();
    return s_doc.as<JsonObjectConst>();
}

void Orchestrator::begin() {
    JsonDocument doc;
    doc["Orchestrator"] = "Discover";

    auto reply = SendUPD("255.255.255.255", 30030, doc.as<JsonObjectConst>());
    if (!reply.isNull()) {
        if (Settings.Orchestrator.Assigned()) {
            bool ServerID_match = Settings.Orchestrator.ServerID().equalsIgnoreCase(reply["Orchestrator"]["Server ID"].as<String>());
            bool IPAddress_match = Settings.Orchestrator.IP_Address().toString().equalsIgnoreCase(reply["Orchestrator"]["IP Address"].as<String>());
            bool Port_match = Settings.Orchestrator.Port() == reply["Orchestrator"]["Port"].as<uint16_t>();

            if (ServerID_match && (!IPAddress_match || !Port_match)) {
                Settings.Orchestrator.IP_Address(reply["Orchestrator"]["IP Address"].as<String>());
                Settings.Orchestrator.Port(reply["Orchestrator"]["Port"].as<uint16_t>());
                Settings.Save();

                devLog->Write("Orchestrator: Server IP address updated to " + Settings.Orchestrator.ServerID(), LOGLEVEL_INFO);
            }
        } else {
            Settings.Orchestrator.IP_Address(reply["Orchestrator"]["IP Address"].as<String>());
            Settings.Orchestrator.Port(reply["Orchestrator"]["Port"].as<uint16_t>());
            Settings.Save();
            
            devLog->Write("Orchestrator: Found server at " + Settings.Orchestrator.IP_Address().toString() + ":" + String(Settings.Orchestrator.Port()), LOGLEVEL_INFO);
        }
        
    } else {
        devLog->Write("Orchestrator: Server not found in this network", LOGLEVEL_WARNING);
    }

    if (!udp.listen(30030)) { devLog->Write("Orchestrator: Failed to start UDP listener on port 30030", LOGLEVEL_ERROR); return; }
    udp.onPacket([this](AsyncUDPPacket packet) { this->handleUdpPacket(packet); });
}

void Orchestrator::handleUdpPacket(AsyncUDPPacket& packet) {
    // Debug - print whatever arrives
    // Serial.printf("\r\n---\r\n"); Serial.println((char*)packet.data()); Serial.printf("---\r\n");

    JsonDocument doc;
    if (deserializeJson(doc, packet.data(), packet.length()) != DeserializationError::Ok) {
        return;
    }

    String request = doc["Command"] | "";
    IPAddress senderIp = packet.remoteIP();
    
    if (request == "Discover") { Discover(doc); }
    else if (request == "Restart") { Restart(doc); }
    else if (request == "Restore") { Restore(doc); }
    else if (request == "Refresh") { Refresh(doc); }
    else if (request == "Add") { Add(doc); }
    else if (request == "Remove") { Remove(doc); }
    else if (request == "Update") { Update(doc); }
    else if (request == "Pull") { Pull(doc); }
    else if (request == "Push") { Push(doc); }
    else if (request == "GetLog") { GetLog(doc); }
    else if (request == "ClearLog") { ClearLog(doc); }
    else { devLog->Write("Orchestrator: Unknown request [" + request + "]", LOGLEVEL_WARNING); }
}

bool Orchestrator::CheckOrchestratorAssignedAndServerID(const JsonObjectConst &cmd) {
    if (!Settings.Orchestrator.Assigned()) {
        devLog->Write("Orchestrator: Ignoring command - Device is not assigned", LOGLEVEL_WARNING);
        return false;
    }

    if (!Settings.Orchestrator.ServerID().equals(cmd["Server ID"].as<String>())) {
        devLog->Write("Orchestrator: Ignoring command - Server ID mismatch", LOGLEVEL_WARNING);
        return false;
    }

    return true;
}

bool Orchestrator::FindOrchestratorServer() {

}

bool Orchestrator::Discover(const JsonVariantConst& cmd) {
    if (Settings.Orchestrator.IP_Address().toString().isEmpty()) {
        
    }

    if (connectAndExchangeJson(Settings.Orchestrator.IP_Address(), Settings.Orchestrator.Port(), [&](WiFiClient& client) {
        JsonDocument reply;
        reply["Provider"] = mManagerName;
        reply["Command"] = "Discover";
        reply["Hostname"] = devNetwork->Hostname();
        reply["MAC Address"] = devNetwork->MAC_Address();
        reply["Parameter"]["Server ID"] = Settings.Orchestrator.ServerID();
        reply["Parameter"]["Product Name"] = Version.ProductName;
        reply["Parameter"]["Hardware Model"] = Version.Hardware.Model;
        reply["Parameter"]["Version"] = Version.Software.Info();
        reply["Parameter"]["Hostname"] = devNetwork->Hostname();
        reply["Parameter"]["MAC Address"] = devNetwork->MAC_Address();
        reply["Parameter"]["IP Address"] = devNetwork->IP_Address();
        reply["Parameter"]["Local Timestamp"] = devClock->CurrentDateTime();

        String json;
        serializeJson(reply, json);
        client.print(json);
    })) {
        devLog->Write("Orchestrator: Sent updated device info to " + Settings.Orchestrator.IP_Address().toString() + ":" + String(Settings.Orchestrator.Port()), LOGLEVEL_INFO);
        return true;
    }

    devLog->Write("Orchestrator: Error sending updated device info to " + Settings.Orchestrator.IP_Address().toString() + ":" + String(Settings.Orchestrator.Port()), LOGLEVEL_ERROR);
    return false;   
}

bool Orchestrator::Refresh(const JsonVariantConst& cmd) {
    if (!CheckOrchestratorAssignedAndServerID(cmd)) return false;

    return Discover(cmd);
}

bool Orchestrator::GetLog(const JsonVariantConst& cmd) {
    if (!CheckOrchestratorAssignedAndServerID(cmd)) return false;

    if (devFileSystem->Exists(Defaults.LogFileName)) {
        File f =  devFileSystem->OpenFile(Defaults.LogFileName, "r");
        uint32_t crc = CRC32_File(f);
        size_t fileSize = f.size();

        if (connectAndExchangeJson(Settings.Orchestrator.IP_Address(), Settings.Orchestrator.Port(), [&](WiFiClient& client) {
            StreamFileAsBase64Json(Defaults.LogFileName, devNetwork->MAC_Address(), "GetLog", client, f, fileSize, crc);
        })) {
            devLog->Write("Orchestrator: Sent device log file to " + Settings.Orchestrator.IP_Address().toString() + ":" + String(Settings.Orchestrator.Port()), LOGLEVEL_INFO);
            return true;
        } else {
            devLog->Write("Orchestrator: Error sending log file to " + Settings.Orchestrator.IP_Address().toString() + ":" + String(Settings.Orchestrator.Port()), LOGLEVEL_ERROR);
            return false;
        }
    }
    
    devLog->Write("Orchestrator: File " + String(Defaults.LogFileName) + " not found", LOGLEVEL_ERROR);
    return false;
}

bool Orchestrator::ClearLog(const JsonVariantConst& cmd) {
    if (!CheckOrchestratorAssignedAndServerID(cmd)) return false;

    if (connectAndExchangeJson(Settings.Orchestrator.IP_Address(), Settings.Orchestrator.Port(), [&](WiFiClient& client) {
        JsonDocument reply;
        reply["Provider"] = mManagerName;
        reply["Command"] = "ClearLog";
        reply["Parameter"] = "ACK";
        reply["Hostname"] = devNetwork->Hostname();
        reply["MAC Address"] = devNetwork->MAC_Address();

        String json;
        serializeJson(reply, json);
        client.print(json);
    })) {
        devLog->Write("Orchestrator: Replied ClearLog command to " + Settings.Orchestrator.IP_Address().toString() + ":" + String(Settings.Orchestrator.Port()), LOGLEVEL_INFO);
    } else {
        devLog->Write("Orchestrator: Error sending ClearLog ACK to " + Settings.Orchestrator.IP_Address().toString() + ":" + String(Settings.Orchestrator.Port()), LOGLEVEL_ERROR);
    }

    return devLog->Clear();
}

bool Orchestrator::Pull(const JsonVariantConst& cmd) {
    if (!CheckOrchestratorAssignedAndServerID(cmd)) return false;

    Settings.Save();

    if (devFileSystem->Exists(Defaults.ConfigFileName)) {
        File f =  devFileSystem->OpenFile(Defaults.ConfigFileName, "r");
        uint32_t crc = CRC32_File(f);
        size_t fileSize = f.size();

        if (connectAndExchangeJson(Settings.Orchestrator.IP_Address(), Settings.Orchestrator.Port(), [&](WiFiClient& client) {
            StreamFileAsBase64Json(Defaults.ConfigFileName, devNetwork->MAC_Address(), "Pull", client, f, fileSize, crc);
        })) {
            devLog->Write("Orchestrator: Sent device config file to " + Settings.Orchestrator.IP_Address().toString() + ":" + String(Settings.Orchestrator.Port()), LOGLEVEL_INFO);
            return true;
        } else {
            devLog->Write("Orchestrator: Error sending config file to " + Settings.Orchestrator.IP_Address().toString() + ":" + String(Settings.Orchestrator.Port()), LOGLEVEL_ERROR);
            return false;
        }
    }
    
    devLog->Write("Orchestrator: File " + String(Defaults.ConfigFileName) + " not found", LOGLEVEL_ERROR);
    return false;
}

bool Orchestrator::Push(const JsonVariantConst& cmd) {
    if (!CheckOrchestratorAssignedAndServerID(cmd)) return false;

    fs::File file = devFileSystem->OpenFile(String(Defaults.ConfigFileName) + ".tmp", "w");
    if (!file) {
        devLog->Write("Orchestrator: Failed to open temporary file for writing", LOGLEVEL_ERROR);
        return false;
    }
    if (connectAndExchangeJson(Settings.Orchestrator.IP_Address(), Settings.Orchestrator.Port(), [&](WiFiClient& client) {
        JsonDocument reply;
        reply["Provider"] = mManagerName;
        reply["Command"] = "Push";
        reply["Parameter"] = devNetwork->MAC_Address();
        reply["Hostname"] = devNetwork->Hostname();
        reply["MAC Address"] = devNetwork->MAC_Address();

        String json;
        serializeJson(reply, json);
        client.print(json);

        devLog->Write("Orchestrator: Requested device configuration file to " + Settings.Orchestrator.IP_Address().toString() + ":" + String(Settings.Orchestrator.Port()), LOGLEVEL_INFO);

        const size_t chunkSize = 128;
        uint8_t buffer[chunkSize];
        unsigned long start = millis();
        size_t totalWritten = 0;

        while ((millis() - start) < 5000) {
            int len = client.read(buffer, chunkSize);
            if (len > 0) {
                file.write(buffer, len);
                totalWritten += len;
                start = millis();
            } else {
                delay(1);
            }
        }

        file.close();
        devLog->Write("Orchestrator: Received new config file (" + String(totalWritten) + " bytes)", LOGLEVEL_INFO);

        file = devFileSystem->OpenFile(String(Defaults.ConfigFileName) + ".tmp", "r");
        if (!file) {
            devLog->Write("Orchestrator: Failed to reopen pushed config file for reading", LOGLEVEL_ERROR);
            return;
        }

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, file);
        file.close();

        if (err) {
            devLog->Write("Orchestrator: Failed to parse config JSON from file " + String(err.c_str()), LOGLEVEL_ERROR);
            return;
        }

        bool pushedAssigned = (bool)(doc["Result"]["Orchestrator"]["Assigned"] | false);
        String pushedServerID = String(doc["Result"]["Orchestrator"]["Server ID"] | "");

        if ((pushedAssigned == false) || (Settings.Orchestrator.ServerID().equals(pushedServerID) == false)) {
            devLog->Write("Orchestrator: Invalid configuration - Push configuration not applied", LOGLEVEL_ERROR);
            return;
        }

        if (doc.containsKey("Result")) {
            File f = devFileSystem->OpenFile(Defaults.ConfigFileName, "w");
            if (!f) {
                devLog->Write("Orchestrator: Failed to open file " + String(Defaults.ConfigFileName) + " for writing - Push configuration not applied", LOGLEVEL_ERROR);
                return;
            }
            
            serializeJson(doc["Result"], f);
            f.flush();
            f.close();
            devFileSystem->DeleteFile(String(Defaults.ConfigFileName) + ".tmp");
            devLog->Write("Orchestrator: New config applied and saved - Restarting to apply new settings", LOGLEVEL_INFO);
        }

        Restart(cmd);
    })) {
        return true;
    }

    devLog->Write("Orchestrator: Error requesting configuration file to " + Settings.Orchestrator.IP_Address().toString()+ ":" + String(Settings.Orchestrator.Port()), LOGLEVEL_ERROR);
    return false;
}

bool Orchestrator::Add(const JsonVariantConst& cmd) {
    if (Settings.Orchestrator.Assigned()){
        devLog->Write("Orchestrator: Device already assigned to server ID " + Settings.Orchestrator.ServerID(), LOGLEVEL_ERROR);
        return false;
    }

    if (connectAndExchangeJson(Settings.Orchestrator.IP_Address(), Settings.Orchestrator.Port(), [&](WiFiClient& client) {
        JsonDocument reply;
        reply["Provider"] = mManagerName;
        reply["Command"] = "Add";
        reply["Parameter"] = "ACK";
        reply["MAC Address"] = devNetwork->MAC_Address();
        reply["Hardware Model"] = Version.Hardware.Model;
        reply["Hostname"] = devNetwork->Hostname();
        reply["IP Address"] = devNetwork->IP_Address();
        reply["Local Timestamp"] = devClock->CurrentDateTime();
        reply["Product Name"] = Version.ProductName;
        reply["Version"] = Version.Software.Info();

        String json;
        serializeJson(reply, json);
        client.print(json);
    })) {
        devLog->Write("Orchestrator: Replied Add command to " + Settings.Orchestrator.IP_Address().toString() + ":" + String(Settings.Orchestrator.Port()), LOGLEVEL_INFO);
        Settings.Orchestrator.Assigned(true);
        Settings.Orchestrator.ServerID(cmd["Server ID"]);
        Settings.Save();

        devLog->Write("Orchestrator: Added assignment to server ID " + Settings.Orchestrator.ServerID(), LOGLEVEL_INFO);
        return true;
    }
    
    devLog->Write("Orchestrator: Error sending Add ACK to " + Settings.Orchestrator.IP_Address().toString() + ":" + String(Settings.Orchestrator.Port()), LOGLEVEL_ERROR);
    return false;
}

bool Orchestrator::Restore(const JsonVariantConst& cmd) {
    if (!CheckOrchestratorAssignedAndServerID(cmd)) return false;

    if (connectAndExchangeJson(Settings.Orchestrator.IP_Address(), Settings.Orchestrator.Port(), [&](WiFiClient& client) {
        JsonDocument reply;
        reply["Provider"] = mManagerName;
        reply["Command"] = "Restore";
        reply["Parameter"] = "ACK";
        reply["Hostname"] = devNetwork->Hostname();
        reply["MAC Address"] = devNetwork->MAC_Address();

        String json;
        serializeJson(reply, json);
        client.print(json);

        devLog->Write("Orchestrator: Replied Restore command to " + Settings.Orchestrator.IP_Address().toString() + ":" + String(Settings.Orchestrator.Port()), LOGLEVEL_INFO);
    })) {
        devLog->Write("Orchestrator: Device is restoring to factory defaults", LOGLEVEL_INFO);
        Settings.RestoreToFactoryDefaults();
        return true;
    }
    
    devLog->Write("Orchestrator: Error sending Restore ACK to " + Settings.Orchestrator.IP_Address().toString() + ":" + String(Settings.Orchestrator.Port()), LOGLEVEL_ERROR);
    return false;
}

bool Orchestrator::Remove(const JsonVariantConst& cmd) {
    if (!CheckOrchestratorAssignedAndServerID(cmd)) return false;

    String oldServer = Settings.Orchestrator.ServerID();

    if (connectAndExchangeJson(Settings.Orchestrator.IP_Address(), Settings.Orchestrator.Port(), [&](WiFiClient& client) {
        JsonDocument reply;
        reply["Provider"] = mManagerName;
        reply["Command"] = "Remove";
        reply["Parameter"] = "ACK";
        reply["Hostname"] = devNetwork->Hostname();
        reply["MAC Address"] = devNetwork->MAC_Address();

        String json;
        serializeJson(reply, json);
        client.print(json);

        devLog->Write("Orchestrator: Replied Remove command to " + Settings.Orchestrator.IP_Address().toString() + ":" + String(Settings.Orchestrator.Port()), LOGLEVEL_INFO);

        Settings.Orchestrator.Assigned(false);
        Settings.Orchestrator.ServerID("");
        Settings.Save();
    })) {
        devLog->Write("Orchestrator: Removed assignment to server ID " + oldServer, LOGLEVEL_INFO);
        return true;
    }
    
    devLog->Write("Orchestrator: Error sending Remove ACK to " + Settings.Orchestrator.IP_Address().toString() + ":" + String(Settings.Orchestrator.Port()), LOGLEVEL_ERROR);
    return false;
}

bool Orchestrator::Restart(const JsonVariantConst& cmd) {
    if (!CheckOrchestratorAssignedAndServerID(cmd)) return false;

    if (connectAndExchangeJson(Settings.Orchestrator.IP_Address(), Settings.Orchestrator.Port(), [&](WiFiClient& client) {
        JsonDocument reply;
        reply["Provider"] = mManagerName;
        reply["Command"] = "Restart";
        reply["Parameter"] = "ACK";
        reply["Hostname"] = devNetwork->Hostname();
        reply["MAC Address"] = devNetwork->MAC_Address();

        String json;
        serializeJson(reply, json);
        client.print(json);
    })) {
        devLog->Write("Orchestrator: Replied Restart command to " + Settings.Orchestrator.IP_Address().toString() + ":" + String(Settings.Orchestrator.Port()), LOGLEVEL_INFO);
    } else {
        devLog->Write("Orchestrator: Error sending Restart ACK to " + Settings.Orchestrator.IP_Address().toString() + ":" + String(Settings.Orchestrator.Port()), LOGLEVEL_ERROR);
    }

    Settings.Save();
    esp_sleep_enable_timer_wakeup(200 * 1000);
    esp_deep_sleep_start();

    return true;
}

bool Orchestrator::Update(const JsonVariantConst& cmd) {
    if (!CheckOrchestratorAssignedAndServerID(cmd)) return false;

    if (connectAndExchangeJson(Settings.Orchestrator.IP_Address(), Settings.Orchestrator.Port(), [&](WiFiClient& client) {
        JsonDocument reply;
        reply["Provider"] = mManagerName;
        reply["Command"] = "Update";
        reply["Parameter"] = "ACK";
        reply["Hostname"] = devNetwork->Hostname();
        reply["MAC Address"] = devNetwork->MAC_Address();

        String json;
        serializeJson(reply, json);
        client.print(json);
    })) {
        devLog->Write("Orchestrator: Replied Update command to " + Settings.Orchestrator.IP_Address().toString() + ":" + String(Settings.Orchestrator.Port()), LOGLEVEL_INFO);
        g_cmdCheckNow = true;

        return true;
    }
    
    devLog->Write("Orchestrator: Error sending Update ACK to " + Settings.Orchestrator.IP_Address().toString() + ":" + String(Settings.Orchestrator.Port()), LOGLEVEL_ERROR);
    return false;
}

bool Orchestrator::connectAndExchangeJson(IPAddress remoteIp, uint16_t port, std::function<void(WiFiClient&)> exchange) {
    WiFiClient client;

    client.setTimeout(8000);

    if (!client.connect(remoteIp, port, 8000)) {
        devLog->Write("Orchestrator: Failed to connect to server over TCP", LOGLEVEL_ERROR);
        return false;
    }
    
    exchange(client);
    client.stop();
    return true;
}
