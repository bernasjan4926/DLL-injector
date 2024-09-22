#include <windows.h>
#include <iostream>
#include <string>
#include <psapi.h>
#include <VersionHelpers.h>
#include <atlstr.h>
#include <commdlg.h> // For file dialog

#define CREATE_THREAD_ACCESS (PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ)

LPCWSTR stringToLPCWSTR(const std::string& str) {
    size_t size = str.length() + 1;
    wchar_t* wstr = new wchar_t[size];
    size_t outSize;
    mbstowcs_s(&outSize, wstr, size, str.c_str(), size - 1);
    return wstr;
}

BOOL InjectDLL(DWORD ProcessID, LPCWSTR dllPath)
{
    LPVOID LoadLibAddy, RemoteString;

    if (!ProcessID)
        return FALSE;

    HANDLE Proc = OpenProcess(CREATE_THREAD_ACCESS, FALSE, ProcessID);

    if (!Proc)
    {
        std::cout << "OpenProcess() failed: " << GetLastError() << std::endl;
        return FALSE;
    }

    HMODULE hKernel32 = GetModuleHandle(L"kernel32.dll");
    if (!hKernel32)
    {
        std::cout << "GetModuleHandle() failed: " << GetLastError() << std::endl;
        CloseHandle(Proc);
        return FALSE;
    }

    LoadLibAddy = (LPVOID)GetProcAddress(hKernel32, "LoadLibraryW");

    if (!LoadLibAddy)
    {
        std::cout << "GetProcAddress() failed: " << GetLastError() << std::endl;
        CloseHandle(Proc);
        return FALSE;
    }

    size_t dllPathSize = (wcslen(dllPath) + 1) * sizeof(wchar_t);
    RemoteString = (LPVOID)VirtualAllocEx(Proc, NULL, dllPathSize, MEM_COMMIT, PAGE_READWRITE);
    if (!RemoteString)
    {
        std::cout << "VirtualAllocEx() failed: " << GetLastError() << std::endl;
        CloseHandle(Proc);
        return FALSE;
    }

    if (!WriteProcessMemory(Proc, RemoteString, (LPVOID)dllPath, dllPathSize, NULL))
    {
        std::cout << "WriteProcessMemory() failed: " << GetLastError() << std::endl;
        VirtualFreeEx(Proc, RemoteString, 0, MEM_RELEASE);
        CloseHandle(Proc);
        return FALSE;
    }

    HANDLE hThread = CreateRemoteThread(Proc, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibAddy, RemoteString, 0, NULL);
    if (!hThread)
    {
        std::cout << "CreateRemoteThread() failed: " << GetLastError() << std::endl;
        VirtualFreeEx(Proc, RemoteString, 0, MEM_RELEASE);
        CloseHandle(Proc);
        return FALSE;
    }

    CloseHandle(hThread);
    CloseHandle(Proc);

    return TRUE;
}

BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam) {
    DWORD dwThreadId, dwProcessId;
    char String[255];
    if (!hWnd)
        return TRUE;
    if (!::IsWindowVisible(hWnd))
        return TRUE;
    if (!SendMessage(hWnd, WM_GETTEXT, sizeof(String), (LPARAM)String))
        return TRUE;
    dwThreadId = GetWindowThreadProcessId(hWnd, &dwProcessId);
    std::cout << "PID: " << dwProcessId << '\t' << String << '\t' << std::endl;
    return TRUE;
}

std::wstring OpenFileDialog()
{
    OPENFILENAME ofn;
    wchar_t szFile[260] = { 0 };
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = L"DLL Files\0*.dll\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn) == TRUE)
    {
        return std::wstring(ofn.lpstrFile);
    }
    return L"";
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HWND hProcessIdEdit, hDllPathEdit, hInjectButton, hBrowseButton;
    static std::wstring dllPath;

    switch (uMsg)
    {
    case WM_CREATE:
        CreateWindow(L"STATIC", L"Process ID:", WS_VISIBLE | WS_CHILD, 10, 10, 100, 20, hwnd, NULL, NULL, NULL);
        hProcessIdEdit = CreateWindow(L"EDIT", NULL, WS_VISIBLE | WS_CHILD | WS_BORDER, 120, 10, 200, 20, hwnd, NULL, NULL, NULL);

        CreateWindow(L"STATIC", L"DLL Path:", WS_VISIBLE | WS_CHILD, 10, 40, 100, 20, hwnd, NULL, NULL, NULL);
        hDllPathEdit = CreateWindow(L"EDIT", NULL, WS_VISIBLE | WS_CHILD | WS_BORDER, 120, 40, 200, 20, hwnd, NULL, NULL, NULL);

        hBrowseButton = CreateWindow(L"BUTTON", L"Browse", WS_VISIBLE | WS_CHILD, 330, 40, 80, 20, hwnd, (HMENU)1, NULL, NULL);
        hInjectButton = CreateWindow(L"BUTTON", L"Inject DLL", WS_VISIBLE | WS_CHILD, 120, 70, 100, 30, hwnd, (HMENU)2, NULL, NULL);
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1) // Browse button
        {
            dllPath = OpenFileDialog();
            SetWindowText(hDllPathEdit, dllPath.c_str());
        }
        else if (LOWORD(wParam) == 2) // Inject button
        {
            wchar_t processIdStr[10];
            GetWindowText(hProcessIdEdit, processIdStr, 10);
            DWORD processId = _wtoi(processIdStr);

            if (InjectDLL(processId, dllPath.c_str()))
                MessageBox(hwnd, L"DLL injected successfully!", L"Success", MB_OK);
            else
                MessageBox(hwnd, L"DLL injection failed.", L"Error", MB_OK);
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"Sample Window Class";

    WNDCLASS wc = { };

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"DLL Injector",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 450, 150,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (hwnd == NULL)
    {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}