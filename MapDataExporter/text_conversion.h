#pragma once

#include <string>
#include <windows.h>

inline std::wstring ToWide(const std::string& text)
{
    if (text.empty())
    {
        return {};
    }
    std::wstring result(MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), static_cast<int>(result.size()));
    return result;
}

inline std::string ToUtf8(const std::wstring& text)
{
    if (text.empty())
    {
        return {};
    }
    std::string result(WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), static_cast<int>(result.size()), nullptr, nullptr);
    return result;
}
