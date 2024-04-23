#include "util/WsmArg.h"
#include "core/WsmAgent.h"
#include "core/WsmSvc.h"

#pragma comment(lib, "User32.lib")

WsmAgent::WsmAgent(const std::string& name, const std::string& alias):
    WsmApp(name, alias),
    _stopEvent(NULL),
    _statusHandle(NULL),
    _stdInRead(NULL),
    _stdInWrite(NULL),
    _stdOutRead(NULL),
    _stdOutWrite(NULL),
    _readEvent(NULL)
{
   SECURITY_ATTRIBUTES sa;
   sa.nLength = sizeof(SECURITY_ATTRIBUTES);
   sa.lpSecurityDescriptor = NULL;
   sa.bInheritHandle = TRUE;

   if (!CreatePipeEx(&_stdOutRead, &_stdOutWrite, &sa, 0, FILE_FLAG_OVERLAPPED, FILE_FLAG_OVERLAPPED)) {}
   if (!SetHandleInformation(_stdOutRead, HANDLE_FLAG_INHERIT, 0) ) {}
   //if (!CreatePipe(&_stdInRead, &_stdInWrite, &sa, 0)) {}
   //if (!SetHandleInformation(_stdInWrite, HANDLE_FLAG_INHERIT, 0)) {}

   _buf = new BYTE[1024];
}

WsmAgent::~WsmAgent()
{
    if (_stdOutWrite)
        CloseHandle(_stdOutWrite);
    if (_stdInRead)
        CloseHandle(_stdInRead);
    if (_stdInWrite)
        CloseHandle(_stdInWrite);
    if (_stdOutRead)
        CloseHandle(_stdOutRead);
}

void WsmAgent::WriteToPipe(void)
{
#define BUFSIZE 2048
   DWORD dwRead, dwWritten;
   CHAR chBuf[BUFSIZE];
   BOOL bSuccess = FALSE;

   for (;;) {
      //bSuccess = ReadFile(g_hInputFile, chBuf, BUFSIZE, &dwRead, NULL);
      //if ( ! bSuccess || dwRead == 0 ) break;

      bSuccess = WriteFile(_stdInWrite, chBuf, dwRead, &dwWritten, NULL);
      if (!bSuccess) break;
   }
}

void WsmAgent::ReadFromPipe(void)
{
#define BUFSIZE 2048
   DWORD dwRead, dwWritten;
   CHAR chBuf[BUFSIZE];
   BOOL bSuccess = FALSE;
   HANDLE hParentStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

   for (;;)
   {
      bSuccess = ReadFile(_stdOutRead, chBuf, BUFSIZE, &dwRead, NULL);
      if( ! bSuccess || dwRead == 0 ) break;

      bSuccess = WriteFile(hParentStdOut, chBuf,
                           dwRead, &dwWritten, NULL);
      if (! bSuccess ) break;
   }
}

bool WsmAgent::Install(const std::string& path)
{
    CHAR unquotedPath[MAX_PATH];
    if (!GetModuleFileName(NULL, unquotedPath, MAX_PATH)) {
        SPDLOG_ERROR("GetModuleFileName failed!");
        return false;
    }

    std::string agentPath = "\"";
    agentPath += unquotedPath;
	agentPath += "\" /RunAsService \"";
	agentPath += GetName();
	agentPath += "\" ";
	agentPath += path;
    return WsmApp::Install(agentPath);
}

std::string WsmAgent::GetPath() const
{
    std::string path;
    auto& cmd = ArgManager::Inst().Get("/RunAsService");
    auto argv = cmd.get<std::vector<std::string>>("argv");
    for (auto& arg : argv) {
        if (path.empty()) {
            path = arg;
        } else {
            path += " ";
            path += arg;
        }
    }
    return path;
}

void WsmAgent::Dispatch()
{
    SERVICE_TABLE_ENTRY tableEntry[] = {
        {(LPTSTR)GetName().data(), ServiceMainProc},
        {NULL, NULL}
    };

    if (!StartServiceCtrlDispatcher(tableEntry)) {
        SPDLOG_ERROR("StartServiceCtrlDispatcher");
    }
}

BOOL WsmAgent::CreatePipeEx(OUT LPHANDLE lpReadPipe, OUT LPHANDLE lpWritePipe,
    IN LPSECURITY_ATTRIBUTES lpPipeAttributes, IN DWORD nSize,
    DWORD dwReadMode, DWORD dwWriteMode)
{
    static volatile long PipeSerialNumber = 0;
    HANDLE ReadPipeHandle, WritePipeHandle;
    DWORD dwError;
    CHAR PipeNameBuffer[ MAX_PATH ];

    if ((dwReadMode | dwWriteMode) & (~FILE_FLAG_OVERLAPPED)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

  //
  //  Set the default timeout to 120 seconds
  //

  if (nSize == 0) {
    nSize = 1024;
  }

  sprintf( PipeNameBuffer,
           "\\\\.\\Pipe\\RemoteExeAnon.%08x.%08x",
           GetCurrentProcessId(),
           InterlockedIncrement(&PipeSerialNumber)
         );

  ReadPipeHandle = CreateNamedPipeA(
                       PipeNameBuffer,
                       PIPE_ACCESS_DUPLEX | dwReadMode,
                       PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                       PIPE_UNLIMITED_INSTANCES,             // Number of pipes
                       nSize,         // Out buffer size
                       nSize,         // In buffer size
                       30 * 1000,    // Timeout in ms
                       lpPipeAttributes
                       );

  if (! ReadPipeHandle) {
    return FALSE;
  }

  WritePipeHandle = CreateFileA(
                      PipeNameBuffer,
                      GENERIC_WRITE,
                      0,                         // No sharing
                      lpPipeAttributes,
                      OPEN_EXISTING,
                      FILE_ATTRIBUTE_NORMAL | dwWriteMode,
                      NULL                       // Template file
                    );

  if (INVALID_HANDLE_VALUE == WritePipeHandle) {
    dwError = GetLastError();
    CloseHandle( ReadPipeHandle );
    SetLastError(dwError);
    return FALSE;
  }

    *lpReadPipe = ReadPipeHandle;
    *lpWritePipe = WritePipeHandle;
    return TRUE;
}

VOID CALLBACK WsmAgent::ReadPipeCompletion(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
{
    SPDLOG_INFO("ReadPipeCompletion: {}", dwErrorCode);
    //auto& app = *reinterpret_cast<WsmAgent*>(lpOverlapped->hEvent);
    //if (dwErrorCode == ERROR_SUCCESS) {
    //    DWORD bytesRead = dwNumberOfBytesTransfered;
    //    SPDLOG_INFO("Read {} bytes from pipe: {}", bytesRead, (char*)app._buf);
    //} else {
    //    SPDLOG_INFO("Read pipe failed with error code: {}", dwErrorCode);
    //}
    // 设置读取完成事件
    //SetEvent(app._readEvent);
}

VOID WINAPI WsmAgent::ServiceMainProc(DWORD argc, LPTSTR *argv)
{
    std::string name = (LPSTR)argv[0];
    auto& app = WsmAgent(name);
    app.GetConfig();

    app._statusHandle = RegisterServiceCtrlHandlerEx(
        (LPTSTR)app.GetName().data(), CtrlHandlerProc, &app);
    if (!app._statusHandle) {
        SPDLOG_ERROR("RegisterServiceCtrlHandler");
        return;
    }

    app._status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    app._status.dwServiceSpecificExitCode = 0;
    app.SetStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    app._stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (app._stopEvent == NULL) {
        app.SetStatus(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

	PROCESS_INFORMATION pi = {0,};
	STARTUPINFO si = {sizeof(STARTUPINFO),};
    si.hStdError = app._stdOutWrite;
    si.hStdOutput = app._stdOutWrite;
    si.hStdInput = app._stdInRead;
    si.dwFlags |= STARTF_USESTDHANDLES;
	//si.wShowWindow = SW_SHOW;
	if (!CreateProcess(NULL, (LPTSTR)app.GetPath().data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
		return;
    SPDLOG_INFO("cmd: [{}] pid: [{}] tid: [{}]", app.GetPath(), pi.dwProcessId, pi.dwThreadId);

    app.SetStatus(SERVICE_RUNNING, NO_ERROR, 0);



    //OVERLAPPED overlapped = {};
    //overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    //char buffer[1024] = {0,};
    //BOOL result = ReadFileEx(app._stdOutRead, buffer, 16, &overlapped, ReadPipeCompletion);

    //app._readEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    OVERLAPPED overlapped = { 0 };
    overlapped.hEvent = reinterpret_cast<HANDLE>(&app);
    //DWORD bytesRead;
    //BOOL result = ReadFileEx(app._stdOutRead, app._buf, 16, &overlapped, ReadPipeCompletion);
    //if (!result) {
    //    DWORD lastError = GetLastError();
    //    if (lastError != ERROR_IO_PENDING) {
    //        //delete[] reinterpret_cast<BYTE*>(overlapped.hEvent);
    //        SPDLOG_ERROR("ReadFileEx lastError: {}", lastError);
    //        return;
    //    }
    //}
    while (true) {
        HANDLE waitHandles[2] = {pi.hProcess, app._stopEvent};
        DWORD dwEvent = WaitForMultipleObjects(ARRAYSIZE(waitHandles), waitHandles, FALSE, 1000);
        switch (dwEvent) {
            case WAIT_OBJECT_0 + 0:
                SPDLOG_INFO("Child process({}) exit.", pi.dwProcessId);
                ExitProcess(0);
                break;
            case WAIT_OBJECT_0 + 1:
                SPDLOG_INFO("Receive stop event, prepare stop child process: {}", pi.dwProcessId);
                EnumWindows(WindowCloserProc, pi.dwThreadId);
                if (WaitForSingleObject(pi.hProcess, 2000) != WAIT_OBJECT_0)
                    TerminateProcess(pi.hProcess, -1);
                ExitProcess(0);
                break;
            case WAIT_OBJECT_0 + 2:
                {
                    DWORD dwBytesTransferred = 0;
                    BOOL result = GetOverlappedResult(app._stdOutRead, &overlapped, &dwBytesTransferred, TRUE);
                    SPDLOG_INFO("Receive read event. {} {}", result, dwBytesTransferred);

                    //DWORD bytesAvailable;
                    //if (PeekNamedPipe(app._stdOutRead, NULL, 0, NULL, &bytesAvailable, NULL) && bytesAvailable > 0) {
                    //    DWORD dwRead = 0;
                    //    CHAR chBuf[1024] = {0,};
                    //    BOOL bSuccess = FALSE;
                    //    bSuccess = ReadFile(app._stdOutRead, chBuf, BUFSIZE, &dwRead, NULL);
                    //    SPDLOG_INFO("ok:{} read:{} buf:{}", bSuccess, dwRead, chBuf);
                    //}
                }
                break;
            case WAIT_TIMEOUT:
                SPDLOG_INFO("Wait timed out.");
                {
                    //DWORD dwBytesTransferred = 0;
                    //BOOL result = GetOverlappedResult(app._stdOutRead, &overlapped, &dwBytesTransferred, TRUE);
                    //SPDLOG_INFO("Receive timed2 event. {} {}", result, dwBytesTransferred);

                    //BOOL result2 = ReadFileEx(app._stdOutRead, app._buf, 16, &overlapped, ReadPipeCompletion);
                    //SPDLOG_INFO("Receive timed3 event. {}", result2);

                    DWORD dwRead = 0;
                    CHAR chBuf[1024] = {0,};
                    BOOL bSuccess = FALSE;
                    bSuccess = ReadFile(app._stdOutRead, chBuf, BUFSIZE, &dwRead, NULL);
                    SPDLOG_INFO("ok:{} read:{} buf:{}", bSuccess, dwRead, chBuf);
                    ExitProcess(0);
                }
                break;
            default:
                SPDLOG_INFO("Unknown error.");
        }
    }
}

DWORD WINAPI WsmAgent::CtrlHandlerProc(
    DWORD control,
    DWORD eventType,
    LPVOID eventData,
    LPVOID context)
{
    WsmAgent& app = *(WsmAgent*)context;
    switch (control) {
        case SERVICE_CONTROL_STOP:
            app.SetStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
            SetEvent(app._stopEvent);
            app.SetStatus(app._status.dwCurrentState, NO_ERROR, 0);
            break;

        case SERVICE_CONTROL_INTERROGATE:
            break;

        default:
            break;
    }
    return 0;
}

BOOL CALLBACK WsmAgent::WindowCloserProc(HWND hWnd, LPARAM lParam)
{
    if ((GetWindowThreadProcessId(hWnd, NULL) == lParam) && !(GetWindowLong(hWnd, GWL_STYLE) & WS_CHILD))
        PostMessage(hWnd, WM_CLOSE, 0, 0);
    return TRUE;
}

bool WsmAgent::SetStatus(DWORD currentState, DWORD win32ExitCode, DWORD waitHint)
{
    static DWORD dwCheckPoint = 1;

    _status.dwCurrentState = currentState;
    _status.dwWin32ExitCode = win32ExitCode;
    _status.dwWaitHint = waitHint;

    if (currentState == SERVICE_START_PENDING)
        _status.dwControlsAccepted = 0;
    else
        _status.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    if (currentState == SERVICE_RUNNING
        || currentState == SERVICE_STOPPED)
        _status.dwCheckPoint = 0;
    else
        _status.dwCheckPoint = dwCheckPoint++;

    return SetServiceStatus(_statusHandle, &_status);
}
