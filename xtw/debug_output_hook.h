/// @file
/// @brief  xtw::debug
/// @author (C) 2022 ttsuki

#pragma once

#include <Windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "DbgHelp.lib")

// debug_output_hook
namespace xtw::debug
{
    static void (WINAPI* original_OutputDebugStringA)(LPCSTR) = reinterpret_cast<void(WINAPI*)(LPCSTR)>(::GetProcAddress(::GetModuleHandleA("kernel32.dll"), "OutputDebugStringA"));

    static void install_debug_output_hook(void (WINAPI* fun)(LPCSTR), LPCSTR target_module_name = nullptr)
    {
        ULONG size = 0;
        auto base = reinterpret_cast<BYTE*>(GetModuleHandleA(target_module_name));

        for (auto iid = static_cast<PIMAGE_IMPORT_DESCRIPTOR>(ImageDirectoryEntryToData(base, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &size)); iid->Name; iid++)
        {
            for (auto i = reinterpret_cast<PIMAGE_THUNK_DATA>(base + iid->FirstThunk); i->u1.Function; i++)
            {
                if (i->u1.Function == reinterpret_cast<decltype(i->u1.Function)>(original_OutputDebugStringA))
                {
                    if (MEMORY_BASIC_INFORMATION mbi{};
                        VirtualQuery(i, &mbi, sizeof(MEMORY_BASIC_INFORMATION)) != 0 &&
                        VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &mbi.Protect))
                    {
                        i->u1.Function = reinterpret_cast<decltype(i->u1.Function)>(fun);
                        VirtualProtect(mbi.BaseAddress, mbi.RegionSize, mbi.Protect, &mbi.Protect);
                    }
                }
            }
        }
    }
}
