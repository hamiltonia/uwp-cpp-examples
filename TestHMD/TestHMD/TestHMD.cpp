// TestHMD.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windows.h>
#include <thread>
#include <string>
#include <sstream>
#include <iostream>
#include <memory>
#include <vector>
#include <conio.h>  
#include <tlhelp32.h>
#include <ppltasks.h>

using namespace Concurrency;
using namespace Windows::Foundation;
using namespace Windows::System;

#define MR_APP_NAME L"TestHMDApp.exe"
#define MR_APP_PROTOCOL L"testhmdapp:"
#define MR_PORTAL_PROTOCOL L"ms-holographicfirstrun:"

// The Title of the MR Portal Window
#define MR_PORTAL_CAPTION L"Mixed Reality Portal"

// desired monitor to position MR Portal (left coordinate of monitor)
#define MR_MONITOR_LEFT 1920 

class TestHMD
{
public:
    TestHMD()
        :m_quitting(false)
        ,m_thread(nullptr)
    {
    }

    ~TestHMD() 
    {
        Stop();
    }

    void Start() 
    { 
        m_quitting = false;
        m_thread = std::make_shared<std::thread>(&ThreadProc, this);
    }

    void Stop()
    {
        if(m_thread != nullptr)
        { 
            m_quitting = true;
            m_thread->join();
            m_thread.reset();
        }
    }

private:
    static void ThreadProc(TestHMD* h)
    {
        h->ThreadWorker();
    }

    void LaunchUWPApp(Platform::String^ protocol)
    {
        // Launch the Win32 App
        auto uri = ref new Uri(protocol); // The protocol handled by the launched app
        auto options = ref new LauncherOptions();
        concurrency::task<bool> task(Launcher::LaunchUriAsync(uri, options));
    }

    void ThreadWorker()
    {
        DWORD id = 0;
        bool portalRunning = IsProcessRunning(L"MixedRealityPortal.exe", id);
        DWORD delay = portalRunning ? 1000 : 10000;

        std::cout << "TestHMD:starting MR Portal..." << std::endl;
        LaunchUWPApp(MR_PORTAL_PROTOCOL);
        Sleep(delay);

        SendMRPortalEvents();
        Sleep(1000);
        std::cout << "TestHMD:starting MR App..." << std::endl;
        LaunchUWPApp(MR_APP_PROTOCOL);

        while (!m_quitting)
        {
            std::stringstream w;

            Sleep(1000);

            DWORD mrAppId = 0;
            bool mrAppRunning = IsProcessRunning(MR_APP_NAME, mrAppId);
            if (mrAppRunning)
            {
                std::cout << "MR App is running." << std::endl;
                continue;
            }
            else
            {
                w << "MR App is not running, ";
            }

            HWND hWnd = GetForegroundWindow();
            DWORD foregroundAppId = 0;
            GetWindowThreadProcessId(hWnd, &foregroundAppId);

            DWORD holoAppId = 0;
            bool holoAppRunning = IsProcessRunning(L"HoloShellApp.exe", holoAppId);

            DWORD portalAppId = 0;
            bool portalRunning = IsProcessRunning(L"MixedRealityPortal.exe", portalAppId);

            // w << "Foreground App:" << foregroundAppId << " HoloApp: " << holoAppId << ", ";
            if (holoAppRunning && (foregroundAppId == holoAppId))
            {
                w << "HMD is on, ";
                if (!mrAppRunning)
                {
                    w << "Launching MR App, ";
                    SendMRPortalEvents();
                    Sleep(1000);
                    LaunchUWPApp(MR_APP_PROTOCOL);
                }
            }
            else
            {
                w << "HMD is off, ";
            }

            if (portalRunning)
            {
                w << "portal is running, ";
            }
            else
            {
                w << "portal is not running, ";
            }

            std::cout << w.str() << std::endl;
        }
        std::cout << "TestHMD:threadworker exited" << std::endl;
    }

    static bool IsExtendedKey(WORD keyCode)
    {
        if (keyCode == VK_MENU ||
            keyCode == VK_LMENU ||
            keyCode == VK_RMENU ||
            keyCode == VK_CONTROL ||
            keyCode == VK_RCONTROL ||
            keyCode == VK_INSERT ||
            keyCode == VK_DELETE ||
            keyCode == VK_HOME ||
            keyCode == VK_END ||
            keyCode == VK_PRIOR ||
            keyCode == VK_NEXT ||
            keyCode == VK_RIGHT ||
            keyCode == VK_UP ||
            keyCode == VK_LEFT ||
            keyCode == VK_DOWN ||
            keyCode == VK_NUMLOCK ||
            keyCode == VK_CANCEL ||
            keyCode == VK_SNAPSHOT ||
            keyCode == VK_DIVIDE)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    bool IsProcessRunning(const wchar_t *processName, DWORD& id)
    {
        bool exists = false;
        PROCESSENTRY32 entry;
        entry.dwSize = sizeof(PROCESSENTRY32);
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

        if (Process32First(snapshot, &entry))
        {
            while (Process32Next(snapshot, &entry))
            {
                if (!_wcsicmp(entry.szExeFile, processName))
                {
                    exists = true;
                    id = entry.th32ProcessID;
                }
            }
        }

        CloseHandle(snapshot);
        return exists;
    }

    void SendKeyboardEvents(const std::vector<WORD>& keys)
    {
        std::vector<INPUT> inputs;

        for (const WORD& key : keys)
        {
            INPUT ip = { 0 };
            ip.type = INPUT_KEYBOARD;          
            ip.ki.wVk = key;
            ip.ki.dwFlags = IsExtendedKey(key) ? KEYEVENTF_EXTENDEDKEY : 0;
            inputs.push_back(ip);
        }

        // reverse iterate through vector to send Keyup events
        auto rit = keys.rbegin();
        for (; rit != keys.rend(); ++rit)
        {
            INPUT ip = { 0 };
            ip.type = INPUT_KEYBOARD;
            ip.ki.wVk = *rit;
            ip.ki.dwFlags = IsExtendedKey(*rit) ? KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP : KEYEVENTF_KEYUP;
            inputs.push_back(ip);
        }

        SendInput(static_cast<int>(inputs.size()), inputs.data(), sizeof(INPUT));
    }

    void SendMouseClick(LONG x, LONG y)
    {
        std::vector<INPUT> inputs;
        
        INPUT ip = { 0 };
        ip.type = INPUT_MOUSE;
        ip.mi.dx = x * (65536 / GetSystemMetrics(SM_CXSCREEN)); // x being coord in pixels
        ip.mi.dy = y * (65536 / GetSystemMetrics(SM_CYSCREEN)); // y being coord in pixels
        ip.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
        inputs.push_back(ip);

        ip.mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_ABSOLUTE;
        inputs.push_back(ip);
        ip.mi.dwFlags = MOUSEEVENTF_LEFTUP | MOUSEEVENTF_ABSOLUTE;
        inputs.push_back(ip);
        SendInput(static_cast<int>(inputs.size()), inputs.data(), sizeof(INPUT));
    }

    void MoveWindowToMonitor()
    {
        HWND hWnd = FindWindow(L"ApplicationFrameWindow", MR_PORTAL_CAPTION);
        if (hWnd)
        {
            HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTOPRIMARY);

            MONITORINFO info;
            info.cbSize = sizeof(MONITORINFO);
            GetMonitorInfo(monitor, &info);
            if (info.rcMonitor.left != MR_MONITOR_LEFT)
            {
                SendKeyboardEvents({ VK_LWIN, VK_SHIFT, VK_RIGHT });
                Sleep(1000);
                MoveWindowToMonitor();
            }
        }
    }

    void PressPlayButton()
    {
        HWND hWnd = FindWindow(L"ApplicationFrameWindow", L"Mixed Reality Portal");

        RECT r;
        GetWindowRect(hWnd, &r);
        LONG x = r.left + ((r.right - r.left)  / 2) + 15;
        LONG y = r.bottom - 40;
        SendMouseClick(x, y);
    }

    void SendMRPortalEvents()
    {
        std::cout << "TestHMD:sending window events..." << std::endl;

        // Send Win-Y twice to release MR Portal's focus
        SendKeyboardEvents({ VK_LWIN, 0x59 });
        Sleep(1000);
        SendKeyboardEvents({ VK_LWIN, 0x59 });
        Sleep(1000);
        // maximize the window
        SendKeyboardEvents({ VK_LWIN, VK_UP });
        Sleep(1000);
        MoveWindowToMonitor();
        Sleep(1000);
        PressPlayButton();
    }



    std::shared_ptr<std::thread> m_thread;
    bool m_quitting;
};

BOOL CALLBACK MonitorEnumProc(
    _In_ HMONITOR hMonitor,
    _In_ HDC      hdcMonitor,
    _In_ LPRECT   lprcMonitor,
    _In_ LPARAM   dwData
)
{
    MONITORINFO mi;
    static int count = 1;

    mi.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(hMonitor, &mi);
    std::cout << "Monitor " << count++ << ":";
    std::cout << " left: " << mi.rcMonitor.left;
    std::cout << " top: " << mi.rcMonitor.top;
    std::cout << " right: " << mi.rcMonitor.right;
    std::cout << " bottom: " << mi.rcMonitor.bottom;
    std::cout << std::endl;
    return true;
}


[Platform::MTAThread]
int main()
{
    TestHMD app;

    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc,NULL);

    std::cout << "Press q to exit app..." << std::endl;
    app.Start();
    while (auto c = _getch() != 'q');
    app.Stop();
    return 0;
}

