#include "core/WsmApp.h"
#include "core/WsmSvc.h"

WsmApp::WsmApp(const std::string& name, const std::string& alias):
    _name(name),
    _alias(alias),
    _manager(NULL),
    _service(NULL)
{
    if (_alias.empty())
        _alias = _name;
}

WsmApp::~WsmApp()
{
    if (_service)
        CloseServiceHandle(_service);
}

bool WsmApp::Install(const std::string& path)
{
    if (!Init(TRUE, FALSE))
        return false;

    TCHAR executedPath[MAX_PATH];
    StringCbPrintf(executedPath, MAX_PATH, TEXT("%s"), path.data());

    _service = CreateService(_manager,
        _name.data(),
        _alias.data(),
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        executedPath,
        NULL, NULL, NULL, NULL, NULL);
    if (_service == NULL) {
        SPDLOG_ERROR("CreateService failed!");
        return false;
    }

    _path = path;
    SPDLOG_INFO("Service installed successfully");
    return true;
}

bool WsmApp::Uninstall()
{
    if (!Init(TRUE, TRUE, DELETE))
        return false;

    if (!DeleteService(_service)) {
        SPDLOG_ERROR("DeleteService failed.");
        return false;
    }

    SPDLOG_INFO("Service deleted successfully");
    return TRUE;
}

bool WsmApp::SetDescription(const std::string& desc)
{
    if (desc.empty())
        return false;

    if (!Init(TRUE, TRUE, SERVICE_CHANGE_CONFIG))
        return false;

    SERVICE_DESCRIPTION sd;
    sd.lpDescription = (LPTSTR)desc.data();
    if (!ChangeServiceConfig2(_service,
            SERVICE_CONFIG_DESCRIPTION,
            &sd)) {
        SPDLOG_ERROR("ChangeServiceConfig2 failed");
        return false;
    }

    _desc = desc;
    SPDLOG_INFO("Service description updated successfully.");
    return true;
}

std::optional<WsmSvcConfig> WsmApp::GetConfig()
{
    std::optional<WsmSvcConfig> result = std::nullopt;

    if (!Init(TRUE, TRUE, SERVICE_QUERY_CONFIG))
        goto svcconfig_cleanup;

    LPQUERY_SERVICE_CONFIG lpsc = NULL;
    DWORD bytesNeeded, bufSize, lastError;
    if (!QueryServiceConfig(_service, NULL, 0, &bytesNeeded)) {
        lastError = GetLastError();
        if (ERROR_INSUFFICIENT_BUFFER != lastError) {
            SPDLOG_ERROR("QueryServiceConfig failed ({})", lastError);
            goto svcconfig_cleanup;
        }

        bufSize = bytesNeeded;
        lpsc = (LPQUERY_SERVICE_CONFIG) LocalAlloc(LMEM_FIXED, bufSize);
    }

    if (!QueryServiceConfig(_service, lpsc, bufSize, &bytesNeeded)) {
        SPDLOG_ERROR("QueryServiceConfig failed.");
        goto svcconfig_cleanup;
    }

    LPSERVICE_DESCRIPTION lpsd = NULL;
    if (!QueryServiceConfig2(_service,
            SERVICE_CONFIG_DESCRIPTION,
            NULL,
            0,
            &bytesNeeded)) {
        lastError = GetLastError();
        if (ERROR_INSUFFICIENT_BUFFER != lastError) {
            //SPDLOG_ERROR("QueryServiceConfig2 failed");
            //goto svcconfig_cleanup;
        }

        bufSize = bytesNeeded;
        lpsd = (LPSERVICE_DESCRIPTION) LocalAlloc(LMEM_FIXED, bufSize);
    }

    if (!QueryServiceConfig2(_service,
            SERVICE_CONFIG_DESCRIPTION,
            (LPBYTE) lpsd,
            bufSize,
            &bytesNeeded)) {
        //SPDLOG_ERROR("QueryServiceConfig2 failed.");
        //goto svcconfig_cleanup;
    }

    result = std::make_optional<WsmSvcConfig>();
    result.value().init(_name, *lpsc, *lpsd);

svcconfig_cleanup:
    if (lpsc)
        LocalFree(lpsc);
    if (lpsd)
        LocalFree(lpsd);
    return std::move(result);
}

bool WsmApp::Enable()
{
    return SetStartup(SERVICE_AUTO_START);
}

bool WsmApp::Disable()
{
    return SetStartup(SERVICE_DISABLED);
}

bool WsmApp::Start()
{
    if (!Init(TRUE, TRUE, SERVICE_ALL_ACCESS))
        return false;

    auto wssopt = GetStatus();
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

    if (!::StartService(_service, 0, NULL)) {
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
        SPDLOG_INFO("  Current State: {}", wss.getCurrentState());
        SPDLOG_INFO("  Exit Code: {}", wss.win32ExitCode);
        SPDLOG_INFO("  Check Point: {}", wss.checkPoint);
        SPDLOG_INFO("  Wait Hint: {}", wss.waitHint);
        return false;
    }

    SPDLOG_INFO("Service started successfully.");
    return true;
}

bool WsmApp::Stop()
{
    if (!Init(TRUE, TRUE, SERVICE_STOP |
            SERVICE_QUERY_STATUS |
            SERVICE_ENUMERATE_DEPENDENTS))
        return false;

    auto wssopt = GetStatus();
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
    StopDependents();

    SERVICE_STATUS_PROCESS ssp;
    if (!ControlService(_service, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS) &ssp)) {
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

bool WsmApp::SetDacl(const std::string& trustee)
{
    if (!Init(TRUE, TRUE, READ_CONTROL | WRITE_DAC))
        return false;

    bool result = false;
    DWORD descSize = 0;
    DWORD bytesNeeded = 0;
    PSECURITY_DESCRIPTOR psd = NULL;
    if (!QueryServiceObjectSecurity(_service,
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

        if (!QueryServiceObjectSecurity(_service,
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

    PACL newAcl = NULL;
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

    if (!SetServiceObjectSecurity(_service, DACL_SECURITY_INFORMATION, &sd)) {
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

bool WsmApp::Init(bool needManager, bool needOpenService, DWORD desiredAccess)
{
    if (needManager && !_manager) {
        _manager = WsmSvc::Inst().GetManager();
        if (!_manager) {
            return false;
        }
    }

    if (needManager && needOpenService && !_service) {
        _service = OpenService(_manager, _name.data(), desiredAccess);
        if (!_service) {
            SPDLOG_ERROR("OpenService failed");
            return false;
        }
    }

    return true;
}

bool WsmApp::SetStartup(DWORD type)
{
    if (!Init(TRUE, TRUE, SERVICE_CHANGE_CONFIG))
        return false;

    if (!ChangeServiceConfig(
            _service,
            SERVICE_NO_CHANGE,
            type,
            SERVICE_NO_CHANGE,
            NULL, NULL, NULL, NULL, NULL, NULL, NULL)) {
        SPDLOG_ERROR("ChangeServiceConfig failed");
        return false;
    }

    SPDLOG_INFO("Service enabled successfully.");
    return true;
}

std::optional<WsmSvcStatus> WsmApp::GetStatus()
{
    DWORD bytesNeeded;
    SERVICE_STATUS_PROCESS statusProcess;
    if (!QueryServiceStatusEx(
            _service,
            SC_STATUS_PROCESS_INFO,
            (LPBYTE) &statusProcess,
            sizeof(SERVICE_STATUS_PROCESS),
            &bytesNeeded)) {
        SPDLOG_ERROR("QueryServiceStatusEx failed");
        return std::nullopt;
    }

    WsmSvcStatus status;
    status.init(_name, _alias, statusProcess);
    return status;
}

bool WsmApp::StopDependents()
{
    if (!Init(TRUE, TRUE))
        return false;

    DWORD startTime = GetTickCount();
    DWORD timeoutMS = 30000;
    auto deps = GetDependents();
    for (DWORD i = 0; i < deps.size(); i++) {
        auto& wss = deps[i];
        if (wss.currentState == SERVICE_STOPPED)
            continue;

        SC_HANDLE depService = OpenService(_manager, wss.serviceName.data(),
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
    return true;
}

std::vector<WsmSvcStatus> WsmApp::GetDependents()
{
    std::vector<WsmSvcStatus> result;

    if (!Init(TRUE, TRUE))
        goto svcdepend_cleanup;

    DWORD bytesNeeded;
    DWORD returnedCount;
    LPENUM_SERVICE_STATUS lpDeps = NULL;
    if (EnumDependentServices(_service, SERVICE_STATE_ALL, lpDeps, 0, &bytesNeeded, &returnedCount)) {
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

    if (!EnumDependentServices(_service, SERVICE_STATE_ALL, lpDeps, bytesNeeded, &bytesNeeded, &returnedCount)) {
        SPDLOG_ERROR("EnumDependentServices {} byte failed", bytesNeeded);
        goto svcdepend_cleanup;
    }

    for (DWORD i = 0; i < returnedCount; i++) {
        ENUM_SERVICE_STATUS ess = *(lpDeps + i);
        SERVICE_STATUS ss = ess.ServiceStatus;
        WsmSvcStatus svcStatus;
        svcStatus.init(ess.lpServiceName, ess.lpDisplayName, ss);
        result.push_back(svcStatus);
    }

svcdepend_cleanup:
    if (lpDeps)
        HeapFree(GetProcessHeap(), 0, lpDeps);
    return std::move(result);
}
