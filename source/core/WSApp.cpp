#include "core/WSApp.h"
#include "core/WSGeneral.h"

WSApp::WSApp(const std::string& name, const std::string& alias)
    : name_(name), alias_(alias)
{
    if (alias_.empty())
        alias_ = name_;
}

WSApp::~WSApp()
{
}

bool WSApp::Install(const std::string& path)
{
    WSHandle wsHandle(SC_MANAGER_CREATE_SERVICE);
    if (!wsHandle.Check())
        return false;

    TCHAR executedPath[MAX_PATH];
    StringCbPrintf(executedPath, MAX_PATH, TEXT("%s"), path.data());
    SC_HANDLE service = CreateService(wsHandle.Manager,
        name_.data(),
        alias_.data(),
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        executedPath,
        NULL, NULL, NULL, NULL, NULL);
    if (!service) {
        SPDLOG_ERROR("CreateService failed! WinApi@");
        return false;
    }

    path_ = path;
    CloseServiceHandle(service);
    SPDLOG_INFO("Service installed successfully");
    return true;
}

bool WSApp::Uninstall()
{
    WSHandle wsHandle(SC_MANAGER_CREATE_SERVICE, DELETE, name_);
    if (!wsHandle.Check())
        return false;

    if (!DeleteService(wsHandle.Service)) {
        SPDLOG_ERROR("DeleteService failed! WinApi@");
        return false;
    }

    SPDLOG_INFO("Service deleted successfully");
    return true;
}

bool WSApp::SetDescription(const std::string& desc)
{
    if (desc.empty())
        return false;

    WSHandle wsHandle(SC_MANAGER_CREATE_SERVICE, SERVICE_CHANGE_CONFIG, name_);
    if (!wsHandle.Check())
        return false;

    SERVICE_DESCRIPTION sd;
    sd.lpDescription = (LPTSTR)desc.data();
    if (!ChangeServiceConfig2(
            wsHandle.Service,
            SERVICE_CONFIG_DESCRIPTION,
            &sd)) {
        SPDLOG_ERROR("ChangeServiceConfig2 failed! WinApi@");
        return false;
    }

    desc_ = desc;
    SPDLOG_INFO("Service description updated successfully.");
    return true;
}

std::optional<WSvcConfig> WSApp::GetConfig(bool hasDesc)
{
    std::optional<WSvcConfig> result = std::nullopt;
    LPQUERY_SERVICE_CONFIG lpsc = NULL;
    LPSERVICE_DESCRIPTION lpsd = NULL;

    WSHandle wsHandle(SC_MANAGER_ENUMERATE_SERVICE, SERVICE_QUERY_CONFIG, name_);
    if (!wsHandle.Check())
        goto svc_cleanup;

    DWORD bytesNeeded, bufSize, lastError;
    if (!QueryServiceConfig(wsHandle.Service, NULL, 0, &bytesNeeded)) {
        lastError = GetLastError();
        if (ERROR_INSUFFICIENT_BUFFER != lastError) {
            SPDLOG_ERROR("QueryServiceConfig failed ({})", lastError);
            goto svc_cleanup;
        }

        bufSize = bytesNeeded;
        lpsc = (LPQUERY_SERVICE_CONFIG) LocalAlloc(LMEM_FIXED, bufSize);
    }

    if (!QueryServiceConfig(wsHandle.Service, lpsc, bufSize, &bytesNeeded)) {
        SPDLOG_ERROR("QueryServiceConfig failed.");
        goto svc_cleanup;
    }

    if (hasDesc) {
        if (!QueryServiceConfig2(wsHandle.Service, SERVICE_CONFIG_DESCRIPTION, NULL, 0, &bytesNeeded)) {
            if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
                bufSize = bytesNeeded;
                lpsd = (LPSERVICE_DESCRIPTION) LocalAlloc(LMEM_FIXED, bufSize);
                if (!QueryServiceConfig2(wsHandle.Service, SERVICE_CONFIG_DESCRIPTION, (LPBYTE) lpsd, bufSize, &bytesNeeded)) {
                    SPDLOG_ERROR("QueryServiceConfig2 failed.");
                    goto svc_cleanup;
                }
            }
        }
    }

    if (!lpsd)
        lpsd = (LPSERVICE_DESCRIPTION) LocalAlloc(LMEM_ZEROINIT, 0);

    result = std::make_optional<WSvcConfig>(name_, *lpsc, *lpsd);

svc_cleanup:
    if (lpsc)
        LocalFree(lpsc);
    if (lpsd)
        LocalFree(lpsd);
    return std::move(result);
}

bool WSApp::Enable()
{
    return SetStartup(SERVICE_AUTO_START);
}

bool WSApp::Disable()
{
    return SetStartup(SERVICE_DISABLED);
}

bool WSApp::Start()
{
    WSHandle wsHandle(SC_MANAGER_ENUMERATE_SERVICE, SERVICE_ALL_ACCESS, name_);
    if (!wsHandle.Check())
        return false;

    std::optional<WSvcStatus> wssopt = GetStatus();
    if (!wssopt)
        return false;

    auto& wss = wssopt.value();
    if (wss.currentState != SERVICE_STOPPED
        && wss.currentState != SERVICE_STOP_PENDING) {
        SPDLOG_ERROR("Cannot start the service because it is already running");
        return false;
    }

    DWORD waitTime;
    DWORD startTickCount = GetTickCount();
    DWORD oldCheckPoint = wss.checkPoint;
    while (wss.currentState == SERVICE_STOP_PENDING) {
        waitTime = wss.waitHint / 10;
        if (waitTime < 1000)
            waitTime = 1000;
        else if (waitTime > 10000)
            waitTime = 10000;
        Sleep(waitTime);

        wssopt = GetStatus();
        if (!wssopt)
            return false;

        wss = wssopt.value();
        if (wss.checkPoint > oldCheckPoint) {
            startTickCount = GetTickCount();
            oldCheckPoint = wss.checkPoint;
        } else {
            if (GetTickCount() - startTickCount > wss.waitHint) {
                SPDLOG_ERROR("Timeout waiting for service to stop");
                return false;
            }
        }
    }

    if (!::StartService(wsHandle.Service, 0, NULL)) {
        SPDLOG_ERROR("StartService failed.");
        return false;
    }

    SPDLOG_INFO("Service start pending...");
    wssopt = GetStatus();
    if (!wssopt)
        return false;

    wss = wssopt.value();
    startTickCount = GetTickCount();
    oldCheckPoint = wss.checkPoint;
    while (wss.currentState == SERVICE_START_PENDING) {
        waitTime = wss.waitHint / 10;
        if( waitTime < 1000 )
            waitTime = 1000;
        else if ( waitTime > 10000 )
            waitTime = 10000;
        Sleep(waitTime);

        wssopt = GetStatus();
        if (!wssopt)
            return false;

        wss = wssopt.value();
        if (wss.checkPoint > oldCheckPoint) {
            startTickCount = GetTickCount();
            oldCheckPoint = wss.checkPoint;
        } else {
            if (GetTickCount()-startTickCount > wss.waitHint) {
                // No progress made within the wait hint.
                break;
            }
        }
    }

    if (wss.currentState != SERVICE_RUNNING) {
        SPDLOG_INFO("Service not started.");
        SPDLOG_INFO("  Current State: {}", wss.GetCurrentState());
        SPDLOG_INFO("  Exit Code: {}", wss.win32ExitCode);
        SPDLOG_INFO("  Check Point: {}", wss.checkPoint);
        SPDLOG_INFO("  Wait Hint: {}", wss.waitHint);
        return false;
    }

    SPDLOG_INFO("Service started successfully.");
    return true;
}

bool WSApp::Stop()
{
    WSHandle wsHandle(SC_MANAGER_ENUMERATE_SERVICE, SERVICE_STOP |
        SERVICE_QUERY_STATUS |
        SERVICE_ENUMERATE_DEPENDENTS, name_);
    if (!wsHandle.Check())
        return false;

    std::optional<WSvcStatus> wssopt = GetStatus();
    if (!wssopt)
        return false;

    auto& wss = wssopt.value();
    if (wss.currentState == SERVICE_STOPPED) {
        SPDLOG_INFO("Service is already stopped.");
        return false;
    }

    DWORD waitTime;
    DWORD startTime = GetTickCount();
    DWORD timeoutMS = 30000;
    while (wss.currentState == SERVICE_STOP_PENDING) {
        SPDLOG_INFO("Service stop pending...");
        waitTime = wss.waitHint / 10;
        if( waitTime < 1000 )
            waitTime = 1000;
        else if ( waitTime > 10000 )
            waitTime = 10000;
        Sleep(waitTime);

        wssopt = GetStatus();
        if (!wssopt)
            return false;

        wss = wssopt.value();
        if (wss.currentState == SERVICE_STOPPED) {
            SPDLOG_INFO("Service stopped successfully.");
            return false;
        }

        if (GetTickCount() - startTime > timeoutMS) {
            SPDLOG_INFO("Service stop timed out.");
            return false;
        }
    }

    // dependencies must be stopped first!
    StopDependents(wsHandle.Manager);

    SERVICE_STATUS_PROCESS ssp;
    if (!ControlService(wsHandle.Service, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS) &ssp)) {
        DWORD lastError = GetLastError();
        if (ERROR_BROKEN_PIPE != lastError) {
            SPDLOG_ERROR("ControlService failed.");
            return false;
        }
    }

    wss.currentState = ssp.dwCurrentState;
    wss.waitHint = ssp.dwWaitHint;
    while (wss.currentState != SERVICE_STOPPED) {
        Sleep(wss.waitHint);

        wssopt = GetStatus();
        if (!wssopt)
            return false;

        wss = wssopt.value();
        if (wss.currentState == SERVICE_STOPPED)
            break;

        if (GetTickCount() - startTime > timeoutMS) {
            SPDLOG_INFO("Wait timed out");
            return false;
        }
    }

    SPDLOG_INFO("Service stopped successfully");
    return true;
}

bool WSApp::SetDacl(const std::string& trustee)
{
    bool result = false;
    PACL newAcl = NULL;
    PSECURITY_DESCRIPTOR psd = NULL;

    WSHandle wsHandle(SC_MANAGER_CREATE_SERVICE, READ_CONTROL | WRITE_DAC, name_);
    if (!wsHandle.Check())
        goto dacl_cleanup;

    DWORD descSize = 0;
    DWORD bytesNeeded = 0;
    if (!QueryServiceObjectSecurity(wsHandle.Service,
            DACL_SECURITY_INFORMATION,
            &psd,
            0,
            &bytesNeeded)) {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            SPDLOG_ERROR("QueryServiceObjectSecurity failed.");
            goto dacl_cleanup;
        }

        descSize = bytesNeeded;
        psd = (PSECURITY_DESCRIPTOR)HeapAlloc(
            GetProcessHeap(), HEAP_ZERO_MEMORY, descSize);
        if (psd == NULL) {
            SPDLOG_INFO("HeapAlloc failed");
            goto dacl_cleanup;
        }

        if (!QueryServiceObjectSecurity(wsHandle.Service,
                DACL_SECURITY_INFORMATION,
                psd,
                descSize,
                &bytesNeeded)) {
            SPDLOG_ERROR("QueryServiceObjectSecurity failed");
            goto dacl_cleanup;
        }
    }

    PACL pacl = NULL;
    BOOL isDaclPresent = FALSE;
    BOOL isDaclDefaulted = FALSE;
    if (!GetSecurityDescriptorDacl(psd,
            &isDaclPresent,
            &pacl,
            &isDaclDefaulted)) {
        SPDLOG_ERROR("GetSecurityDescriptorDacl failed");
        goto dacl_cleanup;
    }

    EXPLICIT_ACCESS ea;
    BuildExplicitAccessWithName(&ea,
        (LPTSTR)trustee.data(),
        SERVICE_START | SERVICE_STOP | READ_CONTROL | DELETE,
        SET_ACCESS,
        NO_INHERITANCE);

    DWORD lastError = SetEntriesInAcl(1, &ea, pacl, &newAcl);
    if (lastError != ERROR_SUCCESS) {
        SPDLOG_ERROR("SetEntriesInAcl failed({})", lastError);
        goto dacl_cleanup;
    }

    SECURITY_DESCRIPTOR sd;
    if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) {
        SPDLOG_ERROR("InitializeSecurityDescriptor failed");
        goto dacl_cleanup;
    }

    if (!SetSecurityDescriptorDacl(&sd, TRUE, newAcl, FALSE)) {
        SPDLOG_ERROR("SetSecurityDescriptorDacl failed");
        goto dacl_cleanup;
    }

    if (!SetServiceObjectSecurity(wsHandle.Service, DACL_SECURITY_INFORMATION, &sd)) {
        SPDLOG_ERROR("SetServiceObjectSecurity failed");
        goto dacl_cleanup;
    }

    result = true;
    SPDLOG_INFO("Service DACL updated successfully");

dacl_cleanup:
    if(NULL != newAcl)
        LocalFree((HLOCAL)newAcl);
    if(NULL != psd)
        HeapFree(GetProcessHeap(), 0, (LPVOID)psd);
    return result;
}

bool WSApp::SetStartup(DWORD type)
{
    WSHandle wsHandle(SC_MANAGER_CREATE_SERVICE, SERVICE_CHANGE_CONFIG, name_);
    if (!wsHandle.Check())
        return false;

    if (!ChangeServiceConfig(
            wsHandle.Service,
            SERVICE_NO_CHANGE,
            type,
            SERVICE_NO_CHANGE,
            NULL, NULL, NULL, NULL, NULL, NULL, NULL)) {
        SPDLOG_ERROR("ChangeServiceConfig failed! WinApi@");
        return false;
    }

    SPDLOG_INFO("Service enabled successfully.");
    return true;
}

std::optional<WSvcStatus> WSApp::GetStatus()
{
    WSHandle wsHandle(SC_MANAGER_CREATE_SERVICE, SERVICE_CHANGE_CONFIG, name_);
    if (!wsHandle.Check())
        return std::nullopt;

    DWORD bytesNeeded;
    SERVICE_STATUS_PROCESS statusProcess;
    if (!QueryServiceStatusEx(
            wsHandle.Service,
            SC_STATUS_PROCESS_INFO,
            (LPBYTE) &statusProcess,
            sizeof(SERVICE_STATUS_PROCESS),
            &bytesNeeded)) {
        SPDLOG_ERROR("QueryServiceStatusEx failed! WinApi@");
        return std::nullopt;
    }

    auto result = std::make_optional<WSvcStatus>(name_, alias_, statusProcess);
    return std::move(result);
}

bool WSApp::StopDependents(SC_HANDLE manager)
{
    bool result = false;
    DWORD startTime = GetTickCount();
    DWORD timeoutMS = 30000;
    auto deps = GetDependents();
    for (DWORD i = 0; i < deps.size(); i++) {
        auto& wss = deps[i];
        if (wss.currentState == SERVICE_STOPPED)
            continue;

        SC_HANDLE depService = OpenService(manager, wss.serviceName.data(),
                                           SERVICE_STOP | SERVICE_QUERY_STATUS);
        if (!depService) {
            SPDLOG_ERROR("OpenService depend failed");
            return false;
        }

        SERVICE_STATUS_PROCESS ssp;
        if (!ControlService(depService, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS) &ssp)) {
            SPDLOG_ERROR("ControlService failed");
            CloseServiceHandle(depService);
            return false;
        }

        while (ssp.dwCurrentState != SERVICE_STOPPED) {
            Sleep(ssp.dwWaitHint);

            DWORD bytesNeeded;
            if (!QueryServiceStatusEx(depService, SC_STATUS_PROCESS_INFO, (LPBYTE) &ssp,
                                      sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded)) {
                SPDLOG_ERROR("QueryServiceStatusEx failed");
                CloseServiceHandle(depService);
                return false;
            }

            if (ssp.dwCurrentState == SERVICE_STOPPED)
                break;

            if (GetTickCount() - startTime > timeoutMS)
                CloseServiceHandle(depService);
                return false;
        }

        CloseServiceHandle(depService);
    }

    return result;
}

std::vector<WSvcStatus> WSApp::GetDependents()
{
    std::vector<WSvcStatus> result;
    LPENUM_SERVICE_STATUS lpDeps = NULL;

    WSHandle wsHandle(SC_MANAGER_ENUMERATE_SERVICE, SERVICE_ALL_ACCESS, name_);
    if (!wsHandle.Check())
        goto svcdepend_cleanup;

    DWORD bytesNeeded;
    DWORD returnedCount;
    if (EnumDependentServices(wsHandle.Service, SERVICE_STATE_ALL, lpDeps, 0, &bytesNeeded, &returnedCount)) {
        // no dependent services!
        goto svcdepend_cleanup;
    }

    if (GetLastError() != ERROR_MORE_DATA) {
        SPDLOG_ERROR("EnumDependentServices failed");
        goto svcdepend_cleanup;
    }

    lpDeps = (LPENUM_SERVICE_STATUS)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytesNeeded);
    if (!lpDeps) {
        SPDLOG_ERROR("HeapAlloc {} byte failed", bytesNeeded);
        goto svcdepend_cleanup;
    }

    if (!EnumDependentServices(wsHandle.Service, SERVICE_STATE_ALL, lpDeps, bytesNeeded, &bytesNeeded, &returnedCount)) {
        SPDLOG_ERROR("EnumDependentServices {} byte failed", bytesNeeded);
        goto svcdepend_cleanup;
    }

    for (DWORD i = 0; i < returnedCount; i++) {
        ENUM_SERVICE_STATUS ess = *(lpDeps + i);
        SERVICE_STATUS ss = ess.ServiceStatus;
        WSvcStatus svcStatus(ess.lpServiceName, ess.lpDisplayName, ss);
        result.push_back(std::move(svcStatus));
    }

svcdepend_cleanup:
    if (lpDeps)
        HeapFree(GetProcessHeap(), 0, lpDeps);
    return std::move(result);
}
