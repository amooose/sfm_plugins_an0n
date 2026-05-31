#pragma once

#include <Windows.h>
#include <stdint.h>
#include <cstring>

#include "patch.h"

class TrampolineHook
{
public:
    TrampolineHook()
        : m_target(0),
        m_hook(nullptr),
        m_trampoline(nullptr),
        m_stolenLen(0),
        m_installed(false)
    {
        std::memset(m_originalBytes, 0, sizeof(m_originalBytes));
    }

    ~TrampolineHook()
    {
        Remove();
    }

    bool Install(uintptr_t target, void* hook, int stolenLen)
    {
        if (m_installed)
            return true;

        if (!target || !hook || stolenLen < 5)
            return false;

        if (stolenLen > (int)sizeof(m_originalBytes))
            return false;

        m_target = target;
        m_hook = hook;
        m_stolenLen = stolenLen;

        Patch::ReadBytes((void*)m_target, m_originalBytes, m_stolenLen);

        m_trampoline = VirtualAlloc(
            nullptr,
            m_stolenLen + 5,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE
        );

        if (!m_trampoline)
            return false;

        Patch::WriteBytes(m_trampoline, m_originalBytes, m_stolenLen);

        unsigned char* trampJmp = (unsigned char*)m_trampoline + m_stolenLen;
        trampJmp[0] = 0xE9;
        *(int*)(trampJmp + 1) =
            (int)((m_target + m_stolenLen) - ((uintptr_t)trampJmp + 5));

        Patch::WriteJump((void*)m_target, m_hook, 0);

        if (m_stolenLen > 5)
            Patch::WriteNoop((void*)(m_target + 5), m_stolenLen - 5);

        FlushInstructionCache(GetCurrentProcess(), (void*)m_target, m_stolenLen);
        FlushInstructionCache(GetCurrentProcess(), m_trampoline, m_stolenLen + 5);

        m_installed = true;
        return true;
    }

    bool Remove()
    {
        if (!m_installed)
            return true;

        Patch::WriteBytes((void*)m_target, m_originalBytes, m_stolenLen);
        FlushInstructionCache(GetCurrentProcess(), (void*)m_target, m_stolenLen);

        if (m_trampoline)
        {
            VirtualFree(m_trampoline, 0, MEM_RELEASE);
            m_trampoline = nullptr;
        }

        m_target = 0;
        m_hook = nullptr;
        m_stolenLen = 0;
        m_installed = false;
        std::memset(m_originalBytes, 0, sizeof(m_originalBytes));

        return true;
    }

    template <typename FnT>
    FnT GetOriginal() const
    {
        return reinterpret_cast<FnT>(m_trampoline);
    }

    void* GetTrampoline() const
    {
        return m_trampoline;
    }

    uintptr_t GetTarget() const
    {
        return m_target;
    }

    int GetStolenLen() const
    {
        return m_stolenLen;
    }

    bool IsInstalled() const
    {
        return m_installed;
    }

private:
    uintptr_t     m_target;
    void* m_hook;
    void* m_trampoline;
    int           m_stolenLen;
    bool          m_installed;
    unsigned char m_originalBytes[32];
};