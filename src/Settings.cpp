#include "Settings.h"

void settings_t::network_t::Hostname(String value) noexcept {
    value.trim();
    value.toLowerCase();
    value.replace(" ", "-");

    for (size_t i = 0; i < value.length(); ++i) {
        char c = value.charAt(i);
        bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '-');
        if (!ok) value.setCharAt(i, '-');
    }

    while (value.indexOf("--") >= 0) value.replace("--", "-");
    while (value.length() > 0 && value.charAt(0) == '-') value.remove(0, 1);
    while (value.length() > 0 && value.charAt(value.length() - 1) == '-') value.remove(value.length() - 1);

    if (value.length() > 63) value.remove(63);
    if (value.length() == 0) value = "dev";

    pHostname = std::move(value);
}

void settings_t::sanitizeIpString(String& s) noexcept {
    s.trim();
    s.replace(',', '.');
    s.replace(" ", "");

    while (s.indexOf("..") >= 0) s.replace("..", ".");

    if (s.length() && s[0] == '.')   s.remove(0, 1);
    if (s.length() && s[s.length()-1] == '.') s.remove(s.length()-1);
}

void settings_t::network_t::IP_Address(String value) noexcept {
    sanitizeIpString(value);

    if (value.length() == 0) {
        pIP_Address = IPAddress(0,0,0,0);
        return;
    }

    IPAddress parsed;
    if (!parsed.fromString(value)) {
        pIP_Address = IPAddress(0,0,0,0);
        return;
    }

    if (parsed[0] == 255 && parsed[1] == 255 && parsed[2] == 255 && parsed[3] == 255) {
        pIP_Address = IPAddress(0,0,0,0);
        return;
    }

    pIP_Address = parsed;
}

void settings_t::orchestrator_t::IP_Address(String value) noexcept {
    sanitizeIpString(value);

    if (value.length() == 0) {
        pIP_Address = IPAddress(0,0,0,0);
        return;
    }

    IPAddress parsed;
    if (!parsed.fromString(value)) {
        pIP_Address = IPAddress(0,0,0,0);
        return;
    }

    if (parsed[0] == 255 && parsed[1] == 255 && parsed[2] == 255 && parsed[3] == 255) {
        pIP_Address = IPAddress(0,0,0,0);
        return;
    }

    pIP_Address = parsed;
}

void settings_t::network_t::Gateway(String value) noexcept {
    sanitizeIpString(value);

    if (value.length() == 0) {
        pGateway = IPAddress(0,0,0,0);
        return;
    }

    IPAddress parsed;
    if (!parsed.fromString(value)) {
        pGateway = IPAddress(0,0,0,0);
        return;
    }

    bool isBroadcast = (parsed[0] == 255 && parsed[1] == 255 && parsed[2] == 255 && parsed[3] == 255);
    bool isMulticast = (parsed[0] >= 224 && parsed[0] <= 239);

    if (isBroadcast || isMulticast || parsed == IPAddress(0,0,0,0)) {
        pGateway = IPAddress(0,0,0,0);
        return;
    }

    pGateway = parsed;
}

bool settings_t::network_t::isValidNetmask(const IPAddress& mask) noexcept {
    uint32_t m = (uint32_t(mask[0]) << 24) | (uint32_t(mask[1]) << 16) | (uint32_t(mask[2]) << 8)  | uint32_t(mask[3]);
    return m != 0 && ((m | (m - 1)) == 0xFFFFFFFFu);
}

void settings_t::network_t::stripControlChars(String& s) noexcept {
    String out; out.reserve(s.length());
    for (size_t i = 0; i < s.length(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if ((c >= 0x20 && c != 0x7F)) out += char(c);
    }
    s = out;
}

bool settings_t::network_t::isPrintableASCII(const String& s) noexcept {
    for (size_t i = 0; i < s.length(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x20 || c > 0x7E) return false;
    }
    return true;
}

bool settings_t::network_t::isHex64(const String& s) noexcept {
    if (s.length() != 64) return false;
    for (size_t i = 0; i < 64; ++i) {
        char c = s[i];
        bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!hex) return false;
    }
    return true;
}

void settings_t::network_t::Netmask(String value) noexcept {
    sanitizeIpString(value);

    if (value.length() == 0) {
        pNetmask = IPAddress(255,255,255,0);
        return;
    }

    IPAddress parsed;
    if (!parsed.fromString(value)) {
        pNetmask = IPAddress(255,255,255,0);
        return;
    }

    if (!isValidNetmask(parsed)) {
        pNetmask = IPAddress(255,255,255,0);
        return;
    }

    pNetmask = parsed;
}

void settings_t::network_t::DNS(uint8_t index, String value) noexcept {
    if (index >= 2) return;
    sanitizeIpString(value);

    if (value.length() == 0) { pDNS[index] = IPAddress(0,0,0,0); return; }

    IPAddress parsed;
    if (!parsed.fromString(value)) { pDNS[index] = IPAddress(0,0,0,0); return; }

    bool isBroadcast = (parsed[0]==255 && parsed[1]==255 && parsed[2]==255 && parsed[3]==255);
    bool isMulticast = (parsed[0] >= 224 && parsed[0] <= 239);
    if (isBroadcast || isMulticast) { pDNS[index] = IPAddress(0,0,0,0); return; }

    pDNS[index] = parsed;  
}

void settings_t::network_t::SSID(String value) noexcept {
    value.trim();
    stripControlChars(value);

    if (value.length() > 32) value.remove(32);

    pSSID = std::move(value);
}

void settings_t::network_t::Passphrase(String value) noexcept {
    value.trim();

    if (value.length() == 0) {
        pPassphrase = String();
        return;
    }

    if (isHex64(value)) {
        pPassphrase = std::move(value);
        return;
    }

    if (value.length() >= 8 && value.length() <= 63 && isPrintableASCII(value)) {
        pPassphrase = std::move(value);
        return;
    }

    pPassphrase = String();
}

void settings_t::update_t::ManifestURL(String value) noexcept {
    value.trim();
    value.toLowerCase();

    constexpr size_t MIN_URL_LEN = 10;
    constexpr size_t MAX_URL_LEN = 200;

    if (value.length() < MIN_URL_LEN || value.length() > MAX_URL_LEN) {
        pManifestURL = String();
        return;
    }

    if (!value.startsWith("http://") && !value.startsWith("https://")) {
        pManifestURL = String();
        return;
    }
    for (size_t i = 0; i < value.length(); ++i) {
        unsigned char c = static_cast<unsigned char>(value[i]);
        if (c <= 0x20 || c >= 0x7F) {
            pManifestURL = String();
            return;
        }
    }

    pManifestURL = std::move(value);
}

void settings_t::update_t::PasswordLANOTA(String value) noexcept {
    value.trim();

    constexpr size_t MIN_LEN = 6;
    constexpr size_t MAX_LEN = 64;

    if (value.length() < MIN_LEN || value.length() > MAX_LEN) {
        pPasswordLANOTA = String();
        return;
    }

    for (size_t i = 0; i < value.length(); ++i) {
        unsigned char c = static_cast<unsigned char>(value[i]);
        if (c < 0x20 || c > 0x7E) {
            pPasswordLANOTA = String();
            return;
        }
    }

    pPasswordLANOTA = std::move(value);
}

void settings_t::general_t::NTPServer(String value) noexcept {
    value.trim();
    value.toLowerCase();

    constexpr size_t MIN_LEN = 3;
    constexpr size_t MAX_LEN = 128;

    if (value.length() == 0) {
        pNTPServer = "pool.ntp.org";
        return;
    }

    if (value.length() < MIN_LEN || value.length() > MAX_LEN) {
        pNTPServer = "pool.ntp.org";
        return;
    }

    for (size_t i = 0; i < value.length(); ++i) {
        char c = value.charAt(i);
        bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '.') || (c == '-');
        if (!ok) {
            pNTPServer = "pool.ntp.org";
            return;
        }
    }

    if (value.indexOf(' ') >= 0) {
        pNTPServer = "pool.ntp.org";
        return;
    }

    pNTPServer = std::move(value);
}

void settings_t::orchestrator_t::ServerID(String value) noexcept {
    value.trim();
    value.toUpperCase();

    constexpr size_t REQUIRED_LEN = 15;
    if (value.length() != REQUIRED_LEN) {
        pServerID = String();
        return;
    }

    for (size_t i = 0; i < value.length(); ++i) {
        char c = value.charAt(i);
        bool ok = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        if (!ok) {
            pServerID = String();
            return;
        }
    }

    pServerID = std::move(value);
}

void settings_t::webhooks_t::Token(String value) noexcept {
    value.trim();

    constexpr size_t MIN_LEN = 1;
    constexpr size_t MAX_LEN = 64;

    if (value.length() < MIN_LEN || value.length() > MAX_LEN) {
        pToken = String();
        return;
    }

    for (size_t i = 0; i < value.length(); ++i) {
        char c = value.charAt(i);
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '-') || (c == '_');
        if (!ok) {
            pToken = String();
            return;
        }
    }

    pToken = std::move(value);
}

void settings_t::mqtt_t::Broker(String value) noexcept {
    value.trim();
    value.toLowerCase();

    constexpr size_t MIN_LEN = 3;
    constexpr size_t MAX_LEN = 128;

    if (value.length() == 0) {
        pBroker = String();
        return;
    }

    if (value.length() < MIN_LEN || value.length() > MAX_LEN) {
        pBroker = String();
        return;
    }

    for (size_t i = 0; i < value.length(); ++i) {
        char c = value.charAt(i);
        bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '.') || (c == '-');
        if (!ok) {
            pBroker = String();
            return;
        }
    }

    if (value.indexOf(' ') >= 0) {
        pBroker = String();
        return;
    }

    pBroker = std::move(value);
}

void settings_t::mqtt_t::User(String value) noexcept {
    value.trim();

    constexpr size_t MIN_LEN = 3;
    constexpr size_t MAX_LEN = 64;

    if (value.length() < MIN_LEN || value.length() > MAX_LEN) {
        pUser = String();
        return;
    }

    for (size_t i = 0; i < value.length(); ++i) {
        char c = value.charAt(i);
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '.') || (c == '_') || (c == '-');
        if (!ok) {
            pUser = String();
            return;
        }
    }

    pUser = std::move(value);
}

void settings_t::mqtt_t::Password(String value) noexcept {
    value.trim();

    constexpr size_t MIN_LEN = 6;
    constexpr size_t MAX_LEN = 64;

    if (value.length() < MIN_LEN || value.length() > MAX_LEN) {
        pPassword = String();
        return;
    }

    for (size_t i = 0; i < value.length(); ++i) {
        unsigned char c = static_cast<unsigned char>(value[i]);
        if (c < 0x20 || c > 0x7E) {
            pPassword = String();
            return;
        }
    }

    pPassword = std::move(value);
}

void settings_t::LoadDefaults() {
    // Log
    Log.Endpoint(Defaults.Log.Endpoint);
    Log.LogLevel(Defaults.Log.Level);
    Log.SyslogServerHost(Defaults.Log.SyslogServer);
    Log.SyslogServerPort(Defaults.Log.SyslogPort);

    // Network
    Network.DHCPClient(Defaults.Network.DHCPClient);
    Network.Hostname(Defaults.Network.Hostname());
    Network.IP_Address(Defaults.Network.IP_Address);
    Network.Gateway(Defaults.Network.Gateway);
    Network.Netmask(Defaults.Network.Netmask);
    Network.DNS(0, Defaults.Network.DNS[0]);
    Network.DNS(1, Defaults.Network.DNS[1]);
    Network.SSID(Defaults.Network.SSID);
    Network.Passphrase(Defaults.Network.Passphrase);
    Network.ConnectionTimeout(Defaults.Network.ConnectionTimeout);
    Network.OnlineChecking(Defaults.Network.OnlineChecking);
    Network.OnlineCheckingTimeout(Defaults.Network.OnlineCheckingTimeout);

    // Update
    Update.ManifestURL(Defaults.Update.ManifestURL);
    Update.AllowInsecure(Defaults.Update.AllowInsecure);
    Update.EnableLANOTA(Defaults.Update.EnableLANOTA);
    Update.PasswordLANOTA(Defaults.Update.PasswordLANOTA);
    Update.CheckInterval(Defaults.Update.CheckInterval);
    Update.AutoReboot(Defaults.Update.AutoReboot);
    Update.Debug(Defaults.Update.Debug);
    Update.CheckAtStartup(Defaults.Update.CheckAtStartup);

    // General
    General.NTPUpdate(Defaults.General.NTPUpdate);
    General.NTPServer(Defaults.General.NTPServer);
    General.SaveStatePooling(Defaults.General.SaveStatePooling);

    // Orchestrator
    Orchestrator.Assigned(Defaults.Orchestrator.Assigned);
    Orchestrator.ServerID(Defaults.Orchestrator.ServerID);
    Orchestrator.IP_Address(Defaults.Orchestrator.IP_Address);
    Orchestrator.Port(Defaults.Orchestrator.Port);

    // WebHooks
    WebHooks.Port(Defaults.WebHooks.Port);
    WebHooks.Enabled(Defaults.WebHooks.Enabled);
    WebHooks.Token(Defaults.WebHooks.Token);

    // MQTT
    MQTT.Enabled(Defaults.MQTT.Enabled);
    MQTT.Broker(Defaults.MQTT.Broker);
    MQTT.Port(Defaults.MQTT.Port);
    MQTT.User(Defaults.MQTT.User);
    MQTT.Password(Defaults.MQTT.Password);
}

bool settings_t::Load(const String& configfilename) noexcept {
    LoadDefaults();

    const String path = configfilename.length() ? configfilename : String(Defaults.ConfigFileName);
    File f = devFileSystem->OpenFile(path, "r");
    if (!f || !f.available()) {
        if (f) f.close();
        pFirstRun = true;
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    
    f.close();
    if (err) return false;

    JsonObjectConst root = doc.as<JsonObjectConst>();
    if (root.isNull()) return false;

    // Log
    if (root["Log"].is<JsonObjectConst>()) {
        JsonObjectConst log = root["Log"].as<JsonObjectConst>();
        Log.Endpoint((uint8_t)(log["Endpoint"] | Defaults.Log.Endpoint));
        Log.LogLevel((uint8_t)(log["Level"] | Defaults.Log.Level));
        Log.SyslogServerHost(String(log["Syslog Server"] | Defaults.Log.SyslogServer));
        Log.SyslogServerPort((uint16_t)(log["Syslog Port"] | Defaults.Log.SyslogPort));
    }

    // Network
    if (root["Network"].is<JsonObjectConst>()) {
        JsonObjectConst net = root["Network"].as<JsonObjectConst>();
        Network.DHCPClient((bool)(net["DHCP Client"] | Defaults.Network.DHCPClient));
        Network.Hostname(String(net["Hostname"] | Defaults.Network.Hostname()));
        Network.IP_Address(String(net["IP Address"] | Defaults.Network.IP_Address));
        Network.Gateway(String(net["Gateway"] | Defaults.Network.Gateway));
        Network.Netmask(String(net["Netmask"] | Defaults.Network.Netmask));

        if (net["DNS Servers"].is<JsonArrayConst>()) {
            JsonArrayConst dns = net["DNS Servers"].as<JsonArrayConst>();
            uint8_t idx = 0;
            for (JsonVariantConst v : dns) {
                if (idx >= 2) break;
                Network.DNS(idx, v);
            }
        }

        Network.SSID(String(net["SSID"] | Defaults.Network.SSID));
        Network.Passphrase(String(net["Passphrase"] | Defaults.Network.Passphrase));
        Network.ConnectionTimeout((uint16_t)(net["Connection Timeout"] | Defaults.Network.ConnectionTimeout));
        Network.OnlineChecking((uint16_t)(net["Online Checking"] | Defaults.Network.OnlineChecking));
        Network.OnlineCheckingTimeout((uint16_t)(net["Online Checking Timeout"] | Defaults.Network.OnlineCheckingTimeout));
    }

    // Update
    if (root["Update"].is<JsonObjectConst>()) {
        JsonObjectConst up = root["Update"].as<JsonObjectConst>();
        Update.ManifestURL(String(up["Manifest URL"] | Defaults.Update.ManifestURL));
        Update.AllowInsecure((bool)(up["Allow Insecure"] | Defaults.Update.AllowInsecure));
        Update.EnableLANOTA((bool)(up["Enable LAN OTA"] | Defaults.Update.EnableLANOTA));
        Update.PasswordLANOTA(String(up["Password LAN OTA"] | Defaults.Update.PasswordLANOTA));
        Update.CheckInterval((uint16_t)(up["Check Interval"] | Defaults.Update.CheckInterval));
        Update.AutoReboot((bool)(up["Auto Reboot"] | Defaults.Update.AutoReboot));
        Update.Debug((bool)(up["Debug"] | Defaults.Update.Debug));
        Update.CheckAtStartup((bool)(up["Check At Startup"] | Defaults.Update.CheckAtStartup));
    }

    // General
    if (root["General"].is<JsonObjectConst>()) {
        JsonObjectConst gen = root["General"].as<JsonObjectConst>();
        General.NTPUpdate((bool)(gen["NTP Update"] | Defaults.General.NTPUpdate));
        General.NTPServer(String(gen["NTP Server"] | Defaults.General.NTPServer));
        General.SaveStatePooling((uint32_t)(gen["Save State Pooling"] | Defaults.General.SaveStatePooling));
    }

    // Orchestrator
    if (root["Orchestrator"].is<JsonObjectConst>()) {
        JsonObjectConst orch = root["Orchestrator"].as<JsonObjectConst>();
        Orchestrator.Assigned((bool)(orch["Assigned"] | Defaults.Orchestrator.Assigned));
        Orchestrator.ServerID(String(orch["Server ID"] | Defaults.Orchestrator.ServerID));
        Orchestrator.IP_Address(String(orch["IP Address"] | Defaults.Orchestrator.IP_Address));
        Orchestrator.Port((uint16_t)(orch["Port"] | Defaults.Orchestrator.Port));
    }

    // WebHooks
    if (root["WebHooks"].is<JsonObjectConst>()) {
        JsonObjectConst wh = root["WebHooks"].as<JsonObjectConst>();
        WebHooks.Port((uint16_t)(wh["Port"] | Defaults.WebHooks.Port));
        WebHooks.Enabled((bool)(wh["Enabled"] | Defaults.WebHooks.Enabled));
        WebHooks.Token(String(wh["Token"] | Defaults.WebHooks.Token));

        if (WebHooks.Token().isEmpty()) WebHooks.Enabled(false); // Token must be >= 1 char
    }

    // MQTT
    if (root["MQTT"].is<JsonObjectConst>()) {
        JsonObjectConst mq = root["MQTT"].as<JsonObjectConst>();
        MQTT.Enabled((bool)(mq["Enabled"] | Defaults.MQTT.Enabled));
        MQTT.Broker(String(mq["Broker"] | Defaults.MQTT.Broker));
        MQTT.Port((uint16_t)(mq["Port"] | Defaults.MQTT.Port));
        MQTT.User(String(mq["User"] | Defaults.MQTT.User));
        MQTT.Password(String(mq["Password"] | Defaults.MQTT.Password));
    }

    return true;
}

bool settings_t::SaveComponentsState(const String& configfilename) noexcept {
    const String path = configfilename.length() ? configfilename : String(Defaults.ConfigFileName);
    File f = devFileSystem->OpenFile(path, "r");
    if (!f || !f.available()) {
        if (f) f.close();
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;

    JsonObject root = doc.as<JsonObject>();
    if (root.isNull()) return false;
    
    JsonArray components = root["Components"].as<JsonArray>();

    if (components.isNull()) return false; // No components found

    auto findComponentObj = [&](String name) -> JsonObject {
        for (JsonObject obj : components) {
            const char* jName = obj["Name"] | "";
            if (name.equalsIgnoreCase(jName)) return obj;
        }
        return JsonObject();
    };

    for (Generic* comp : Components) {
        JsonObject obj = findComponentObj(comp->Name());
        if (obj.isNull()) continue;

        switch (comp->Class()) {
            case CLASS_RELAY : {
                obj["State"] = comp->as<Relay>()->State();
            } break;
            case CLASS_BLINDS : {
                obj["Position"] = comp->as<Blinds>()->State();
            } break;
        }
    }

    File fw = devFileSystem->OpenFile(path, "w");
    if (!fw) return false;

    const size_t written = serializeJsonPretty(doc, fw);
    fw.close();

    pSaveComponentsStateFlag = false;
    return written > 0;
}

bool settings_t::InstallComponents(const String& configfilename) noexcept {
    const String path = configfilename.length() ? configfilename : String(Defaults.ConfigFileName);
    File f = devFileSystem->OpenFile(path, "r");
    if (!f || !f.available()) {
        if (f) f.close();
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;

    JsonObjectConst root = doc.as<JsonObjectConst>();
    if (root.isNull()) return false;
    
    JsonArrayConst components = root["Components"].as<JsonArrayConst>();

    if (components.isNull()) {
        Serial.println(F("No components found in configuration."));
        return false;
    }

    Components.Clear();

    uint8_t comp_id = 0;

    for (JsonObjectConst comp : components) {
        const String comp_name = String(comp["Name"] | "");
        const String comp_class = String(comp["Class"] | "");
        uint8_t comp_address = (uint8_t)(comp["Address"] | 0);
        bool comp_enabled = (bool)(comp["Enabled"] | false);
        String comp_bus = String(comp["Bus"] | "");

        if (comp_name.isEmpty()) {
            if (devLog) devLog->Write("Component: Empty name for component #" + String(comp_id) + " - component not installed", LOGLEVEL_WARNING);
            continue;
        }

        if (comp_class.isEmpty()) {
            if (devLog) devLog->Write("Component: Empty class for component #" + String(comp_id) + " - component not installed", LOGLEVEL_WARNING);
            continue;
        }

        if (comp_bus.isEmpty()) {
            if (devLog) devLog->Write("Component: Empty bus for component #" + String(comp_id) + " - component not installed", LOGLEVEL_WARNING);
            continue;
        }

        if (AvailableComponentBuses.find(comp_bus) == AvailableComponentBuses.end()) {
            if (devLog) devLog->Write("Component: Unknoun bus name '" + comp_bus + "' for component #" + String(comp_id) + " - component not installed", LOGLEVEL_WARNING);
            continue;
        }

        if (AvailableComponentClasses.find(comp_class) == AvailableComponentClasses.end()) {
            if (devLog) devLog->Write("Component: Unknoun class name '" + comp_class + "' for component #" + String(comp_id) + " - component not installed", LOGLEVEL_WARNING);
            continue;
        }

        Generic *NewComponent = nullptr;

        Classes c = AvailableComponentClasses.at(comp_class);

        switch (c) {
            case CLASS_GENERIC: {
                // Reserved
            } break;

            case CLASS_BLINDS: {
                int16_t relayUpIndex = Components.IndexOf(comp["Relay Up"] | "");
                int16_t relayDnIndex = Components.IndexOf(comp["Relay Down"] | "");

                if (relayUpIndex > -1 && relayDnIndex > -1) {
                    NewComponent = new Blinds(comp_name, comp_id, Components.At(relayUpIndex)->as<Relay>(), Components.At(relayDnIndex)->as<Relay>());
                    NewComponent->as<Blinds>()->Position((uint8_t)(comp["Position"] | 0), true);
                } else {
                    if (devLog) devLog->Write("Component: Blinds '" + comp_name + "' not created: relay up/down not found", LOGLEVEL_WARNING);
                }

                auto* n = NewComponent->as<Blinds>();
                n->Event["Changed"]([this, n] {
                    if (devMQTT) {
                        devMQTT->Publish(Network.Hostname() + "/Get/Blinds:" + n->Name() + ":Position", String(n->Position()));
                        devMQTT->Publish(Network.Hostname() + "/Get/Blinds:" + n->Name() + ":State", String(n->State()));
                    }

                    pSaveComponentsStateFlag = true;
                });
            } break;

            case CLASS_BUTTON: {
                NewComponent = new Button(comp_name, comp_id, AvailableComponentBuses.at(comp_bus), comp_address, (comp["Report"].as<String>().equalsIgnoreCase("EdgesOnly") ? ButtonReportModes::BUTTONREPORTMODE_EDGESONLY : ButtonReportModes::BUTTONREPORTMODE_CLICKSONLY));

                auto* n = NewComponent->as<Button>();
                if (n->ReportMode() == ButtonReportModes::BUTTONREPORTMODE_CLICKSONLY) {
                    n->Event["Clicked"]([this, n] {
                        if (devMQTT) devMQTT->Publish(Network.Hostname() + "/Get/Button:" + n->Name(), "Clicked");
                    });
                    n->Event["DoubleClicked"]([this, n] {
                        if (devMQTT) devMQTT->Publish(Network.Hostname() + "/Get/Button:" + n->Name(), "DoubleClicked");
                    });
                    n->Event["TripleClicked"]([this, n] {
                        if (devMQTT) devMQTT->Publish(Network.Hostname() + "/Get/Button:" + n->Name(), "TripleClicked");
                    });
                    n->Event["LongClicked"]([this, n] {
                        if (devMQTT) devMQTT->Publish(Network.Hostname() + "/Get/Button:" + n->Name(), "LongClicked");
                    });
                } else if (n->ReportMode() == ButtonReportModes::BUTTONREPORTMODE_EDGESONLY) {
                    n->Event["Pressed"]([this, n] {
                        if (devMQTT) devMQTT->Publish(Network.Hostname() + "/Get/Button:" + n->Name(), "Pressed");
                    });
                    n->Event["Released"]([this, n] {
                        if (devMQTT) devMQTT->Publish(Network.Hostname() + "/Get/Button:" + n->Name(), "Released");
                    });
                }
            } break;

            case CLASS_CURRENTMETER: {
                NewComponent = new Currentmeter(comp_name, comp_id, AvailableComponentBuses.at(comp_bus), comp_address);

                auto* n = NewComponent->as<Currentmeter>();
                n->Event["Changed"]([this, n] {
                    if (devMQTT) {
                        devMQTT->Publish(Network.Hostname() + "/Get/Currentmeter:" + n->Name() + ":DC", String(n->CurrentDC()));
                        devMQTT->Publish(Network.Hostname() + "/Get/Currentmeter:" + n->Name() + ":AC", String(n->CurrentAC()));
                    }
                });
            } break;

            case CLASS_RELAY: {
                NewComponent = new Relay(comp_name, comp_id, AvailableComponentBuses.at(comp_bus), comp_address, DeviceIQ_Components::RelayTypes::RELAYTYPE_NORMALLYCLOSED);

                auto* n = NewComponent->as<Relay>();
                n->State((bool)(comp["State"] | false));

                n->Event["Changed"]([this, n] {
                    if (devMQTT) devMQTT->Publish(Network.Hostname() + "/Get/Relay:" + n->Name(), n->State() ? "on" : "off");
                    pSaveComponentsStateFlag = true;
                });
            } break;

            case CLASS_PIR: {
                NewComponent = new PIR(comp_name, comp_id, AvailableComponentBuses.at(comp_bus), comp_address);

                auto* n = NewComponent->as<PIR>();
                n->DebounceTime((uint32_t)(comp["Debounce"] | 200));

                n->Event["MotionDetected"]([this, n] {
                    if (devMQTT) devMQTT->Publish(Network.Hostname() + "/Get/PIR:" + n->Name(), "M");
                });
                n->Event["MotionCleared"]([this, n] {
                    if (devMQTT) devMQTT->Publish(Network.Hostname() + "/Get/PIR:" + n->Name(), "C");
                });
            } break;

            case CLASS_DOORBELL: {
                NewComponent = new Doorbell(comp_name, comp_id, AvailableComponentBuses.at(comp_bus), comp_address);

                auto* n = NewComponent->as<Doorbell>();
                n->Timeout((uint32_t)(comp["Timeout"] | 1000));

                n->Event["Ring"]([this, n] {
                    if (devMQTT) devMQTT->Publish(Network.Hostname() + "/Get/Doorbell:" + n->Name(), "1");
                });
                n->Event["DoubleRing"]([this, n] {
                    if (devMQTT) devMQTT->Publish(Network.Hostname() + "/Get/Doorbell:" + n->Name(), "2");
                });
                n->Event["LongRing"]([this, n] {
                    if (devMQTT) devMQTT->Publish(Network.Hostname() + "/Get/Doorbell:" + n->Name(), "L");
                });
            } break;

            case CLASS_CONTACTSENSOR: {
                NewComponent = new ContactSensor(comp_name, comp_id, AvailableComponentBuses.at(comp_bus), comp_address, ((bool)(comp["InvertClose"] | false)));

                auto* n = NewComponent->as<ContactSensor>();
                n->Event["Opened"]([this, n] { if (devMQTT) devMQTT->Publish(Network.Hostname() + "/Get/ContactSensor:" + n->Name(), "Opened"); });
                n->Event["Closed"]([this, n] { if (devMQTT) devMQTT->Publish(Network.Hostname() + "/Get/ContactSensor:" + n->Name(), "Closed"); });
            } break;

            case CLASS_THERMOMETER: {
                auto it = AvailableThermometerTypes.find(String(comp["Type"] | "DS18B20"));
                if (it != AvailableThermometerTypes.end()) NewComponent = new Thermometer(comp_name, comp_id, AvailableComponentBuses.at(comp_bus), comp_address, it->second);

                auto* n = NewComponent->as<Thermometer>();
                n->Event["TemperatureChanged"]([this, n] {
                    if (devMQTT) devMQTT->Publish(Network.Hostname() + "/Get/Thermometer:" + n->Name() + ":Temperature", String(n->Temperature()));
                });
                n->Event["HumidityChanged"]([this, n] {
                    if (devMQTT) devMQTT->Publish(Network.Hostname() + "/Get/Thermometer:" + n->Name() + ":Humidity", String(n->Humidity()));
                });
                n->Event["Changed"]([this, n] {
                    if (devMQTT) {
                        devMQTT->Publish(Network.Hostname() + "/Get/Thermometer:" + n->Name() + ":Temperature", String(n->Temperature()));
                        devMQTT->Publish(Network.Hostname() + "/Get/Thermometer:" + n->Name() + ":Humidity", String(n->Humidity()));
                    }
                });
            } break;
        }

        NewComponent->Enabled(comp_enabled);

        // Components Events
        if (NewComponent) {
            JsonObjectConst EventsInConfig = comp["Events"];
            
            for (auto& ComponentEvent : NewComponent->Event) {
                const String& eventName = ComponentEvent.first;
                auto& setHandler = ComponentEvent.second;
                
                JsonVariantConst v = EventsInConfig[eventName.c_str()];
                if (v.isNull() || !v.is<const char*>()) continue;

                String actionFull = String(v.as<const char*>());
                actionFull.trim();

                int open  = actionFull.indexOf('(');
                int close = actionFull.lastIndexOf(')');

                String cmd = (open > 0) ? actionFull.substring(0, open) : actionFull;
                String param = (open >= 0 && close > open) ? actionFull.substring(open + 1, close) : "";
                cmd.trim(); param.trim();

                // static macros
                param.replace("%NAME%", NewComponent->Name());

                if (cmd.equalsIgnoreCase("log")) {
                    String msg = param;
                    auto logPtr = devLog;
                    setHandler([msg, logPtr]{
                        logPtr->Write(msg, LOGLEVEL_INFO);
                    });
                } else if (cmd.equalsIgnoreCase("enable")) {
                    String targetName = param;
                    auto* target = Components[targetName];

                    if (target) {
                        auto* generic = target->as<Generic>();
                        setHandler([generic] {
                            generic->Enabled(true);
                        });
                    }
                } else if (cmd.equalsIgnoreCase("disable")) {
                    String targetName = param;
                    auto* target = Components[targetName];

                    if (target) {
                        auto* generic = target->as<Generic>();
                        setHandler([generic] {
                            generic->Enabled(false);
                        });
                    }
                } else if (cmd.equalsIgnoreCase("invert")) {
                    String targetName = param;
                    auto* target = Components[targetName];

                    if (target && target->Class() == CLASS_RELAY) {
                        auto* relay = target->as<Relay>();
                        setHandler([relay] {
                            relay->Invert();
                        });
                    }
                } else if (cmd.equalsIgnoreCase("seton")) {
                    String targetName = param;
                    auto* target = Components[targetName];

                    if (target && target->Class() == CLASS_RELAY) {
                        auto* relay = target->as<Relay>();
                        setHandler([relay] {
                            relay->State(true);
                        });
                    }
                } else if (cmd.equalsIgnoreCase("setoff")) {
                    String targetName = param;
                    auto* target = Components[targetName];

                    if (target && target->Class() == CLASS_RELAY) {
                        auto* relay = target->as<Relay>();
                        setHandler([relay] {
                            relay->State(false);
                        });
                    }
                }
            }

            int16_t dup = Components.IndexOf(String(comp_name));
            if (dup >= 0) Components.Remove(dup);

            Components.Add(NewComponent);
            if (devLog) devLog->Write("Component: #" + String(comp_id) + " " + comp_class + "\\" + comp_name + " installed", LOGLEVEL_WARNING);

            comp_id++;
        }
    }
    return true;
}

void settings_t::RestoreToFactoryDefaults() {
    devFileSystem->Format();
    
    esp_sleep_enable_timer_wakeup(200 * 1000);
    esp_deep_sleep_start();
}

bool settings_t::Save(const String& configfilename) const noexcept {
    const String path = configfilename.length() ? configfilename : String(Defaults.ConfigFileName);

    JsonDocument existingDoc;
    {
        File fr = devFileSystem->OpenFile(path, "r");
        if (fr && fr.available()) {
            DeserializationError e = deserializeJson(existingDoc, fr);
            (void)e;
        }
        if (fr) fr.close();
    }

    JsonDocument doc;

    // Log
    {
        JsonObject log = doc["Log"].to<JsonObject>();
        log["Endpoint"] = Log.Endpoint();
        log["Level"] = Log.LogLevel();
        log["Syslog Server"] = Log.SyslogServerHost();
        log["Syslog Port"] = Log.SyslogServerPort();
    }

    // Network
    {
        JsonObject net = doc["Network"].to<JsonObject>();
        net["DHCP Client"] = Network.DHCPClient();
        net["Hostname"] = Network.Hostname();
        net["IP Address"] = Network.IP_Address().toString();
        net["Gateway"] = Network.Gateway().toString();
        net["Netmask"] = Network.Netmask().toString();

        JsonArray dns = net["DNS Servers"].to<JsonArray>();
        dns.add(Network.DNS(0).toString());
        dns.add(Network.DNS(1).toString());

        net["SSID"] = Network.SSID();
        net["Passphrase"] = Network.Passphrase();
        net["Connection Timeout"] = Network.ConnectionTimeout();
        net["Online Checking"] = Network.OnlineChecking();
        net["Online Checking Timeout"] = Network.OnlineCheckingTimeout();
    }

    // Update
    {
        JsonObject up = doc["Update"].to<JsonObject>();
        up["Manifest URL"] = Update.ManifestURL();
        up["Allow Insecure"] = Update.AllowInsecure();
        up["Enable LAN OTA"] = Update.EnableLANOTA();
        up["Password LAN OTA"] = Update.PasswordLANOTA();
        up["Check Interval"] = Update.CheckInterval();
        up["Auto Reboot"] = Update.AutoReboot();
        up["Debug"] = Update.Debug();
        up["Check At Startup"] = Update.CheckAtStartup();
    }

    // General
    {
        JsonObject gen = doc["General"].to<JsonObject>();
        gen["NTP Update"] = General.NTPUpdate();
        gen["NTP Server"] = General.NTPServer();
        gen["Save State Pooling"] = General.SaveStatePooling();
    }

    // Orchestrator
    {
        JsonObject orch = doc["Orchestrator"].to<JsonObject>();
        orch["Assigned"] = Orchestrator.Assigned();
        orch["Server ID"] = Orchestrator.ServerID();
        orch["IP Address"] = Orchestrator.IP_Address().toString();
        orch["Port"] = Orchestrator.Port();
    }

    // WebHooks
    {
        JsonObject wh = doc["WebHooks"].to<JsonObject>();
        wh["Port"] = WebHooks.Port();
        wh["Enabled"] = WebHooks.Enabled();
        wh["Token"] = WebHooks.Token();
    }

    // MQTT
    {
        JsonObject mq = doc["MQTT"].to<JsonObject>();
        mq["Enabled"] = MQTT.Enabled();
        mq["Broker"] = MQTT.Broker();
        mq["Port"] = MQTT.Port();
        mq["User"] = MQTT.User();
        mq["Password"] = MQTT.Password();
    }

    if (existingDoc["Components"].is<JsonArrayConst>()) {
        doc["Components"] = existingDoc["Components"];
    } else {
        (void)doc["Components"].to<JsonArray>();
    }

    File fw = devFileSystem->OpenFile(path, "w");
    if (!fw) return false;

    const size_t written = serializeJsonPretty(doc, fw);
    fw.close();

    return written > 0;
}