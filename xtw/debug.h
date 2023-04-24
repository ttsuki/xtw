/// @file
/// @brief  xtw::debug
/// @author (C) 2022 ttsuki

#pragma once

#include <Windows.h>

#include <cassert>

#include <chrono>
#include <string>
#include <functional>
#include <utility>
#include <streambuf>
#include <ostream>
#include <iomanip>

#include "win32_exception.h"

namespace xtw::debug
{
    namespace output_debug_stream_detail
    {
        template <class T>
        struct basic_callback_ostreambuf : std::basic_streambuf<T>
        {
            T buffer_[3072]{};
            size_t prefix_length_{};
            std::function<void(const T*)> callback_{};

            using base_type = std::basic_streambuf<char>;
            using char_type = typename base_type::char_type;
            using traits_type = typename base_type::traits_type;
            using int_type = typename base_type::int_type;
            using pos_type = typename base_type::pos_type;
            using off_type = typename base_type::off_type;

            template <class F>
            explicit basic_callback_ostreambuf(F f, const T* prefix = "")
                : callback_(std::move(f))
            {
                T* p = buffer_;
                for (char c : std::string_view("[YYYY-MM-DD HH:MM:SS.ffffff] "))
                    *p++ = static_cast<T>(c);

                while (*prefix)
                    *p++ = static_cast<T>(*prefix++);

                size_t buffer_size = std::size(buffer_) - (p - buffer_);
                base_type::setp(p, p + buffer_size - 2); // '\n' '\0'
            }

            basic_callback_ostreambuf(const basic_callback_ostreambuf& other) = delete;
            basic_callback_ostreambuf(basic_callback_ostreambuf&& other) noexcept = delete;
            basic_callback_ostreambuf& operator=(const basic_callback_ostreambuf& other) = delete;
            basic_callback_ostreambuf& operator=(basic_callback_ostreambuf&& other) noexcept = delete;
            ~basic_callback_ostreambuf() override { basic_callback_ostreambuf::sync(); }

            int_type overflow(int_type c) override
            {
                sync();
                if (c != traits_type::eof())
                {
                    *base_type::pptr() = traits_type::to_char_type(c);
                    base_type::pbump(1);
                }
                return traits_type::not_eof(c);
            }

            int sync() override
            {
                if (base_type::pbase() == base_type::pptr()) { return 0; }
                *base_type::pptr() = traits_type::to_char_type('\0');
                strtime_now(buffer_);
                if (*base_type::pptr() != '\n')
                    *base_type::pptr() = '\n';

                callback_(buffer_);
                base_type::pbump(static_cast<int>(base_type::pbase() - base_type::pptr()));
                return 0;
            }

            // YYYY-MM-DD hh:mm:ss.ffffff
            static inline void strtime_now(T out[28]) noexcept
            {
                using namespace std::chrono;
                auto now = system_clock::now();

                T* p = out;

                *p++ = static_cast<T>('['); // 1 character

                {
                    // 19 characters
                    char buf[20]{};
                    auto ti = system_clock::to_time_t(now);
                    std::tm tm{};
                    (void)localtime_s(&tm, &ti);
                    auto q = std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm); // YYYY-MM-DD HH:MM:SS
                    for (size_t i = 0; i < sizeof(buf) - 1; i++) *p++ = static_cast<T>(buf[i]);
                    assert(q == sizeof(buf) - 1);
                }

                {
                    // 7 characters
                    char buf[8]{};
                    static_assert(microseconds::period::num == 1 && microseconds::period::den == 1000000);
                    auto f = duration_cast<microseconds>(now.time_since_epoch()).count() * microseconds::period::num % microseconds::period::den;
                    auto q = std::snprintf(buf, sizeof(buf), ".%06d", static_cast<int>(f));
                    for (size_t i = 0; i < sizeof(buf) - 1; i++) *p++ = static_cast<T>(buf[i]);
                    assert(q == sizeof(buf) - 1);
                }

                *p++ = static_cast<T>(']'); // 1 character
            }
        };
    }

    class debug_output_stream final
        : private output_debug_stream_detail::basic_callback_ostreambuf<char>
        , public std::basic_ostream<char>
    {
    public:
        debug_output_stream(const char* prefix = "") : basic_callback_ostreambuf(OutputDebugStringA, prefix), basic_ostream(this) {}
        debug_output_stream(const debug_output_stream& other) = delete;
        debug_output_stream(debug_output_stream&& other) noexcept = delete;
        debug_output_stream& operator=(const debug_output_stream& other) = delete;
        debug_output_stream& operator=(debug_output_stream&& other) noexcept = delete;
        ~debug_output_stream() override = default;
    };
}

namespace xtw::debug
{
    struct null_output_stream {};

    template <class T>
    static inline constexpr null_output_stream operator <<(null_output_stream, T&&) noexcept { return {}; }
}

namespace xtw::debug
{
    // callback by `%` operator
    template <class F>
    struct percent_operator_redirection : F
    {
        constexpr explicit percent_operator_redirection(F f) : F(std::move(f)) {}
        template <class T> constexpr T operator %(T r) const { return (void)F::operator()(r), r; }
        template <class T> constexpr T operator %=(T r) const { return (void)F::operator()(r), r; }
    };
}


#ifndef NDEBUG
#define XTW_DEBUG_BREAK() (::IsDebuggerPresent() ? ::DebugBreak() : void(0))
#define XTW_DEBUG_LOG(...) (::xtw::debug::debug_output_stream{__VA_ARGS__})
#define XTW_TRACE_LOG(...) (::xtw::debug::debug_output_stream{__VA_ARGS__})
#define XTW_EXPECT_SUCCESS (::xtw::debug::percent_operator_redirection([](::HRESULT hr) { if (FAILED(hr)) { XTW_DEBUG_LOG("EXPECT_SUCCESS FAILED: ") << " at " << __FILE__ << ":" << __LINE__ << ": " << ::xtw::win32_exception(hr).what() << ")"; XTW_DEBUG_BREAK(); } }))%=
#else
#define XTW_DEBUG_BREAK() void(0)
#define XTW_DEBUG_LOG(...) (::xtw::debug::null_ostream{})
#define XTW_TRACE_LOG(...) (::xtw::debug::debug_output_stream{})
#define XTW_EXPECT_SUCCESS
#endif

#define XTW_THROW_ON_FAILURE (::xtw::debug::percent_operator_redirection([](::HRESULT hr) { if (FAILED(hr)) { XTW_DEBUG_BREAK(); throw ::xtw::win32_exception(hr); } }))%=
