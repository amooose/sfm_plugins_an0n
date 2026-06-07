#include "WarnOverlay.h"


static const wchar_t* kWarnText = L".MDL will crash if loaded!";
extern volatile ULONG g_lastFaultTick;
WarnOverlay& WarnOverlay::Get()
{
    static WarnOverlay s_instance;
    return s_instance;
}

void WarnOverlay::Start()
{
    if (InterlockedExchange(&m_started, 1) != 0)
        return;
    m_wake = CreateEventW(nullptr, FALSE, FALSE, nullptr); // auto-reset
    CreateThread(nullptr, 0, &WarnOverlay::ThreadProc, this, 0, nullptr);
}

void WarnOverlay::NotifyFault()
{
    g_lastFaultTick = GetTickCount();
    if (m_wake) SetEvent(m_wake);   // wake the idle thread
}

DWORD WINAPI WarnOverlay::ThreadProc(LPVOID self)
{
    static_cast<WarnOverlay*>(self)->Run();
    return 0;
}

LRESULT CALLBACK WarnOverlay::WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT)
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc; GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH)); // black = keyed out
        SetBkMode(hdc, TRANSPARENT);
        SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));

        SetTextColor(hdc, RGB(20, 20, 20)); // outline (not pure black, so it shows)
        for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy)
            {
                RECT o = rc; OffsetRect(&o, dx, dy);
                DrawTextW(hdc, kWarnText, -1, &o, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
        SetTextColor(hdc, RGB(255, 50, 50));
        DrawTextW(hdc, kWarnText, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        EndPaint(hWnd, &ps);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wp, lp);
}
void WarnOverlay::Run()
{
    WNDCLASSW wc = {};
    wc.lpfnWndProc = &WarnOverlay::WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"SFMWarnOverlay";
    RegisterClassW(&wc);

    const int W = 280, H = 22;
    HWND hWnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wc.lpszClassName, L"", WS_POPUP,
        0, 0, W, H, nullptr, nullptr, wc.hInstance, nullptr);

    SetLayeredWindowAttributes(hWnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    bool visible = false;
    for (;;)
    {
        // wait for wake message so we arent constantly running
        WaitForSingleObject(m_wake, INFINITE);

        while ((GetTickCount() - g_lastFaultTick) < 100)
        {
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
                DispatchMessageW(&msg);

            HWND hDlg = FindWindowW(nullptr, L"Select .MDL File");
            HWND fg = GetForegroundWindow();
            bool dlgActive = hDlg && (fg == hDlg || GetAncestor(fg, GA_ROOTOWNER) == hDlg);

            if (hDlg && dlgActive)
            {
                RECT rc; GetWindowRect(hDlg, &rc);
                int dlgW = rc.right - rc.left;
                int x = rc.left + dlgW / 2 + (dlgW / 2 - (dlgW/6)) / 2;
                int dlgH = rc.bottom - rc.top;
                int y = rc.top + dlgH / 4;
                SetWindowPos(hWnd, HWND_TOPMOST, x, y, 0, 0,
                    SWP_NOSIZE | SWP_NOACTIVATE | (visible ? 0 : SWP_SHOWWINDOW));
                visible = true;
            }
            else if (visible)
            {
                ShowWindow(hWnd, SW_HIDE); 
                visible = false;
            }
            Sleep(16);
        }

        if (visible)
        {
            ShowWindow(hWnd, SW_HIDE);
            visible = false;
        }
    }
}