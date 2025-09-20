#include "Tools.h"

String CharArrayPointerToString(char* Text, uint32_t length) {
    String tmpRet;
    for (uint32_t i = 0; i < length; i++) tmpRet += (char)Text[i];
    return tmpRet;
}

String LimitString(String text, uint16_t sz, bool fill) {
    String ret = (text.length() <= sz ? text : String(text.substring(0, sz - 3) + "..."));
    uint16_t rlen = ret.length();

    if (fill)
        for (uint16_t s = 0; s < (sz - rlen); s++) ret += ' ';

    return ret;
}

String SString::LowerCase() {
    String source = *this;
    source.toLowerCase();
    return source;
}

String SString::UpperCase() {
    String source = *this;
    source.toUpperCase();
    return source;
}

// String SString::LimitString(size_t sz, bool fill) {
//     String ret = ((length() <= sz) ? *this : substring(0, sz - 3) + "...");
//     if (fill) for (size_t s = 0; s < (sz - ret.length()); s++) ret += ' ';
//     return ret;
// }

SString::SString(String str, size_t sz) {
    *this = str.substring(0, sz);
}

SString::SString(const char* str, size_t sz) {
    char tmpRet[sz];
    for (uint32_t i = 0; i < sz; i++) tmpRet[i] = (char)str[i];
    tmpRet[sz] = 0;
    *this = String(tmpRet);
}

SString::SString(const byte* str, size_t sz) {
    *this = SString((const char*)str, sz);
}

std::vector<String> SString::Tokenize(const char separator) {
    std::vector<String> tokens;
    int16_t separatorIndex = this->indexOf(separator);

    while (separatorIndex != -1) {
        String token = this->substring(0, separatorIndex);
        token.trim();
        tokens.push_back(token);
        *this = this->substring(separatorIndex + 1);
        separatorIndex = this->indexOf(separator);
    }

    this->trim();
    tokens.push_back(String(*this));

    return tokens;
}

bool hasValidHeaderToken(AsyncWebServerRequest *request, String api_token) {
    if (request->hasHeader("Authorization")) {
        const AsyncWebHeader* h = request->getHeader("Authorization");
        String token = h->value();
        token.trim();

        const String bearerPrefix = "Bearer ";
        if (token.startsWith(bearerPrefix)) {
            token = token.substring(bearerPrefix.length());
            token.trim();
        }

        return token.equals(api_token);
    }
    return false;
}

bool StreamFileAsBase64Json(String &fileName, WiFiClient &client, File &f, size_t fileSize, uint32_t crc32) {
    client.print(F("{\"Provider\":\"Orchestrator\",\"Command\":\"GetLog\",\"Parameter\":{"));
    client.print(F("\"Filename\":\""));
    client.print(fileName);
    client.print(F("\","));
    client.print(F("\"Size\":"));
    client.print(fileSize);
    client.print(F(",\"Encoding\":\"base64\","));
    client.print(F("\"Checksum\":\""));
    char crcbuf[9]; snprintf(crcbuf, sizeof(crcbuf), "%08X", crc32);
    client.print(crcbuf);
    client.print(F("\",\"Data\":\""));

    bool ok = true;

    static const size_t IN_CHUNK  = 3 * 256;   // múltiplo de 3
    static const size_t OUT_CHUNK = 4 * 256;

    std::unique_ptr<uint8_t[]> inBuf(new (std::nothrow) uint8_t[IN_CHUNK]);
    std::unique_ptr<unsigned char[]> outBuf(new (std::nothrow) unsigned char[OUT_CHUNK + 4]);

    if (!inBuf || !outBuf) {
        ok = false;
    }

    if (ok) {
        while (true) {
            size_t n = f.read(inBuf.get(), IN_CHUNK);
            if (n == 0) break;  // fim do arquivo

            size_t olen = 0;
            int ret = mbedtls_base64_encode(outBuf.get(), OUT_CHUNK + 4, &olen, inBuf.get(), n);
            if (ret != 0) { ok = false; break; }

            // Enviar todo o bloco codificado, respeitando backpressure
            size_t sent = 0;
            uint32_t t0 = millis();
            while (sent < olen) {
                if (!client.connected()) { ok = false; break; }

                int avail = client.availableForWrite();
                if (avail > 0) {
                    int toSend = (int)min<size_t>((size_t)avail, olen - sent);
                    int w = client.write(outBuf.get() + sent, toSend);
                    if (w < 0) { ok = false; break; }
                    sent += (size_t)w;
                    yield();
                } else {
                    delay(1); // dá fôlego ao Wi-Fi
                }

                // timeout por bloco (10s é bastante)
                if (millis() - t0 > 10000) { ok = false; break; }
            }

            if (!ok) break;
            yield();
        }
    }

    client.print(F("\"}}"));
    client.flush();
    return true;
}

uint32_t CRC32_Update(uint32_t crc, const uint8_t* data, size_t len) {
    crc = ~crc;
    while (len--) {
        crc ^= *data++;
        for (int k = 0; k < 8; k++) crc = crc & 1 ? (crc >> 1) ^ 0xEDB88320UL : (crc >> 1);
    }
    return ~crc;
}

uint32_t CRC32_File(File &f) {
    f.seek(0);
    uint32_t crc = 0;
    uint8_t buf[1024];
    while (true) {
        size_t n = f.read(buf, sizeof(buf));
        if (n == 0) break;
        crc = CRC32_Update(crc, buf, n);
    }
    f.seek(0);
    return crc;
}