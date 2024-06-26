#pragma once

#include "core/wsapp.h"

class WSAgent : public WSApp
{
public:
    static std::string GetPath(const std::string& cmd);

    WSAgent(const std::string& name, const std::string& alias = "");
    ~WSAgent();

    bool Install(const std::string& path) override;
    void Dispatch();
    std::string GetPath() const override;

private:
    static VOID WINAPI ServiceMainProc(DWORD argc, LPTSTR *argv);
    static DWORD WINAPI CtrlHandlerProc(DWORD control, DWORD eventType, LPVOID eventData, LPVOID context);
    static DWORD WINAPI StdReadThread(LPVOID lpParam);
    static BOOL CALLBACK WindowCloserProc(HWND hWnd, LPARAM lParam);
    bool SetStatus(DWORD currentState, DWORD win32ExitCode, DWORD waitHint);

private:
    SERVICE_STATUS svcStatus_;
    SERVICE_STATUS_HANDLE svcStatusHandle_;
    HANDLE stdOutRead_;
    HANDLE stdOutWrite_;
    HANDLE stopEvent_;
};
