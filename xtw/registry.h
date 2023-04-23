/// @file
/// @brief  xtw::registry
/// @author (C) 2022 ttsuki

#pragma once

#include <Windows.h>
#include <combaseapi.h>
#include <cstddef>
#include <optional>
#include <string>

#include "./unique_handle.h"

namespace xtw::registry
{
    using registry_key_unique_handle = unique_handle_t<HKEY, decltype(&::RegCloseKey)>;

    static registry_key_unique_handle OpenKey(HKEY parent, const wchar_t* sub_key_name)
    {
        HKEY key{};
        if (::RegOpenKeyW(parent, sub_key_name, &key) != ERROR_SUCCESS) return {HKEY{}, &::RegCloseKey};
        return {key, &::RegCloseKey};
    }

    static std::optional<std::wstring> EnumKeyName(HKEY parent, size_t index)
    {
        WCHAR sub_key_name[256] = {};
        DWORD len = static_cast<DWORD>(std::size(sub_key_name)) - 1;
        if (::RegEnumKeyExW(parent, static_cast<DWORD>(index), sub_key_name, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) return std::nullopt;
        return std::optional<std::wstring>(std::in_place, sub_key_name);
    }

    static std::optional<std::wstring> ReadStringValue(HKEY key, const wchar_t* value_name)
    {
        WCHAR val[4096] = {};
        DWORD len = sizeof val - sizeof val[0];
        DWORD type{};
        if (::RegQueryValueExW(key, value_name, nullptr, &type, reinterpret_cast<LPBYTE>(val), &len) != ERROR_SUCCESS) return std::nullopt;
        if (type != REG_SZ && type != REG_EXPAND_SZ) return std::nullopt;
        return std::optional<std::wstring>(std::in_place, val);
    }

    static std::optional<std::string> ReadStringValueA(HKEY key, const char* value_name)
    {
        CHAR val[4096] = {};
        DWORD len = sizeof val - sizeof val[0];
        DWORD type{};
        if (::RegQueryValueExA(key, value_name, nullptr, &type, reinterpret_cast<LPBYTE>(val), &len) != ERROR_SUCCESS) return std::nullopt;
        if (type != REG_SZ && type != REG_EXPAND_SZ) return std::nullopt;
        return std::optional<std::string>(std::in_place, val);
    }

    static std::optional<GUID> ReadGuidValue(HKEY key, const wchar_t* value_name)
    {
        GUID val{};
        auto str = ReadStringValue(key, value_name);
        if (!str) return std::nullopt;
        if (FAILED(::IIDFromString(str->c_str(), &val))) return std::nullopt;
        return std::optional<GUID>(std::in_place, val);
    }
}
