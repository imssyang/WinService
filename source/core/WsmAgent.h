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
    bool SetStatus(DWORD currentState, DWORD win32ExitCode, DWORD waitHint);
    void WriteToPipe(void);
    void ReadFromPipe(void);
    static VOID CALLBACK ReadPipeCompletion(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);
    BOOL CreatePipeEx(OUT LPHANDLE lpReadPipe, OUT LPHANDLE lpWritePipe,
    IN LPSECURITY_ATTRIBUTES lpPipeAttributes, IN DWORD nSize,
    DWORD dwReadMode, DWORD dwWriteMode);


private:
    HANDLE _stopEvent;
    SERVICE_STATUS _status;
    SERVICE_STATUS_HANDLE _statusHandle;
    HANDLE _stdInRead;
    HANDLE _stdInWrite;
    HANDLE _stdOutRead;
    HANDLE _stdOutWrite;
    HANDLE _readEvent;
    BYTE *_buf;
};
