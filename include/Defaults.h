#ifndef Defaults_h
#define Defaults_h

#pragma once

#include <Arduino.h>

static struct defaults {
    const uint32_t InitialTimeAndDate = 1708136755;

    struct update {
        const String ManifestURL = "https://server.dts-network.com:8081/update.json";
        const bool AllowInsecure = true;
        const bool Debug = true;
        const uint32_t CheckUpdateIntervalMs = 6UL * 60UL * 60UL * 1000UL;
        const bool AutoReboot = true;
        const bool CheckOnBoot = true;
    } Update;
} Defaults;

#endif