#include "DevIQ_Update.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <algorithm>
#include <memory>
#include <mbedtls/md.h>

#ifdef ARDUINO_ARCH_ESP32
  #include <ArduinoOTA.h>
#endif

using namespace DeviceIQ_Update;

static String sha256HexOf(const uint8_t* hash, size_t n) {
    const char* hx = "0123456789abcdef";
    String s; s.reserve(n*2);
    for (size_t i=0; i<n; i++) {
        s += hx[(hash[i]>>4)&0xF];
        s += hx[hash[i]&0xF];
    }
    return s;
}

static std::unique_ptr<WiFiClient> makeClient(const UpdateConfig& cfg, const String& url) {
    const bool https = url.startsWith("https://");
    if (!https) {
        // HTTP
        return std::unique_ptr<WiFiClient>(new WiFiClient());
    }

    // HTTPS
    auto s = std::unique_ptr<WiFiClientSecure>(new WiFiClientSecure());
    if (cfg.allowInsecure) {
        s->setInsecure();
        return std::unique_ptr<WiFiClient>(s.release());
    }

    if (cfg.rootCA_PEM && *cfg.rootCA_PEM) {
        s->setCACert(cfg.rootCA_PEM);
        return std::unique_ptr<WiFiClient>(s.release());
    }

    return nullptr;
}

void UpdateClient::Control() {
    _startIfReady();

    #ifdef ARDUINO_ARCH_ESP32
        if (_started && _cfg.enableLanOta) {
            ArduinoOTA.handle();
        }
    #endif

    if (_cfg.checkInterval && (millis() - _lastCheck > _cfg.checkInterval)) {
        _lastCheck = millis();
        CheckUpdateNow();
    }
}

void UpdateClient::_startNow() {
    #ifdef ARDUINO_ARCH_ESP32
        if (_cfg.enableLanOta) _setupLanOta();
    #endif
    _started = true;
}

bool UpdateClient::CheckForUpdate(Manifest& out, bool* hasUpdate, bool* forceUpdate) {
    _startIfReady();
    if (WiFi.status() != WL_CONNECTED) {
        _emitError(Error::Wifi, "No WiFi");
        return false;
    }

    _emit(Event::Checking);

    HTTPClient http;
    auto cli = makeClient(_cfg, _cfg.manifestUrl);
    if (!cli) { _emitError(Error::HttpBegin, "HTTPS requires rootCA or allowInsecure"); return false; }

    if (!http.begin(*cli, _cfg.manifestUrl)) { _emitError(Error::ManifestDownload, "begin()"); return false; }
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.addHeader("Accept-Encoding", "identity", true, true);
    http.setTimeout(_cfg.httpTimeout);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        _emitError(Error::ManifestDownload, String("HTTP ") + code);
        http.end();
        return false;
    }

    String body = http.getString();
    http.end();

    JsonDocument doc;
    auto err = deserializeJson(doc, body);
    if (err) { _emitError(Error::ManifestParse, err.c_str()); return false; }

    // Seu schema (chaves com maiúsculas e espaço)
    out.model      = doc["Model"]       | "";
    out.version    = doc["Version"]     | "";
    out.minVersion = doc["Min Version"] | "0.0.0";
    out.url        = doc["URL"]         | "";
    out.sha256     = doc["SHA256"]      | "";

    if (!out.model.equalsIgnoreCase(_cfg.model)) { _emitError(Error::ModelMismatch, out.model); return false; }
    if (out.url.length() == 0) { _emitError(Error::ManifestParse, "Empty url"); return false; }

    // Cache para getters
    _lastManifest = out;
    _hasManifest  = true;

    const bool available = IsNewer(out.version, _cfg.currentVersion);
    const bool forced    = IsNewer(out.minVersion, _cfg.currentVersion);

    if (hasUpdate)   *hasUpdate   = available;
    if (forceUpdate) *forceUpdate = forced;

    _emit(available ? Event::NewVersion : Event::NoUpdate);
    return true;
}

bool UpdateClient::CheckUpdateNow() {
    _startIfReady();
    if (WiFi.status() != WL_CONNECTED) {
        _emitError(Error::Wifi, "No WiFi");
        return false;
    }

    _emit(Event::Checking);

    Manifest m;
    if (!_loadManifest(m)) return false;

    // cache
    _lastManifest = m;
    _hasManifest  = true;

    if (!IsNewer(m.version, _cfg.currentVersion)) {
        _emit(Event::NoUpdate);
        return false;
    }

    _emit(Event::NewVersion);
    return _downloadAndApply(m);
}

bool UpdateClient::UpdateFromURL(const String& url, const String& expectedSha) {
    if (url.length() == 0) {
        _emitError(Error::HttpBegin, "Empty URL");
        return false;
    }
    Manifest m;
    m.url = url;
    m.sha256 = expectedSha;
    m.version = "0.0.0";
    return _downloadAndApply(m);
}

bool UpdateClient::InstallLatest() {
    if (!_hasManifest || _lastManifest.url.length() == 0) {
        _emitError(Error::ManifestParse, "No cached manifest");
        return false;
    }
    return _downloadAndApply(_lastManifest);
}

bool UpdateClient::_loadManifest(Manifest& out) {
    HTTPClient http;

    auto cli = makeClient(_cfg, _cfg.manifestUrl);
    if (!cli) { _emitError(Error::HttpBegin, "HTTPS requires rootCA or allowInsecure"); return false; }

    if (!http.begin(*cli, _cfg.manifestUrl)) { _emitError(Error::ManifestDownload, "begin()"); return false; }
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.addHeader("Accept-Encoding", "identity", true, true);
    http.setTimeout(_cfg.httpTimeout);

    int code = http.GET();
    if (code != HTTP_CODE_OK) { _emitError(Error::ManifestDownload, String("HTTP ") + code); http.end(); return false; }

    String body = http.getString();
    http.end();

    JsonDocument doc;
    auto err = deserializeJson(doc, body);
    if (err) { _emitError(Error::ManifestParse, err.c_str()); return false; }

    out.model      = doc["Model"]       | "";
    out.version    = doc["Version"]     | "";
    out.minVersion = doc["Min Version"] | "0.0.0";
    out.url        = doc["URL"]         | "";
    out.sha256     = doc["SHA256"]      | "";

    if (!out.model.equalsIgnoreCase(_cfg.model)) { _emitError(Error::ModelMismatch, out.model); return false; }
    if (out.url.length() == 0) { _emitError(Error::ManifestParse, "Empty url"); return false; }

    return true;
}

void UpdateClient::_setupLanOta() {
    #ifdef ARDUINO_ARCH_ESP32
        ArduinoOTA.setHostname(_cfg.lanHostname.c_str());
        if (_cfg.lanPassword.length()) ArduinoOTA.setPassword(_cfg.lanPassword.c_str());

        ArduinoOTA.onStart([this]{ _emit(Event::Applying); });
        ArduinoOTA.onEnd([this]{
            _emit(Event::Applying);
            if (_cfg.autoReboot) {
                _emit(Event::Rebooting);
                delay(200);
                ESP.restart();
            }
        });
        ArduinoOTA.onProgress([this](unsigned p, unsigned t) {
            if (_onProgress) _onProgress(p, t);
        });
        ArduinoOTA.onError([this](ota_error_t e){
            String d;
            switch (e) {
                case OTA_AUTH_ERROR:    d = "AUTH";    break;
                case OTA_BEGIN_ERROR:   d = "BEGIN";   break;
                case OTA_CONNECT_ERROR: d = "CONNECT"; break;
                case OTA_RECEIVE_ERROR: d = "RECEIVE"; break;
                case OTA_END_ERROR:     d = "END";     break;
                default:                d = String("code ") + (int)e; break;
            }
            _emitError(Error::LanOta, "ArduinoOTA " + d);
        });

        ArduinoOTA.begin();
        _emit(Event::LanReady);
    #endif
}

bool UpdateClient::_downloadAndApply(const Manifest& m) {
    HTTPClient http;

    auto cli = makeClient(_cfg, m.url);
    if (!cli) { _emitError(Error::HttpBegin, "HTTPS requires rootCA or allowInsecure"); return false; }

    if (!http.begin(*cli, m.url)) { _emitError(Error::HttpBegin, "begin()"); return false; }
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(_cfg.httpTimeout);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        _emitError(Error::HttpCode, String(code));
        http.end();
        return false;
    }

    int contentLen = http.getSize();
    WiFiClient& stream = http.getStream();

    // Read-ahead para detectar DevPkg sem perder bytes
    const size_t kHdrSize = 92;
    uint8_t hdrBuf[kHdrSize];
    size_t hdrRead = 0;

    while (hdrRead < kHdrSize) {
        size_t avail = stream.available();
        if (avail == 0) { delay(1); continue; }
        size_t need = kHdrSize - hdrRead;
        size_t toRead = (avail < need) ? avail : need;
        int r = stream.readBytes(reinterpret_cast<char*>(hdrBuf + hdrRead), toRead);
        if (r <= 0) break;
        hdrRead += (size_t)r;
    }

    auto shaToHex = [&](const uint8_t* raw)->String {
        return sha256HexOf(raw, 32);
    };

    // Local: cabeçalho do pacote (sem alterar headers públicos)
    #pragma pack(push, 1)
    struct DevpkgHeader {
        char     magic[8];     // "DEVPKG10"
        uint32_t headerLen;    // 92
        uint32_t flags;        // bit0 FS, bit1 FW
        uint32_t fsType;       // 0 = LittleFS
        uint32_t fsSize;       // bytes
        uint8_t  fsSha[32];    // SHA-256 raw
        uint32_t fwSize;       // bytes
        uint8_t  fwSha[32];    // SHA-256 raw
    };
    #pragma pack(pop)

    auto isDevPkg = [&](const uint8_t* b, size_t n)->bool {
        if (n < kHdrSize) return false;
        const DevpkgHeader* h = reinterpret_cast<const DevpkgHeader*>(b);
        if (memcmp(h->magic, "DEVPKG10", 8) != 0) return false;
        if (h->headerLen != kHdrSize) return false;
        if ((h->flags & 0x3) == 0) return false;
        return true;
    };

    // Caminho DevPkg: atualiza FS e depois FW
    if (isDevPkg(hdrBuf, hdrRead)) {
        const DevpkgHeader* H = reinterpret_cast<const DevpkgHeader*>(hdrBuf);
        _emit(Event::Downloading);

        // streamToUpdate: aplica N bytes do stream para a partição alvo, calculando SHA-256
        auto streamToUpdate = [&](size_t total, uint8_t target, String* outHex)->bool {
            if (!Update.begin(total > 0 ? total : UPDATE_SIZE_UNKNOWN, target)) {
                _emitError(Error::UpdateBegin, String(Update.getError()));
                return false;
            }

            mbedtls_md_context_t ctx;
            const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
            mbedtls_md_init(&ctx);
            mbedtls_md_setup(&ctx, info, 0);
            mbedtls_md_starts(&ctx);

            std::unique_ptr<uint8_t[]> buf(new uint8_t[_cfg.streamBufSize]);
            size_t written = 0;

            while (written < total) {
                size_t avail = stream.available();
                if (!avail) { delay(1); continue; }
                size_t toRead = avail;
                if (toRead > (total - written)) toRead = total - written;
                if (toRead > _cfg.streamBufSize) toRead = _cfg.streamBufSize;

                int r = stream.readBytes(buf.get(), toRead);
                if (r <= 0) { Update.abort(); mbedtls_md_free(&ctx); _emitError(Error::UpdateWrite, "short read"); return false; }

                mbedtls_md_update(&ctx, buf.get(), r);

                if (Update.write(buf.get(), r) != (size_t)r) {
                    Update.abort();
                    mbedtls_md_free(&ctx);
                    _emitError(Error::UpdateWrite, String(Update.getError()));
                    return false;
                }

                written += (size_t)r;
                if (_onProgress) _onProgress(written, total);
            }

            uint8_t hash[32];
            mbedtls_md_finish(&ctx, hash);
            mbedtls_md_free(&ctx);

            if (!Update.end()) {
                _emitError(Error::UpdateEnd, String(Update.getError()));
                return false;
            }
            if (!Update.isFinished()) {
                _emitError(Error::NotFinished, "Update not finished");
                return false;
            }

            *outHex = sha256HexOf(hash, 32);
            return true;
        };

        // 1) Filesystem (U_SPIFFS também cobre LittleFS em ESP32 Arduino)
        if (H->fsSize > 0 && (H->flags & 0x1)) {
            String fsHex;
            if (!streamToUpdate(H->fsSize, U_SPIFFS, &fsHex)) { http.end(); return false; }
            String expect = shaToHex(H->fsSha);
            if (fsHex.length() == 64 && !fsHex.equalsIgnoreCase(expect)) {
                _emitError(Error::ShaMismatch, "FS:" + fsHex);
                http.end();
                return false;
            }
        }

        // 2) Firmware
        if (H->fwSize > 0 && (H->flags & 0x2)) {
            String fwHex;
            if (!streamToUpdate(H->fwSize, U_FLASH, &fwHex)) { http.end(); return false; }
            String expect = shaToHex(H->fwSha);
            if (fwHex.length() == 64 && !fwHex.equalsIgnoreCase(expect)) {
                _emitError(Error::ShaMismatch, "FW:" + fwHex);
                http.end();
                return false;
            }
        }

        http.end();

        _emit(Event::Applying);
        if (_cfg.autoReboot) {
            _emit(Event::Rebooting);
            delay(400);
            ESP.restart();
        }
        return true;
    }

    // Caminho legado: binário simples de firmware
    if (!Update.begin(contentLen > 0 ? contentLen : UPDATE_SIZE_UNKNOWN)) {
        _emitError(Error::UpdateBegin, String(Update.getError()));
        http.end();
        return false;
    }
    _emit(Event::Downloading);

    // SHA-256 on-the-fly para FW simples (inclui bytes lidos no lookahead)
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, info, 0);
    mbedtls_md_starts(&ctx);

    size_t written = 0;
    if (hdrRead) {
        mbedtls_md_update(&ctx, hdrBuf, hdrRead);
        if (Update.write(hdrBuf, hdrRead) != hdrRead) {
            mbedtls_md_free(&ctx);
            Update.abort();
            http.end();
            _emitError(Error::UpdateWrite, String(Update.getError()));
            return false;
        }
        written += hdrRead;
        if (_onProgress) _onProgress(written, contentLen > 0 ? (size_t)contentLen : 0);
    }

    std::unique_ptr<uint8_t[]> buf(new uint8_t[_cfg.streamBufSize]);
    int remaining = contentLen > 0 ? (contentLen - (int)written) : -1;

    while (http.connected() && (remaining > 0 || contentLen == -1)) {
        size_t avail = stream.available();
        if (avail) {
            int r = stream.readBytes(buf.get(), std::min((int)_cfg.streamBufSize, (int)avail));
            if (r <= 0) break;

            mbedtls_md_update(&ctx, buf.get(), r);

            if (Update.write(buf.get(), r) != (size_t)r) {
                mbedtls_md_free(&ctx);
                Update.abort();
                http.end();
                _emitError(Error::UpdateWrite, String(Update.getError()));
                return false;
            }

            written += r;
            if (_onProgress) _onProgress(written, contentLen > 0 ? (size_t)contentLen : 0);
            if (remaining > 0) remaining -= r;
        }
        delay(1);
    }

    uint8_t hash[32];
    mbedtls_md_finish(&ctx, hash);
    mbedtls_md_free(&ctx);

    if (m.sha256.length() == 64) {
        String got = sha256HexOf(hash, 32);
        if (!got.equalsIgnoreCase(m.sha256)) {
            Update.abort();
            http.end();
            _emitError(Error::ShaMismatch, got);
            return false;
        }
    }

    _emit(Event::Verifying);
    if (!Update.end()) {
        http.end();
        _emitError(Error::UpdateEnd, String(Update.getError()));
        return false;
    }
    if (!Update.isFinished()) {
        http.end();
        _emitError(Error::NotFinished, "Update not finished");
        return false;
    }

    http.end();

    _emit(Event::Applying);
    if (_cfg.autoReboot) {
        _emit(Event::Rebooting);
        delay(400);
        ESP.restart();
    }
    return true;
}
