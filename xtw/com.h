/// @file
/// @brief  xtw::com
/// @author (C) 2022 ttsuki
/// Distributed under the Boost Software License, Version 1.0.

#pragma once

#include <Windows.h>
#include <combaseapi.h>

#include <string>
#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <memory>
#include <type_traits>
#include <utility>

// com_util
namespace xtw
{
    static inline HRESULT CoInitializeSTA()
    {
        return ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE | COINIT_SPEED_OVER_MEMORY);
    }

    static inline HRESULT CoInitializeMTA()
    {
        return ::CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE | COINIT_SPEED_OVER_MEMORY);
    }

    static inline void CoUninitialize()
    {
        ::CoUninitialize();
    }
}

// com_util
namespace xtw
{
    static inline std::wstring to_wstring(GUID guid)
    {
        OLECHAR buf[64]{};
        auto wrote = ::StringFromGUID2(guid, buf, 63);
        return std::wstring(buf, buf + wrote - 1);
    }

    static inline std::string to_string(GUID guid)
    {
        std::wstring o = to_wstring(guid); // assumes no non-ascii character.
        std::string s;
        s.reserve(o.size());
        std::transform(o.begin(), o.end(), std::back_insert_iterator(s), [](wchar_t w) { return static_cast<char>(w); });
        return s;
    }
}

// com_ptr
namespace xtw
{
    template <class TInterface>
    class com_ptr final
    {
        TInterface* pointer_{};

    public:
        // ctor (nullptr)
        constexpr com_ptr(nullptr_t = nullptr) noexcept {}

        // ctor from raw pointer with AddRef
        com_ptr(TInterface* ptr) noexcept
        {
            this->reset(ptr);
        }

        // copy ctor with AddRef
        com_ptr(const com_ptr& other) noexcept
        {
            this->reset(other.get());
        }

        // copy assign with AddRef
        com_ptr& operator=(const com_ptr& other) noexcept
        {
            if (this == std::addressof(other)) return *this;
            this->reset(other.get());
            return *this;
        }

        // up-cast copy ctor with AddRef
        template <class SInterface, std::enable_if_t<std::is_convertible_v<SInterface*, TInterface*>>* = nullptr>
        com_ptr(const com_ptr<SInterface>& ptr) noexcept
        {
            this->reset(ptr.get());
        }

        // up-cast copy assign with AddRef
        template <class SInterface, std::enable_if_t<std::is_convertible_v<SInterface*, TInterface*>>* = nullptr>
        com_ptr& operator=(const com_ptr<SInterface>& other) noexcept
        {
            this->reset(other.get());
            return *this;
        }


        // move ctor (without AddRef)
        com_ptr(com_ptr&& other) noexcept
        {
            pointer_ = std::exchange(other.pointer_, nullptr);
        }

        // move assign (without AddRef)
        com_ptr& operator=(com_ptr&& other) noexcept
        {
            if (this == std::addressof(other)) return *this;
            auto old = std::exchange(pointer_, std::exchange(other.pointer_, nullptr));
            if (old) { old->Release(); }
            return *this;
        }

        // with Release
        ~com_ptr()
        {
            if (pointer_) { pointer_->Release(); }
        }

        // without AddRef
        void attach(TInterface* ptr) noexcept
        {
            auto old = std::exchange(pointer_, ptr);
            if (old) { old->Release(); }
        }

        // without Release
        TInterface* detach() noexcept
        {
            return std::exchange(pointer_, nullptr);
        }

        // with AddRef
        void reset(TInterface* ptr = nullptr) noexcept
        {
            if (ptr) { ptr->AddRef(); }
            auto old = std::exchange(pointer_, ptr);
            if (old) { old->Release(); }
        }

        [[nodiscard]] TInterface* get() const noexcept
        {
            return pointer_;
        }

        // can be used with IID_PPV_ARGS
        [[nodiscard]] TInterface** put()
        {
            if (pointer_ != nullptr) throw std::logic_error("pointer is already set.");
            return &pointer_;
        }

        [[nodiscard]] void** put_void()
        {
            return reinterpret_cast<void**>(this->put());
        }

        [[nodiscard]] TInterface** reput()
        {
            reset(nullptr);
            return put();
        }

        [[nodiscard]] void** reput_void()
        {
            reset(nullptr);
            return put_void();
        }

        [[nodiscard]] TInterface* const* get_address() const { return &pointer_; }

        void operator&() = delete;
        void operator&() const = delete;
        template <class T> void operator[](T) = delete;
        template <class T> void operator[](T) const = delete;

        explicit operator bool() const noexcept { return pointer_; }

        struct __declspec(novtable) InterfaceProxy : public TInterface
        {
        private:
            virtual ULONG STDMETHODCALLTYPE AddRef() override = 0;  // make method private
            virtual ULONG STDMETHODCALLTYPE Release() override = 0; // make method private
        };

        InterfaceProxy* operator ->() const noexcept
        {
            return reinterpret_cast<InterfaceProxy*>(pointer_);
        }

        // convert to U with static_cast
        template <class UInterface, std::enable_if_t<std::is_convertible_v<TInterface*, UInterface*>>* = nullptr>
        [[nodiscard]] com_ptr<UInterface> as() const
        {
            return com_ptr<UInterface>(*this); // delegates to up-cast copy-constructor
        }

        // convert to U with QueryInterface
        template <class UInterface, std::enable_if_t<!std::is_convertible_v<TInterface*, UInterface*>>* = nullptr>
        [[nodiscard]] com_ptr<UInterface> as() const
        {
            if (!pointer_) return nullptr;
            com_ptr<UInterface> result{};
            HRESULT hr = pointer_->QueryInterface(__uuidof(UInterface), result.put_void()); // AddRef if succeeded
            if (SUCCEEDED(hr)) return result;
            if (hr == E_NOINTERFACE) return nullptr;
            return nullptr; // or throw
        }
    };

    static_assert(std::is_nothrow_move_assignable_v<com_ptr<IUnknown>>);
    static_assert(std::is_nothrow_move_constructible_v<com_ptr<IUnknown>>);
}

// com_task_mem_ptr
namespace xtw
{
    template <class T>
    class com_task_mem_ptr final
    {
        void* ptr_{};

    public:
        constexpr com_task_mem_ptr(nullptr_t = nullptr) noexcept {}
        constexpr explicit com_task_mem_ptr(T* ptr) noexcept : ptr_(ptr) {}
        com_task_mem_ptr(const com_task_mem_ptr& other) = delete;
        com_task_mem_ptr(com_task_mem_ptr&& other) noexcept : ptr_(std::exchange(other.ptr_, nullptr)) {}
        com_task_mem_ptr& operator=(const com_task_mem_ptr& other) = delete;

        com_task_mem_ptr& operator=(com_task_mem_ptr&& other) noexcept
        {
            if (this->ptr_ != other.ptr_) { this->reset(other.detach()); }
            return *this;
        }

        ~com_task_mem_ptr() { reset(nullptr); }

        [[nodiscard]] T* get() const noexcept { return static_cast<T*>(ptr_); }
        [[nodiscard]] T** put() noexcept { return reinterpret_cast<T**>(&ptr_); }
        [[nodiscard]] void** put_void() noexcept { return &ptr_; }
        T* operator ->() const noexcept { return get(); }
        T& operator *() const noexcept { return *get(); }

        T* detach() noexcept { return static_cast<T*>(std::exchange(ptr_, nullptr)); }

        void reset(T* ptr = nullptr) noexcept
        {
            if (ptr_) { ::CoTaskMemFree(std::exchange(ptr_, nullptr)); }
            this->ptr_ = ptr;
        }
    };

    static_assert(std::is_nothrow_move_assignable_v<com_task_mem_ptr<int>>);
    static_assert(std::is_nothrow_move_constructible_v<com_task_mem_ptr<int>>);
}
