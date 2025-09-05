#ifndef Version_h
#define Version_h

#pragma once

#include <Arduino.h>

static struct version {
    const String ProductFamily = "DeviceIQ";
    const String ProductName = "ESP32-Pro";
    struct software {
        const uint8_t Major = 1;
        const uint8_t Minor = 0;
        const uint8_t Revision = 1;
        inline String Info() { return String(Major) + "." + String(Minor) + "." + String(Revision); }
    } Software;
    struct hardware {
        const String Model = "ESP-32";
        const uint8_t Major = 1;
        const uint8_t Minor = 3;
        const uint8_t Revision = 2;
        inline String Info() { return String(Major) + "." + String(Minor) + "." + String(Revision); }
    } Hardware;
} Version;

#endif