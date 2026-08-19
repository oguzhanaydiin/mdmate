#include "FileIO.h"

#include <windows.h>

#include <fstream>
#include <vector>

namespace mdmate {

namespace {

std::wstring DecodeBytesUtf8OrAnsi(const std::vector<char>& bytes, size_t offset) {
    const char* data = bytes.data() + offset;
    const int byteCount = static_cast<int>(bytes.size() - offset);

    int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, byteCount, nullptr, 0);
    if (required > 0) {
        std::wstring out(static_cast<size_t>(required), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, byteCount, out.data(), required);
        return out;
    }

    required = MultiByteToWideChar(CP_ACP, 0, data, byteCount, nullptr, 0);
    std::wstring out(static_cast<size_t>(required), L'\0');
    if (required > 0) {
        MultiByteToWideChar(CP_ACP, 0, data, byteCount, out.data(), required);
    }
    return out;
}

std::vector<char> EncodeUtf8(const std::wstring& text) {
    const int required = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0,
                                             nullptr, nullptr);
    std::vector<char> bytes(static_cast<size_t>(required));
    if (required > 0) {
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), bytes.data(), required, nullptr,
                            nullptr);
    }
    return bytes;
}

}

bool LoadTextFile(const std::wstring& path, std::wstring& outText) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    const std::vector<char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (bytes.empty()) {
        outText.clear();
        return true;
    }

    if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB && static_cast<unsigned char>(bytes[2]) == 0xBF) {
        outText = DecodeBytesUtf8OrAnsi(bytes, 3);
        return true;
    }

    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF &&
        static_cast<unsigned char>(bytes[1]) == 0xFE) {
        const size_t wcharCount = (bytes.size() - 2) / sizeof(wchar_t);
        outText.assign(wcharCount, L'\0');
        memcpy(outText.data(), bytes.data() + 2, wcharCount * sizeof(wchar_t));
        return true;
    }

    outText = DecodeBytesUtf8OrAnsi(bytes, 0);
    return true;
}

bool SaveTextFileUtf8(const std::wstring& path, const std::wstring& text) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }

    const std::vector<char> bytes = EncodeUtf8(text);
    if (!bytes.empty()) {
        file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    return static_cast<bool>(file);
}

}
