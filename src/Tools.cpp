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

bool StreamFileAsBase64Json(String fileName, String macAddress, String command, WiFiClient &client, File &f, size_t fileSize, uint32_t crc32) {
    client.setNoDelay(true);

    client.print(F("{\"Provider\":\"Orchestrator\",\"Command\":\""));
    client.print(command);
    client.print(F("\",\"Parameter\":{\"MAC Address\":\""));
    client.print(macAddress);
    client.print(F("\","));
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

    // ======= ESSE helper é o que muda =======
    const uint32_t IDLE_TIMEOUT_MS = 15000;     // timeout por INATIVIDADE
    const size_t   MAX_TRY_CHUNK   = 1024;      // tamanho de tentativa (seguro p/ TCP)

    auto writeAll = [&](const uint8_t* buf, size_t len) -> bool {
        size_t sent = 0;
        uint32_t lastProgress = millis();
        while (sent < len) {
            if (!client.connected()) return false;

            size_t toSend = len - sent;
            if (toSend > MAX_TRY_CHUNK) toSend = MAX_TRY_CHUNK;

            int w = client.write(buf + sent, (int)toSend);  // tente SEM checar availableForWrite
            if (w > 0) {
                sent += (size_t)w;
                lastProgress = millis();         // reset no progresso
            } else if (w == 0) {
                // Sem progresso agora; ceda CPU e tente de novo
                delay(1);
            } else { // w < 0
                return false;
            }

            if (millis() - lastProgress > IDLE_TIMEOUT_MS) return false;
            yield();
        }
        return true;
    };
    // ========================================

    bool ok = true;

    static const size_t IN_CHUNK  = 3 * 128;    // múltiplo de 3 (menor = mais suave)
    static const size_t OUT_CHUNK = 4 * 128;

    std::unique_ptr<uint8_t[]> inBuf(new (std::nothrow) uint8_t[IN_CHUNK]);
    std::unique_ptr<unsigned char[]> outBuf(new (std::nothrow) unsigned char[OUT_CHUNK + 4]);
    if (!inBuf || !outBuf) ok = false;

    if (ok) {
        while (true) {
            size_t n = f.read(inBuf.get(), IN_CHUNK);
            if (n == 0) break;

            size_t olen = 0;
            int ret = mbedtls_base64_encode(outBuf.get(), OUT_CHUNK + 4, &olen, inBuf.get(), n);
            if (ret != 0) { ok = false; break; }

            if (!writeAll(outBuf.get(), olen)) { ok = false; break; }
            yield();
        }
    }

    if (client.connected()) {
        client.print(F("\"}}"));
    }

    return ok;
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