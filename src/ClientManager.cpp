#include <LittleFS.h>
#include "ClientManager.h"
#include "Version.h"

extern Configuration* devConfiguration;
extern Log* devLog;
extern FileSystem* devFileSystem;
extern Network* devNetwork;
extern Clock *devClock;
extern bool g_cmdCheckNow;

ClientManager& ClientManager::getInstance() { static ClientManager instance; return instance; }

void ClientManager::begin() {
    if (!udp.listen(30030)) { devLog->Write("Orchestrator: Failed to start UDP listener on port 30030", LOGLEVEL_ERROR); return; }
    udp.onPacket([this](AsyncUDPPacket packet) { this->handleUdpPacket(packet); });
}

void ClientManager::handleUdpPacket(AsyncUDPPacket& packet) {
    StaticJsonDocument<1024> doc;
    if (deserializeJson(doc, packet.data(), packet.length()) != DeserializationError::Ok) {
        devLog->Write("Orchestrator: Invalid JSON received over UDP", LOGLEVEL_WARNING);
        return;
    }

    String request = doc["Command"] | "";
    IPAddress senderIp = packet.remoteIP();
    
    if (request == "Discover") { handleDiscover(doc, senderIp); }
    else if (request == "Restart") { handleRestart(doc, senderIp); }
    else if (request == "Refresh") { handleRefresh(doc, senderIp); }
    else if (request == "Add") { handleAdd(doc, senderIp); }
    else if (request == "Remove") { handleRemove(doc, senderIp); }
    else if (request == "Update") { handleUpdate(doc, senderIp); }
    else if (request == "Pull") { handlePull(doc, senderIp); }
    else if (request == "Push") { handlePush(doc, senderIp); }
    else { devLog->Write("Orchestrator: Unknown request [" + request + "]", LOGLEVEL_WARNING); }
}

bool ClientManager::UpdateOrchestrator() {
    IPAddress serverip;
    uint16_t serverport = devConfiguration->Setting["Orchestrator"]["Port"].as<uint16_t>();

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

void ClientManager::handleDiscover(const JsonVariantConst& cmd, IPAddress remoteIp) {
    String calling = cmd["Parameter"] | "";
    uint16_t replyPort = 30030;

    bool assigned = devConfiguration->Setting["Orchestrator"]["Assigned"].as<bool>();
    DiscoveryMode mode = DISCOVERY_NONE;

    if (calling == "All") mode = DISCOVERY_ALL;
    else if (calling == "Managed" && assigned) mode = DISCOVERY_MANAGED;
    else if (calling == "Unmanaged" && !assigned) mode = DISCOVERY_UNMANAGED;

    if (mode == DISCOVERY_NONE) return;

    connectAndExchangeJson(remoteIp, replyPort, [&](WiFiClient& client) {
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
    });

    devLog->Write("Orchestrator: Replied discovery call to " + remoteIp.toString(), LOGLEVEL_INFO);
}

void ClientManager::handleRestart(const JsonVariantConst& cmd, IPAddress remoteIp) {
    uint16_t replyPort = 30030;

    connectAndExchangeJson(remoteIp, replyPort, [&](WiFiClient& client) {
        JsonDocument reply;
        reply["Provider"] = mManagerName;
        reply["Command"] = "Restart";
        reply["Parameter"] = "ACK";
        reply["Hostname"] = devNetwork->Hostname();

        String json;
        serializeJson(reply, json);
        client.print(json);
    });

    devLog->Write("Orchestrator: Replied Restart command to " + remoteIp.toString(), LOGLEVEL_INFO);
    
    esp_sleep_enable_timer_wakeup(200 * 1000);
    esp_deep_sleep_start();
}

void ClientManager::handleRefresh(const JsonVariantConst& cmd, IPAddress remoteIp) {
    UpdateOrchestrator();
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

void ClientManager::handleUpdate(const JsonVariantConst& cmd, IPAddress remoteIp) {
    if (!devConfiguration->Setting["Orchestrator"]["Assigned"].as<bool>()) {
        devLog->Write("Orchestrator: Update ignored - Device is not assigned", LOGLEVEL_WARNING);
        return;
    }

    if (cmd["Server ID"].as<String>() != devConfiguration->Setting["Orchestrator"]["Server ID"].as<String>()) {
        devLog->Write("Orchestrator: Update ignored - Server ID mismatch", LOGLEVEL_WARNING);
        return;
    }

    if (cmd["MAC Address"].as<String>() != devNetwork->MAC_Address()) {
        devLog->Write("Orchestrator: Update ignored - MAC address mismatch", LOGLEVEL_WARNING);
        return;
    }
    
    devLog->Write("Orchestrator: Received Update command", LOGLEVEL_INFO);
    g_cmdCheckNow = true;
}

void ClientManager::handlePull(const JsonVariantConst& cmd, IPAddress remoteIp) {
    if (!devConfiguration->Setting["Orchestrator"]["Assigned"].as<bool>()) {
        devLog->Write("Orchestrator: Ignoring Pull command - Device is not assigned", LOGLEVEL_WARNING);
        return;
    }

    if (cmd["Server ID"] != devConfiguration->Setting["Orchestrator"]["Server ID"].as<String>()) {
        devLog->Write("Orchestrator: Ignoring Pull command - Server ID mismatch", LOGLEVEL_WARNING);
        return;
    }

    if (cmd["MAC Address"] != devNetwork->MAC_Address()) {
        devLog->Write("Orchestrator: Ignoring Pull command - MAC Address mismatch", LOGLEVEL_WARNING);
        return;
    }

    uint16_t port = cmd["Reply Port"] | 30030;
    connectAndExchangeJson(remoteIp, port, [&](WiFiClient& client) {
        devConfiguration->Setting["Network"]["MAC Address"] = devNetwork->MAC_Address();
        String out;
        serializeJson(devConfiguration->Setting, out);
        client.print(out);
        devLog->Write("Orchestrator: Sent config to " + remoteIp.toString() + ":" + String(port) + " (" + String(out.length()) + " bytes)", LOGLEVEL_INFO);
    });
}

void ClientManager::handlePush(const JsonVariantConst& cmd, IPAddress remoteIp) {
    fs::File file = devFileSystem->OpenFile("/tmp_config.json", "w");
    if (!file) {
        devLog->Write("Orchestrator: Failed to open temporary file for writing", LOGLEVEL_ERROR);
        return;
    }

    connectAndExchangeJson(remoteIp, cmd["Reply Port"] | 30030, [&](WiFiClient& client) {
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
    });

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

    JsonObject pushed = doc.as<JsonObject>();

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

    // Send ACK JSON
    connectAndExchangeJson(remoteIp, cmd["Reply Port"] | 30030, [&](WiFiClient& client) {
        DynamicJsonDocument ackDoc(128);
        ackDoc["Result"] = "Config Applied";

        String ack;
        serializeJson(ackDoc, ack);
        client.print(ack);
        devLog->Write("Orchestrator: Sent JSON ACK to " + ack, LOGLEVEL_INFO);
    });

    devFileSystem->DeleteFile("/tmp_config.json");

    if (cmd["Apply"].as<bool>()) handleRestart(cmd, remoteIp);
}

bool ClientManager::connectAndExchangeJson(IPAddress remoteIp, uint16_t port, std::function<void(WiFiClient&)> exchange) {
    WiFiClient client;

    delay(500);

    if (!client.connect(remoteIp, port)) {
        devLog->Write("Orchestrator: Failed to connect to server over TCP", LOGLEVEL_ERROR);
        return false;
    }
    
    exchange(client);
    client.stop();
    return true;
}
