#include "util/WSArg.h"
#include "core/WSAgent.h"
#include "core/WSGeneral.h"

#pragma comment(lib, "User32.lib")

WSAgent::WSAgent(const std::string& name, const std::string& alias):
    WSApp(name, alias),
    stopEvent_(NULL),
    svcStatusHandle_(NULL),
    stdOutRead_(NULL),
    stdOutWrite_(NULL)
{
}

WSAgent::~WSAgent()
{
    if (stdOutWrite_)
        CloseHandle(stdOutWrite_);
    if (stdOutRead_)
        CloseHandle(stdOutRead_);
}

bool WSAgent::Install(const std::string& path)
{
    CHAR unquotedPath[MAX_PATH];
    if (!GetModuleFileName(NULL, unquotedPath, MAX_PATH)) {
        SPDLOG_ERROR("GetModuleFileName failed! WinApi@");
        return false;
    }

    std::string agentPath = "\"";
    agentPath += unquotedPath;
	agentPath += "\" /RunAsService \"";
	agentPath += GetName();
	agentPath += "\" ";
	agentPath += path;
    return WSApp::Install(agentPath);
}

std::string WSAgent::GetPath() const
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

void WSAgent::Dispatch()
{
    SERVICE_TABLE_ENTRY tableEntry[] = {
        {(LPTSTR)GetName().data(), ServiceMainProc},
        {NULL, NULL}
    };

    if (!StartServiceCtrlDispatcher(tableEntry)) {
        SPDLOG_ERROR("StartServiceCtrlDispatcher failed! WinApi@");
    }
}

VOID WINAPI WSAgent::ServiceMainProc(DWORD argc, LPTSTR *argv)
{
    std::string name = (LPSTR)argv[0];
    auto& app = WSAgent(name);
    app.GetConfig();

    app.svcStatusHandle_ = RegisterServiceCtrlHandlerEx(
        (LPTSTR)app.GetName().data(), CtrlHandlerProc, &app);
    if (!app.svcStatusHandle_) {
        SPDLOG_ERROR("RegisterServiceCtrlHandlerEx failed! WinApi@");
        return;
    }

    app.svcStatus_.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    app.svcStatus_.dwServiceSpecificExitCode = 0;
    app.SetStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    app.stopEvent_ = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (app.stopEvent_ == NULL) {
        app.SetStatus(SERVICE_STOPPED, GetLastError(), 0);
        SPDLOG_ERROR("CreateEvent failed! WinApi@");
        return;
    }

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;
    if (!CreatePipe(&app.stdOutRead_, &app.stdOutWrite_, &sa, 0)) {
        SPDLOG_ERROR("CreatePipe failed! WinApi@");
        return;
    }

    if (!SetHandleInformation(app.stdOutRead_, HANDLE_FLAG_INHERIT, 0) ) {
        SPDLOG_ERROR("SetHandleInformation failed! WinApi@");
        return;
    }

    DWORD dwReadThreadId;
    HANDLE hReadThread = CreateThread(NULL, 0, StdReadThread, &app, 0, &dwReadThreadId);
    if (hReadThread == NULL) {
        SPDLOG_ERROR("CreateThread failed! WinApi@");
        return;
    }

	PROCESS_INFORMATION pi = {0,};
	STARTUPINFO si = {sizeof(STARTUPINFO),};
    si.hStdError = app.stdOutWrite_;
    si.hStdOutput = app.stdOutWrite_;
    si.dwFlags |= STARTF_USESTDHANDLES;
	si.wShowWindow = SW_HIDE;
	if (!CreateProcess(NULL, (LPTSTR)app.GetPath().data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        SPDLOG_ERROR("CreateProcess failed! WinApi@");
        CloseHandle(hReadThread);
        return;
    }

    SPDLOG_INFO("cmd: [{}] pid: [{}] tid: [{}] read-tid:[{}]",
        app.GetPath(), pi.dwProcessId, pi.dwThreadId, dwReadThreadId);

    app.SetStatus(SERVICE_RUNNING, NO_ERROR, 0);

    while (true) {
        HANDLE waitHandles[3] = {app.stopEvent_, pi.hProcess, hReadThread};
        DWORD dwEvent = WaitForMultipleObjects(ARRAYSIZE(waitHandles), waitHandles, FALSE, 1000000);
        switch (dwEvent) {
            case WAIT_OBJECT_0 + 0:
                SPDLOG_INFO("Receive stop event, prepare stop child process: {}", pi.dwProcessId);
                CloseHandle(app.stdOutRead_);
                EnumWindows(WindowCloserProc, pi.dwThreadId);
                if (WaitForSingleObject(pi.hProcess, 2000) != WAIT_OBJECT_0)
                    TerminateProcess(pi.hProcess, -1);
                ExitProcess(0);
                break;
            case WAIT_OBJECT_0 + 1:
                SPDLOG_INFO("Child process({}) exit.", pi.dwProcessId);
                CloseHandle(app.stdOutRead_);
                ExitProcess(0);
                break;
            case WAIT_OBJECT_0 + 2:
                SPDLOG_INFO("Receive read event.");
                break;
            case WAIT_TIMEOUT:
                SPDLOG_INFO("Wait timed out.");
                break;
            default:
                SPDLOG_INFO("Unknown error.");
        }
    }
}

DWORD WINAPI WSAgent::CtrlHandlerProc(
    DWORD control,
    DWORD eventType,
    LPVOID eventData,
    LPVOID context)
{
    WSAgent& app = *(WSAgent*)context;
    switch (control) {
        case SERVICE_CONTROL_STOP:
            app.SetStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
            SetEvent(app.stopEvent_);
            app.SetStatus(app.svcStatus_.dwCurrentState, NO_ERROR, 0);
            break;

        case SERVICE_CONTROL_INTERROGATE:
            break;

        default:
            break;
    }
    return 0;
}

DWORD WINAPI WSAgent::StdReadThread(LPVOID lpParam)
{
    auto& app = *reinterpret_cast<WSAgent*>(lpParam);
    CHAR chBuf[2048];
    DWORD dwRead;
    for (int count = 0;; count++) {
        BOOL bSuccess = ReadFile(app.stdOutRead_, chBuf, 2048, &dwRead, NULL);
        if (!bSuccess || dwRead == 0) {
            SPDLOG_ERROR("ReadFile failed! WinApi@");
            break;
        }

        std::string buffer(chBuf);
        buffer.erase(std::remove_if(buffer.begin(), buffer.end(),
            [](char c){ return std::isspace(c) && c != ' '; }), buffer.end());
        SPDLOG_INFO("ReadFile:[{}] length:{}", buffer, buffer.length());
    }

    return 0;
}

BOOL CALLBACK WSAgent::WindowCloserProc(HWND hWnd, LPARAM lParam)
{
    if ((GetWindowThreadProcessId(hWnd, NULL) == lParam) && !(GetWindowLong(hWnd, GWL_STYLE) & WS_CHILD))
        PostMessage(hWnd, WM_CLOSE, 0, 0);
    return TRUE;
}

bool WSAgent::SetStatus(DWORD currentState, DWORD win32ExitCode, DWORD waitHint)
{
    static DWORD dwCheckPoint = 1;

    svcStatus_.dwCurrentState = currentState;
    svcStatus_.dwWin32ExitCode = win32ExitCode;
    svcStatus_.dwWaitHint = waitHint;

    if (currentState == SERVICE_START_PENDING)
        svcStatus_.dwControlsAccepted = 0;
    else
        svcStatus_.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    if (currentState == SERVICE_RUNNING
        || currentState == SERVICE_STOPPED)
        svcStatus_.dwCheckPoint = 0;
    else
        svcStatus_.dwCheckPoint = dwCheckPoint++;

    return SetServiceStatus(svcStatusHandle_, &svcStatus_);
}
