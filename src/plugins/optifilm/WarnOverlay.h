#pragma once
#include <windows.h>

class WarnOverlay
{
public:
    static WarnOverlay& Get();

    void Start(); 
    void NotifyFault();

private:
    WarnOverlay() = default;
    HANDLE m_wake = nullptr;
    static DWORD WINAPI ThreadProc(LPVOID self);
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    void Run();

    volatile ULONGLONG m_lastFaultTick = 0;
    volatile LONG      m_started = 0;
};