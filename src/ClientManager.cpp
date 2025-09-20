#include <LittleFS.h>
#include "Tools.h"
#include "ClientManager.h"
#include "Version.h"

extern Configuration* devConfiguration;
extern Log* devLog;
extern FileSystem* devFileSystem;
extern Network* devNetwork;
extern Clock *devClock;
extern bool g_cmdCheckNow;

ClientManager& ClientManager::getInstance() { static ClientManager instance; return instance; }

const JsonObjectConst ClientManager::SendUPD(const String &target, const uint16_t port, const JsonObjectConst &payload) {
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

void ClientManager::begin() {
    JsonDocument doc;
    doc["Orchestrator"] = "Discover";

    auto reply = SendUPD("255.255.255.255", 30030, doc.as<JsonObjectConst>());
    if (!reply.isNull()) {
        if (reply["Orchestrator"]["Server ID"].as<String>().equalsIgnoreCase(devConfiguration->Setting["Orchestrator"]["Server ID"].as<String>()) && !reply["Orchestrator"]["IP Address"].as<String>().equalsIgnoreCase(devConfiguration->Setting["Orchestrator"]["IP Address"].as<String>())) {
            devConfiguration->Setting["Orchestrator"]["IP Address"] = reply["Orchestrator"]["IP Address"];
            devConfiguration->Critical();

            devLog->Write("Orchestrator: Server IP address updated to " + devConfiguration->Setting["Orchestrator"]["IP Address"].as<String>(), LOGLEVEL_INFO);
        }
    } else {
        devLog->Write("Orchestrator: Server not found in this network", LOGLEVEL_WARNING);
    }

    if (!udp.listen(30030)) { devLog->Write("Orchestrator: Failed to start UDP listener on port 30030", LOGLEVEL_ERROR); return; }
    udp.onPacket([this](AsyncUDPPacket packet) { this->handleUdpPacket(packet); });
}

void ClientManager::handleUdpPacket(AsyncUDPPacket& packet) {
    JsonDocument doc;
    if (deserializeJson(doc, packet.data(), packet.length()) != DeserializationError::Ok) {
        devLog->Write("Orchestrator: Invalid JSON received over UDP", LOGLEVEL_WARNING);
        return;
    }

    String request = doc["Command"] | "";
    IPAddress senderIp = packet.remoteIP();
    
    if (request == "Discover") { Discover(doc); }
    else if (request == "Restart") { Restart(doc); }
    else if (request == "Refresh") { Refresh(doc); }
    else if (request == "Add") { handleAdd(doc, senderIp); }
    else if (request == "Remove") { handleRemove(doc, senderIp); }
    else if (request == "Update") { Update(doc); }
    else if (request == "Pull") { Pull(doc); }
    else if (request == "Push") { Push(doc); }
    else if (request == "GetLog") { GetLog(doc); }
    else { devLog->Write("Orchestrator: Unknown request [" + request + "]", LOGLEVEL_WARNING); }
}

bool ClientManager::CheckOrchestratorAssignedAndServerID(const JsonObjectConst &cmd) {
    auto orchestrator = devConfiguration->Setting["Orchestrator"];

    if (!orchestrator["Assigned"].is<bool>() || !orchestrator["Assigned"].as<bool>()) {
        devLog->Write("Orchestrator: Ignoring command - Device is not assigned", LOGLEVEL_WARNING);
        return false;
    }

    if (!orchestrator["Server ID"].is<const char*>() || cmd["Server ID"] != orchestrator["Server ID"].as<String>()) {
        devLog->Write("Orchestrator: Ignoring command - Server ID mismatch", LOGLEVEL_WARNING);
        return false;
    }

    return true;
}

bool ClientManager::Discover(const JsonVariantConst& cmd) {
    IPAddress serverip;
    uint16_t serverport = devConfiguration->Setting["Orchestrator"]["Port"].as<uint16_t>();

    // TODO: discover the Orchestrator server IP address!!!

    if (serverip.fromString(devConfiguration->Setting["Orchestrator"]["IP Address"].as<String>())) {
        connectAndExchangeJson(serverip, serverport, [&](WiFiClient& client) {
            JsonDocument reply;
            reply["Provider"] = mManagerName;
            reply["Command"] = "Discover";
            reply["Parameter"]["Server ID"] = devConfiguration->Setting["Orchestrator"]["Server ID"].as<String>();
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

            devLog->Write("Orchestrator: Sent updated device info to " + serverip.toString() + ":" + String(serverport), LOGLEVEL_INFO);
        });
    } else {
        devLog->Write("Orchestrator: Error sending updated device info to " + devConfiguration->Setting["Orchestrator"]["IP Address"].as<String>()+ ":" + String(serverport), LOGLEVEL_ERROR);
        return false;
    }

    return true;
}

bool ClientManager::Refresh(const JsonVariantConst& cmd) {
    if (!CheckOrchestratorAssignedAndServerID(cmd)) return false;

    return Discover(cmd);
}

bool ClientManager::GetLog(const JsonVariantConst& cmd) {
    if (!CheckOrchestratorAssignedAndServerID(cmd)) return false;

    IPAddress serverip;
    uint16_t serverport = devConfiguration->Setting["Orchestrator"]["Port"].as<uint16_t>();
    String LOG_PATH = "/syslog.log";

    if (devFileSystem->Exists(LOG_PATH)) {
        File f =  devFileSystem->OpenFile(LOG_PATH, "r");
        uint32_t crc = CRC32_File(f);
        size_t fileSize = f.size();

        bool ok = false;

        if (serverip.fromString(devConfiguration->Setting["Orchestrator"]["IP Address"].as<String>())) {
            connectAndExchangeJson(serverip, serverport, [&](WiFiClient& client) {
                ok = StreamFileAsBase64Json(LOG_PATH, client, f, fileSize, crc);
                devLog->Write("Orchestrator: Sent device log file to " + serverip.toString() + ":" + String(serverport), LOGLEVEL_INFO);
            });
            return true;
        } else {
            devLog->Write("Orchestrator: Error sending log file to " + devConfiguration->Setting["Orchestrator"]["IP Address"].as<String>()+ ":" + String(serverport), LOGLEVEL_ERROR);
        }
    } else {
        devLog->Write("Orchestrator: File " + LOG_PATH + " not found", LOGLEVEL_ERROR);
    }

    return false;
}

bool ClientManager::Pull(const JsonVariantConst& cmd) {
    if (!CheckOrchestratorAssignedAndServerID(cmd)) return false;

    IPAddress serverip;
    uint16_t serverport = devConfiguration->Setting["Orchestrator"]["Port"].as<uint16_t>();

    if (serverip.fromString(devConfiguration->Setting["Orchestrator"]["IP Address"].as<String>())) {
        connectAndExchangeJson(serverip, serverport, [&](WiFiClient& client) {
            JsonDocument reply;
            reply["Provider"] = mManagerName;
            reply["Command"] = "Pull";
            reply["Parameter"].set(devConfiguration->Setting);

            String json;
            serializeJson(reply, json);
            client.print(json);

            devLog->Write("Orchestrator: Sent device configuration file to " + serverip.toString() + ":" + String(serverport), LOGLEVEL_INFO);
        });
    } else {
        devLog->Write("Orchestrator: Error sending configuration file to " + devConfiguration->Setting["Orchestrator"]["IP Address"].as<String>()+ ":" + String(serverport), LOGLEVEL_ERROR);
        return false;
    }

    return true;
}

bool ClientManager::Push(const JsonVariantConst& cmd) {
    if (!CheckOrchestratorAssignedAndServerID(cmd)) return false;

    fs::File file = devFileSystem->OpenFile("/tmp_config.json", "w");
    if (!file) {
        devLog->Write("Orchestrator: Failed to open temporary file for writing", LOGLEVEL_ERROR);
        return false;
    }

    IPAddress serverip;
    uint16_t serverport = devConfiguration->Setting["Orchestrator"]["Port"].as<uint16_t>();

    if (serverip.fromString(devConfiguration->Setting["Orchestrator"]["IP Address"].as<String>())) {
        connectAndExchangeJson(serverip, serverport, [&](WiFiClient& client) {
            JsonDocument reply;
            reply["Provider"] = mManagerName;
            reply["Command"] = "Push";
            reply["Parameter"] = devNetwork->MAC_Address();

            String json;
            serializeJson(reply, json);
            client.print(json);

            devLog->Write("Orchestrator: Requested device configuration file to " + serverip.toString() + ":" + String(serverport), LOGLEVEL_INFO);

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

            file = devFileSystem->OpenFile("/tmp_config.json", "r");
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

            JsonVariantConst result = doc["Result"];
            if (result.isNull() || !result.is<JsonObjectConst>()) {
                devLog->Write("Orchestrator: Missing or invalid 'Result' field in pushed JSON", LOGLEVEL_ERROR);
                return;
            }

            JsonObjectConst pushed = result.as<JsonObjectConst>();

            JsonObjectConst pushedOrch = pushed["Orchestrator"];
            bool pushedAssigned = pushedOrch["Assigned"] | false;
            String pushedServerID = pushedOrch["Server ID"] | "";

            bool currentAssigned = devConfiguration->Setting["Orchestrator"]["Assigned"] | false;
            String currentServerID = devConfiguration->Setting["Orchestrator"]["Server ID"] | "";

            if (currentAssigned && pushedServerID != currentServerID) {
                devLog->Write("Orchestrator: Rejected pushed config - Server ID mismatch (" + pushedServerID + " vs " + currentServerID + ")", LOGLEVEL_WARNING);
                return;
            }

            devConfiguration->Setting.clear();
            devConfiguration->Setting.set(pushed);
            devConfiguration->Critical();
            devLog->Write("Orchestrator: New config applied and saved", LOGLEVEL_INFO);

            devFileSystem->DeleteFile("/tmp_config.json");

            Restart(cmd);
        });
    } else {
        devLog->Write("Orchestrator: Error reuesting configuration file to " + devConfiguration->Setting["Orchestrator"]["IP Address"].as<String>()+ ":" + String(serverport), LOGLEVEL_ERROR);
        return false;
    }

    return true;
}

void ClientManager::handleAdd(const JsonVariantConst& cmd, IPAddress remoteIp) {
    if (devConfiguration->Setting["Orchestrator"]["Assigned"].as<bool>() == true) {
        devLog->Write("Orchestrator: Add request ignored - Device assigned to server ID " + devConfiguration->Setting["Orchestrator"]["Server ID"].as<String>(), LOGLEVEL_WARNING);
        return;
    }

    DynamicJsonDocument response(2048);
    response["Orchestrator"]["Server"] = remoteIp.toString();
    response["Orchestrator"]["Port"] = cmd["Reply Port"] | 30030;
    response["Orchestrator"]["Status"] = "Added";
    response["DeviceIQ"]["Product Name"] = Version.ProductName;
    response["DeviceIQ"]["Hardware Model"] = Version.Hardware.Model;
    response["DeviceIQ"]["Device Name"] = devConfiguration->Setting["General"]["Device Name"].as<String>();
    response["DeviceIQ"]["Version"] = Version.Software.Info();
    response["DeviceIQ"]["Hostname"] = devNetwork->Hostname();
    response["DeviceIQ"]["MAC Address"] = devNetwork->MAC_Address();
    response["DeviceIQ"]["IP Address"] = devNetwork->IP_Address();

    devConfiguration->Setting["Orchestrator"]["Assigned"] = true;
    devConfiguration->Setting["Orchestrator"]["Server ID"] = cmd["Server ID"].as<String>();
    devConfiguration->Critical();

    connectAndExchangeJson(remoteIp, cmd["Reply Port"] | 30030, [&](WiFiClient& client) {
        String out;
        serializeJson(response, out);
        client.print(out);
    });

    devLog->Write("Orchestrator: Added assignment to server ID " + devConfiguration->Setting["Orchestrator"]["Server ID"].as<String>(), LOGLEVEL_INFO);
}

void ClientManager::handleRemove(const JsonVariantConst& cmd, IPAddress remoteIp) {
    if (!devConfiguration->Setting["Orchestrator"]["Assigned"].as<bool>()) {
        devLog->Write("Orchestrator: Remove ignored - Device is not assigned", LOGLEVEL_WARNING);
        return;
    }

    if (cmd["Server ID"].as<String>() != devConfiguration->Setting["Orchestrator"]["Server ID"].as<String>()) {
        devLog->Write("Orchestrator: Remove ignored - Server ID mismatch", LOGLEVEL_WARNING);
        return;
    }

    if (cmd["MAC Address"].as<String>() != devNetwork->MAC_Address()) {
        devLog->Write("Orchestrator: Remove ignored - MAC address mismatch", LOGLEVEL_WARNING);
        return;
    }

    connectAndExchangeJson(remoteIp, cmd["Reply Port"] | 30030, [&](WiFiClient& client) {
        DynamicJsonDocument reply(512);
        reply["Orchestrator"]["Status"] = "Removed";
        reply["DeviceIQ"]["MAC Address"] = devNetwork->MAC_Address();

        String json;
        serializeJson(reply, json);
        client.print(json);
    });

    String oldServer = devConfiguration->Setting["Orchestrator"]["Server ID"].as<String>();

    devConfiguration->Setting["Orchestrator"]["Assigned"] = false;
    devConfiguration->Setting["Orchestrator"]["Server ID"] = "";
    devConfiguration->Critical();

    devLog->Write("Orchestrator: Removed assignment to server ID " + oldServer, LOGLEVEL_INFO);
}

bool ClientManager::Restart(const JsonVariantConst& cmd) {
    if (devConfiguration->Setting["Orchestrator"]["Assigned"].as<bool>() && cmd["Server ID"] != devConfiguration->Setting["Orchestrator"]["Server ID"].as<String>()) {
        devLog->Write("Orchestrator: Ignoring Refresh command - Server ID mismatch", LOGLEVEL_WARNING);
        return false;
    }

    IPAddress serverip;
    uint16_t serverport = devConfiguration->Setting["Orchestrator"]["Port"].as<uint16_t>();

    if (serverip.fromString(devConfiguration->Setting["Orchestrator"]["IP Address"].as<String>())) {
        connectAndExchangeJson(serverip, serverport, [&](WiFiClient& client) {
            JsonDocument reply;
            reply["Provider"] = mManagerName;
            reply["Command"] = "Restart";
            reply["Parameter"] = "ACK";
            reply["Hostname"] = devNetwork->Hostname();

            String json;
            serializeJson(reply, json);
            client.print(json);

            devLog->Write("Orchestrator: Replied Restart command to " + serverip.toString() + ":" + String(serverport), LOGLEVEL_INFO);
        });
    } else {
        devLog->Write("Orchestrator: Error sending Restart ACK to " + devConfiguration->Setting["Orchestrator"]["IP Address"].as<String>()+ ":" + String(serverport), LOGLEVEL_ERROR);
    }
    
    esp_sleep_enable_timer_wakeup(200 * 1000);
    esp_deep_sleep_start();

    return true;
}

bool ClientManager::Update(const JsonVariantConst& cmd) {
    if (devConfiguration->Setting["Orchestrator"]["Assigned"].as<bool>() && cmd["Server ID"] != devConfiguration->Setting["Orchestrator"]["Server ID"].as<String>()) {
        devLog->Write("Orchestrator: Ignoring Update command - Server ID mismatch", LOGLEVEL_WARNING);
        return false;
    }

    IPAddress serverip;
    uint16_t serverport = devConfiguration->Setting["Orchestrator"]["Port"].as<uint16_t>();

    if (serverip.fromString(devConfiguration->Setting["Orchestrator"]["IP Address"].as<String>())) {
        connectAndExchangeJson(serverip, serverport, [&](WiFiClient& client) {
            JsonDocument reply;
            reply["Provider"] = mManagerName;
            reply["Command"] = "Update";
            reply["Parameter"] = "ACK";
            reply["Hostname"] = devNetwork->Hostname();

            String json;
            serializeJson(reply, json);
            client.print(json);

            devLog->Write("Orchestrator: Replied Update command to " + serverip.toString() + ":" + String(serverport), LOGLEVEL_INFO);
        });
    } else {
        devLog->Write("Orchestrator: Error sending Update ACK to " + devConfiguration->Setting["Orchestrator"]["IP Address"].as<String>()+ ":" + String(serverport), LOGLEVEL_ERROR);
    }
    
    g_cmdCheckNow = true;

    return true;
}

bool ClientManager::connectAndExchangeJson(IPAddress remoteIp, uint16_t port, std::function<void(WiFiClient&)> exchange) {
    WiFiClient client;

    if (!client.connect(remoteIp, port)) {
        devLog->Write("Orchestrator: Failed to connect to server over TCP", LOGLEVEL_ERROR);
        return false;
    }
    
    exchange(client);
    client.stop();
    return true;
}
