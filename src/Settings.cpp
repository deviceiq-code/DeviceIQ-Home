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

void settings_t::ResetToDefaults() {
    // Log
    Log.Endpoint(Defaults.Log.Endpoint);
    Log.LogLevel(Defaults.Log.Level);
    Log.SyslogServerHost(Defaults.Log.SyslogServer);
    Log.SyslogServerPort(Defaults.Log.SyslogPort);

    // Network
    Network.DHCPClient(Defaults.Network.DHCPClient);
    Network.Hostname(Defaults.Network.Hostname);
    Network.IP_Address(Defaults.Network.IP_Address.toString()); // setter espera String
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