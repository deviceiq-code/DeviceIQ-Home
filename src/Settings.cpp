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

void settings_t::network_t::sanitizeIpString(String& s) noexcept {
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

    constexpr size_t MIN_LEN = 16;
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
    Network.IP_Address(Defaults.Network.IP_Address.toString());
    Network.Gateway(Defaults.Network.Gateway.toString());
    Network.Netmask(Defaults.Network.Netmask.toString());
    Network.DNS(0, Defaults.Network.DNS[0].toString());
    Network.DNS(1, Defaults.Network.DNS[1].toString());
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

    // Orchestrator
    Orchestrator.Assigned(Defaults.Orchestrator.Assigned);
    Orchestrator.ServerID(Defaults.Orchestrator.ServerID);

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

bool settings_t::toU16(JsonVariantConst v, uint16_t& out) {
    if (v.is<unsigned long>()) {
        unsigned long val = v.as<unsigned long>();
        if (val <= 65535UL) { out = static_cast<uint16_t>(val); return true; }
    }
    if (v.is<long>()) {
        long val = v.as<long>();
        if (val >= 0 && val <= 65535) { out = static_cast<uint16_t>(val); return true; }
    }
    if (v.is<const char*>()) {
        char* end = nullptr;
        long val = strtol(v.as<const char*>(), &end, 10);
        if (end && *end == '\0' && val >= 0 && val <= 65535) { out = static_cast<uint16_t>(val); return true; }
    }
    return false;
}

bool settings_t::toBool(JsonVariantConst v, bool& out) {
    if (v.is<bool>()) { out = v.as<bool>(); return true; }
    if (v.is<long>()) { out = v.as<long>() != 0; return true; }
    if (v.is<const char*>()) {
        const char* s = v.as<const char*>();
        if (!s) return false;
        if (!strcasecmp(s, "true") || !strcasecmp(s, "yes") || !strcmp(s, "1"))  { out = true;  return true; }
        if (!strcasecmp(s, "false")|| !strcasecmp(s, "no")  || !strcmp(s, "0"))   { out = false; return true; }
    }
    return false;
}

static String settings_t::ipStringFrom(JsonVariantConst v) {
    if (v.is<const char*>()) return String(v.as<const char*>());
    if (v.is<JsonArrayConst>()) {
        JsonArrayConst a = v.as<JsonArrayConst>();
        if (a.size() == 4 && a[0].is<long>() && a[1].is<long>() && a[2].is<long>() && a[3].is<long>()) {
            return String((int)a[0].as<long>()) + "." + String((int)a[1].as<long>()) + "." + String((int)a[2].as<long>()) + "." + String((int)a[3].as<long>());
        }
    }
    return String();
}

bool settings_t::Load(const String& configfilename) noexcept {
    LoadDefaults();

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

    // Log
    if (root["Log"].is<JsonObjectConst>()) {
        JsonObjectConst log = root["Log"].as<JsonObjectConst>();
        if (log["Endpoint"])      Log.Endpoint((uint8_t) log["Endpoint"].as<long>());
        if (log["Level"])         Log.LogLevel((uint8_t) log["Level"].as<long>());
        if (log["Syslog Server"].is<const char*>()) Log.SyslogServerHost(String(log["Syslog Server"].as<const char*>()));
        uint16_t u16;
        if (toU16(log["Syslog Port"], u16)) Log.SyslogServerPort(u16);
    }

    // Network
    if (root["Network"].is<JsonObjectConst>()) {
        JsonObjectConst net = root["Network"].as<JsonObjectConst>();
        bool bval; uint16_t u16;

        if (toBool(net["DHCP Client"], bval)) Network.DHCPClient(bval);
        if (net["Hostname"].is<const char*>()) Network.Hostname(String(net["Hostname"].as<const char*>()));

        String ip;
        ip = ipStringFrom(net["IP Address"]); if (ip.length()) Network.IP_Address(ip);
        ip = ipStringFrom(net["Gateway"]);    if (ip.length()) Network.Gateway(ip);
        ip = ipStringFrom(net["Netmask"]);    if (ip.length()) Network.Netmask(ip);

        if (net["DNS Servers"].is<JsonArrayConst>()) {
            JsonArrayConst dns = net["DNS Servers"].as<JsonArrayConst>();
            uint8_t idx = 0;
            for (JsonVariantConst v : dns) {
                if (idx >= 2) break;
                String d = ipStringFrom(v);
                if (d.length()) Network.DNS(idx, d);
                idx++;
            }
        }

        if (net["SSID"].is<const char*>())       Network.SSID(String(net["SSID"].as<const char*>()));
        if (net["Passphrase"].is<const char*>()) Network.Passphrase(String(net["Passphrase"].as<const char*>()));
        if (toU16(net["Connection Timeout"], u16))      Network.ConnectionTimeout(u16);
        if (toBool(net["Online Checking"], bval))       Network.OnlineChecking(bval);
        if (toU16(net["Online Checking Timeout"], u16)) Network.OnlineCheckingTimeout(u16);
    }

    // -------------------------
    // Update
    // -------------------------
    if (root["Update"].is<JsonObjectConst>()) {
        JsonObjectConst up = root["Update"].as<JsonObjectConst>();
        bool bval; uint16_t u16;

        if (up["Manifest URL"].is<const char*>())   Update.ManifestURL(String(up["Manifest URL"].as<const char*>()));
        if (toBool(up["Allow Insecure"], bval))     Update.AllowInsecure(bval);
        if (toBool(up["Enable LAN OTA"], bval))     Update.EnableLANOTA(bval);
        if (up["Password LAN OTA"].is<const char*>()) Update.PasswordLANOTA(String(up["Password LAN OTA"].as<const char*>()));
        if (toU16(up["Check Interval"], u16))       Update.CheckInterval(u16);
        if (toBool(up["Auto Reboot"], bval))        Update.AutoReboot(bval);
        if (toBool(up["Debug"], bval))              Update.Debug(bval);
        if (toBool(up["Check At Startup"], bval))   Update.CheckAtStartup(bval);
    }

    // -------------------------
    // General
    // -------------------------
    if (root["General"].is<JsonObjectConst>()) {
        JsonObjectConst gen = root["General"].as<JsonObjectConst>();
        bool bval;
        if (toBool(gen["NTP Update"], bval))        General.NTPUpdate(bval);
        if (gen["NTP Server"].is<const char*>())    General.NTPServer(String(gen["NTP Server"].as<const char*>()));
    }

    // -------------------------
    // Orchestrator
    // -------------------------
    if (root["Orchestrator"].is<JsonObjectConst>()) {
        JsonObjectConst orch = root["Orchestrator"].as<JsonObjectConst>();
        bool bval;
        if (toBool(orch["Assigned"], bval))         Orchestrator.Assigned(bval);
        if (orch["Server ID"].is<const char*>())    Orchestrator.ServerID(String(orch["Server ID"].as<const char*>()));
    }

    // -------------------------
    // WebHooks
    // -------------------------
    if (root["WebHooks"].is<JsonObjectConst>()) {
        JsonObjectConst wh = root["WebHooks"].as<JsonObjectConst>();
        bool bval; uint16_t u16;
        if (toU16(wh["Port"], u16))                 WebHooks.Port(u16);
        if (toBool(wh["Enabled"], bval))            WebHooks.Enabled(bval);
        if (wh["Token"].is<const char*>())          WebHooks.Token(String(wh["Token"].as<const char*>()));
    }

    // -------------------------
    // MQTT
    // -------------------------
    if (root["MQTT"].is<JsonObjectConst>()) {
        JsonObjectConst mq = root["MQTT"].as<JsonObjectConst>();
        bool bval; uint16_t u16;

        // aceita "Enabled" OU "Enable"
        if (toBool(mq["Enabled"], bval) || toBool(mq["Enable"], bval)) MQTT.Enabled(bval);

        if (mq["Broker"].is<const char*>())         MQTT.Broker(String(mq["Broker"].as<const char*>()));
        if (toU16(mq["Port"], u16))                 MQTT.Port(u16);
        if (mq["User"].is<const char*>())           MQTT.User(String(mq["User"].as<const char*>()));
        if (mq["Password"].is<const char*>())       MQTT.Password(String(mq["Password"].as<const char*>()));
    }

    // Components: ignorado aqui (tratar no seu vetor próprio)
    return true; // carregou e aplicou; ausentes/invalidos ficaram com defaults
}