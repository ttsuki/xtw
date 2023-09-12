/// @file
/// @brief  xtw::handle
/// @author (C) 2022 ttsuki
/// Distributed under the Boost Software License, Version 1.0.

#pragma once
#include <Windows.h>
#include <memory>

namespace xtw
{
    struct default_handle_closer
    {
        void operator()(HANDLE p) const noexcept { if (p) ::CloseHandle(p); }
    };

    template <class HANDLE, class CLOSER>
    class unique_handle_t
    {
        std::unique_ptr<std::remove_pointer_t<HANDLE>, CLOSER> handle_;

    public:
        unique_handle_t() = default;
        unique_handle_t(nullptr_t) {};
        explicit unique_handle_t(HANDLE handle) : handle_(std::move(handle)) {}
        unique_handle_t(HANDLE handle, CLOSER closer) : handle_(std::move(handle), std::move(closer)) {}
        unique_handle_t(const unique_handle_t& other) = default;
        unique_handle_t(unique_handle_t&& other) noexcept = default;
        unique_handle_t& operator=(const unique_handle_t& other) = default;
        unique_handle_t& operator=(unique_handle_t&& other) noexcept = default;
        ~unique_handle_t() = default;

        [[nodiscard]] HANDLE get() const noexcept { return handle_.get(); }
        [[nodiscard]] CLOSER& get_deleter() noexcept { return handle_.get_deleter(); }
        [[nodiscard]] const CLOSER& get_deleter() const noexcept { return handle_.get_deleter(); }
        [[nodiscard]] HANDLE release() noexcept { return handle_.release(); }
        void reset(HANDLE h = nullptr) noexcept { return handle_.reset(h); }
        explicit operator bool() const noexcept { return static_cast<bool>(handle_); }
    };

    using unique_handle = unique_handle_t<HANDLE, default_handle_closer>;
}
