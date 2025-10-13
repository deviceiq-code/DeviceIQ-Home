#ifndef DevIQ_Update_h
#define DevIQ_Update_h

#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <functional>

namespace DeviceIQ_Update {
    enum class Event {
        Idle,
        Init,
        LanReady,
        Checking,
        NoUpdate,
        NewVersion,
        Downloading,
        Verifying,
        Applying,
        Rebooting,
        RollbackNeeded,
        Error
    };

    enum class Error {
        None,
        Wifi,
        ManifestDownload,
        ManifestParse,
        ModelMismatch,
        HttpBegin,
        HttpCode,
        UpdateBegin,
        UpdateWrite,
        ShaMismatch,
        UpdateEnd,
        NotFinished,
        LanOta
    };

    struct UpdateConfig {
        String model;
        String currentVersion;
        String manifestUrl;
        const char* rootCA_PEM = nullptr;
        bool allowInsecure = false;
        bool enableLanOta = false;
        String lanHostname = "deviceiq";
        String lanPassword = "";
        uint32_t checkInterval = 21600; // 6h
        uint32_t httpTimeout = 15;
        size_t streamBufSize = 4096;
        bool autoReboot = true;
    };

    struct Manifest {
        String model;
        String version;
        String minVersion;
        String url;
        String sha256;
    };

    using EventCallback    = std::function<void(Event)>;
    using ProgressCallback = std::function<void(size_t written, size_t total)>;
    using ErrorCallback    = std::function<void(Error, const String& detail)>;

    class UpdateClient {
        private:
            const UpdateConfig _cfg;
            bool _started = false;
            uint32_t _lastCheck = 0;

            EventCallback _onEvent = nullptr;
            ProgressCallback _onProgress = nullptr;
            ErrorCallback _onError = nullptr;

            Manifest _lastManifest{};
            bool _hasManifest = false;

            bool _loadManifest(Manifest& out);
            bool _downloadAndApply(const Manifest& m);

            void _emit(Event e) { if (_onEvent) _onEvent(e); }
            void _emitError(Error e, const String& d = "") { if (_onError) _onError(e, d); }
            void _setupLanOta();

            void _startIfReady() { if (_started) return; if (WiFi.status() != WL_CONNECTED) return; _startNow(); }
            void _startNow();

        public:
            explicit UpdateClient(const UpdateConfig& cfg) : _cfg(cfg) { _emit(Event::Init); }

            void Control();

            bool CheckForUpdate(Manifest& out, bool* hasUpdate = nullptr, bool* forceUpdate = nullptr);
            bool CheckUpdateNow();
            bool UpdateFromURL(const String& url, const String& expectedSha256Hex = "");
            bool InstallLatest();

            inline String ManifestURL() const { return _cfg.manifestUrl; }
            inline String LatestVersion() const { return _lastManifest.version; }
            inline String LatestMinVersion() const { return _lastManifest.minVersion; }
            inline String LatestUrl() const { return _lastManifest.url; }
            inline String LatestSha256() const { return _lastManifest.sha256; }
            inline bool HasCachedManifest() const { return _hasManifest; }

            void OnEvent(EventCallback cb)    { _onEvent = cb; }
            void OnProgress(ProgressCallback cb){ _onProgress = cb; }
            void OnError(ErrorCallback cb)    { _onError = cb; }

            static bool IsNewer(const String& a, const String& b) { int aM=0,aN=0,aP=0, bM=0,bN=0,bP=0; sscanf(a.c_str(), "%d.%d.%d", &aM,&aN,&aP); sscanf(b.c_str(), "%d.%d.%d", &bM,&bN,&bP); if (aM!=bM) return aM>bM; if (aN!=bN) return aN>bN; return aP>bP;}
    };
}

#endif