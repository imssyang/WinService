#pragma once

#include "core/WsmApp.h"

class WsmAgent : public WsmApp
{
public:
    WsmAgent(const std::string& name, const std::string& alias = "");
    ~WsmAgent();

    bool Install(const std::string& path) override;
    void Dispatch();

    std::string GetPath() const override;

private:
	static BOOL CALLBACK WindowCloserProc(HWND hWnd, LPARAM lParam);
    static VOID WINAPI ServiceMainProc(DWORD argc, LPTSTR *argv);
    static DWORD WINAPI CtrlHandlerProc(
        DWORD control, DWORD eventType, LPVOID eventData, LPVOID context);
    static DWORD WINAPI StdReadThread(LPVOID lpParam);
    bool SetStatus(DWORD currentState, DWORD win32ExitCode, DWORD waitHint);
    void WriteToPipe(void);
    void ReadFromPipe(void);

private:
    SERVICE_STATUS svcStatus_;
    SERVICE_STATUS_HANDLE svcStatusHandle_;
    HANDLE stdOutRead_;
    HANDLE stdOutWrite_;
    HANDLE stopEvent_;
};
