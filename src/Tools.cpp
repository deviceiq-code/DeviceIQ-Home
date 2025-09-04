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