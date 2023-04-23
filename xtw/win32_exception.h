/// @file
/// @brief  xtw::hresult_exception
/// @author (C) 2022 ttsuki

#pragma once

#include <Windows.h>

#include <string>
#include <memory>
#include <stdexcept>

namespace xtw
{
    // for HRESULT, GetLastError
    static inline std::string get_system_error_message(HRESULT hr, HMODULE source = nullptr)
    {
        char* ptr = nullptr;
        const DWORD len = ::FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM |
            (source ? FORMAT_MESSAGE_FROM_HMODULE : 0),
            source,
            static_cast<DWORD>(hr),
            MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
            reinterpret_cast<char*>(&ptr), 0,
            nullptr);

        const auto p = std::unique_ptr<char, decltype(&::LocalFree)>(ptr, &::LocalFree);
        return len ? std::string(p.get(), len) : std::string("((error message is not resolved))");
    }

    class win32_exception : public std::runtime_error
    {
    public:
        explicit win32_exception(HRESULT hr, HMODULE source = nullptr)
            : runtime_error(std::string("com_error: ") + std::to_string(hr) + ":" + get_system_error_message(hr, source)) { }
    };
}
