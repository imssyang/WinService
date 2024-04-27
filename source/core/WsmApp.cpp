#include "core/WsmApp.h"
#include "core/WsmSvc.h"

WsmApp::WsmApp(const std::string& name, const std::string& alias):
    name_(name), alias_(alias)
{
    if (alias_.empty())
        alias_ = name_;
}

WsmApp::~WsmApp()
{
}

bool WsmApp::Install(const std::string& path)
{
    bool result = false;
    SC_HANDLE manager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!manager) {
        SPDLOG_ERROR("OpenSCManager on install failed! WinApi@");
        goto svc_cleanup;
    }

    TCHAR executedPath[MAX_PATH];
    StringCbPrintf(executedPath, MAX_PATH, TEXT("%s"), path.data());

    SC_HANDLE service = CreateService(manager,
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
        goto svc_cleanup;
    }

    path_ = path;
    result = true;
    SPDLOG_INFO("Service installed successfully");

svc_cleanup:
    if (service)
        CloseServiceHandle(service);
    if (manager)
        CloseServiceHandle(manager);
    return result;
}

bool WsmApp::Uninstall()
{
    bool result = false;
    SC_HANDLE manager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!manager) {
        SPDLOG_ERROR("OpenSCManager on Uninstall failed! WinApi@");
        goto svc_cleanup;
    }

    SC_HANDLE service = OpenService(manager, name_.data(), DELETE);
    if (!service) {
        SPDLOG_ERROR("OpenService failed! WinApi@");
        goto svc_cleanup;
    }

    if (!DeleteService(service)) {
        SPDLOG_ERROR("DeleteService failed! WinApi@");
        goto svc_cleanup;
    }

    result = true;
    SPDLOG_INFO("Service deleted successfully");

svc_cleanup:
    if (service)
        CloseServiceHandle(service);
    if (manager)
        CloseServiceHandle(manager);
    return result;
}

bool WsmApp::SetDescription(const std::string& desc)
{
    bool result = false;
    if (desc.empty())
        goto svc_cleanup;

    SC_HANDLE manager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!manager) {
        SPDLOG_ERROR("OpenSCManager on Uninstall failed! WinApi@");
        goto svc_cleanup;
    }

    SC_HANDLE service = OpenService(manager, name_.data(), SERVICE_CHANGE_CONFIG);
    if (!service) {
        SPDLOG_ERROR("OpenService failed! WinApi@");
        goto svc_cleanup;
    }

    SERVICE_DESCRIPTION sd;
    sd.lpDescription = (LPTSTR)desc.data();
    if (!ChangeServiceConfig2(service,
            SERVICE_CONFIG_DESCRIPTION,
            &sd)) {
        SPDLOG_ERROR("ChangeServiceConfig2 failed! WinApi@");
        goto svc_cleanup;
    }

    desc_ = desc;
    result = true;
    SPDLOG_INFO("Service description updated successfully.");

svc_cleanup:
    if (service)
        CloseServiceHandle(service);
    if (manager)
        CloseServiceHandle(manager);
    return result;
}

std::optional<WsmSvcConfig> WsmApp::GetConfig(bool hasDesc)
{
    std::optional<WsmSvcConfig> result = std::nullopt;

    SC_HANDLE manager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (!manager) {
        SPDLOG_ERROR("OpenSCManager on Uninstall failed! WinApi@");
        goto svcconfig_cleanup;
    }

    SC_HANDLE service = OpenService(manager, name_.data(), SERVICE_QUERY_CONFIG);
    if (!service) {
        SPDLOG_ERROR("OpenService failed! WinApi@");
        goto svcconfig_cleanup;
    }

    LPQUERY_SERVICE_CONFIG lpsc = NULL;
    DWORD bytesNeeded, bufSize, lastError;
    if (!QueryServiceConfig(service, NULL, 0, &bytesNeeded)) {
        lastError = GetLastError();
        if (ERROR_INSUFFICIENT_BUFFER != lastError) {
            SPDLOG_ERROR("QueryServiceConfig failed ({})", lastError);
            goto svcconfig_cleanup;
        }

        bufSize = bytesNeeded;
        lpsc = (LPQUERY_SERVICE_CONFIG) LocalAlloc(LMEM_FIXED, bufSize);
    }

    if (!QueryServiceConfig(service, lpsc, bufSize, &bytesNeeded)) {
        SPDLOG_ERROR("QueryServiceConfig failed.");
        goto svcconfig_cleanup;
    }

    LPSERVICE_DESCRIPTION lpsd = NULL;
    if (hasDesc) {
        if (!QueryServiceConfig2(service, SERVICE_CONFIG_DESCRIPTION, NULL, 0, &bytesNeeded)) {
            if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
                bufSize = bytesNeeded;
                lpsd = (LPSERVICE_DESCRIPTION) LocalAlloc(LMEM_FIXED, bufSize);
                if (!QueryServiceConfig2(service, SERVICE_CONFIG_DESCRIPTION, (LPBYTE) lpsd, bufSize, &bytesNeeded)) {
                    SPDLOG_ERROR("QueryServiceConfig2 failed.");
                    goto svcconfig_cleanup;
                }
            }
        }
    }

    if (!lpsd)
        lpsd = (LPSERVICE_DESCRIPTION) LocalAlloc(LMEM_ZEROINIT, 0);

    result = std::make_optional<WsmSvcConfig>();
    result.value().init(name_, *lpsc, *lpsd);

svcconfig_cleanup:
    if (lpsc)
        LocalFree(lpsc);
    if (lpsd)
        LocalFree(lpsd);
    if (service)
        CloseServiceHandle(service);
    if (manager)
        CloseServiceHandle(manager);
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
    bool result = false;
    SC_HANDLE manager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (!manager) {
        SPDLOG_ERROR("OpenSCManager on start failed! WinApi@");
        goto svc_cleanup;
    }

    SC_HANDLE service = OpenService(manager, name_.data(), SERVICE_ALL_ACCESS);
    if (!service) {
        SPDLOG_ERROR("OpenService failed! WinApi@");
        goto svc_cleanup;
    }

    auto wssopt = GetStatus();
    if (!wssopt)
        goto svc_cleanup;

    auto& wss = wssopt.value();
    if (wss.currentState != SERVICE_STOPPED
        && wss.currentState != SERVICE_STOP_PENDING) {
        SPDLOG_ERROR("Cannot start the service because it is already running");
        goto svc_cleanup;
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
            goto svc_cleanup;

        wss = wssopt.value();
        if (wss.checkPoint > oldCheckPoint) {
            startTickCount = GetTickCount();
            oldCheckPoint = wss.checkPoint;
        } else {
            if (GetTickCount() - startTickCount > wss.waitHint) {
                SPDLOG_ERROR("Timeout waiting for service to stop");
                goto svc_cleanup;
            }
        }
    }

    if (!::StartService(service, 0, NULL)) {
        SPDLOG_ERROR("StartService failed.");
        goto svc_cleanup;
    }

    SPDLOG_INFO("Service start pending...");
    wssopt = GetStatus();
    if (!wssopt)
        goto svc_cleanup;

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
            goto svc_cleanup;

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
        goto svc_cleanup;
    }

    result = true;
    SPDLOG_INFO("Service started successfully.");

svc_cleanup:
    if (service)
        CloseServiceHandle(service);
    if (manager)
        CloseServiceHandle(manager);
    return result;
}

bool WsmApp::Stop()
{
    bool result = false;
    SC_HANDLE manager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (!manager) {
        SPDLOG_ERROR("OpenSCManager on start failed! WinApi@");
        goto svc_cleanup;
    }

    SC_HANDLE service = OpenService(manager, name_.data(), SERVICE_STOP |
        SERVICE_QUERY_STATUS |
        SERVICE_ENUMERATE_DEPENDENTS);
    if (!service) {
        SPDLOG_ERROR("OpenService failed! WinApi@");
        goto svc_cleanup;
    }

    auto wssopt = GetStatus();
    if (!wssopt)
        goto svc_cleanup;

    auto& wss = wssopt.value();
    if (wss.currentState == SERVICE_STOPPED) {
        SPDLOG_INFO("Service is already stopped.");
        goto svc_cleanup;
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
            goto svc_cleanup;

        wss = wssopt.value();
        if (wss.currentState == SERVICE_STOPPED) {
            SPDLOG_INFO("Service stopped successfully.");
            goto svc_cleanup;
        }

        if (GetTickCount() - startTime > timeoutMS) {
            SPDLOG_INFO("Service stop timed out.");
            goto svc_cleanup;
        }
    }

    // dependencies must be stopped first!
    StopDependents();

    SERVICE_STATUS_PROCESS ssp;
    if (!ControlService(service, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS) &ssp)) {
        DWORD lastError = GetLastError();
        if (ERROR_BROKEN_PIPE != lastError) {
            SPDLOG_ERROR("ControlService failed.");
            goto svc_cleanup;
        }
    }

    wss.currentState = ssp.dwCurrentState;
    wss.waitHint = ssp.dwWaitHint;
    while (wss.currentState != SERVICE_STOPPED) {
        Sleep(wss.waitHint);

        wssopt = GetStatus();
        if (!wssopt)
            goto svc_cleanup;

        wss = wssopt.value();
        if (wss.currentState == SERVICE_STOPPED)
            break;

        if (GetTickCount() - startTime > timeoutMS) {
            SPDLOG_INFO("Wait timed out");
            goto svc_cleanup;
        }
    }

    result = true;
    SPDLOG_INFO("Service stopped successfully");

svc_cleanup:
    if (service)
        CloseServiceHandle(service);
    if (manager)
        CloseServiceHandle(manager);
    return result;
}

bool WsmApp::SetDacl(const std::string& trustee)
{
    bool result = false;
    SC_HANDLE manager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!manager) {
        SPDLOG_ERROR("OpenSCManager on start failed! WinApi@");
        goto dacl_cleanup;
    }

    SC_HANDLE service = OpenService(manager, name_.data(), READ_CONTROL | WRITE_DAC);
    if (!service) {
        SPDLOG_ERROR("OpenService failed! WinApi@");
        goto dacl_cleanup;
    }

    DWORD descSize = 0;
    DWORD bytesNeeded = 0;
    PSECURITY_DESCRIPTOR psd = NULL;
    if (!QueryServiceObjectSecurity(service,
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

        if (!QueryServiceObjectSecurity(service,
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

    if (!SetServiceObjectSecurity(service, DACL_SECURITY_INFORMATION, &sd)) {
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
    if (service)
        CloseServiceHandle(service);
    if (manager)
        CloseServiceHandle(manager);
    return result;
}

bool WsmApp::SetStartup(DWORD type)
{
    bool result = false;
    SC_HANDLE manager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!manager) {
        SPDLOG_ERROR("OpenSCManager on start failed! WinApi@");
        goto svc_cleanup;
    }

    SC_HANDLE service = OpenService(manager, name_.data(), SERVICE_CHANGE_CONFIG);
    if (!service) {
        SPDLOG_ERROR("OpenService failed! WinApi@");
        goto svc_cleanup;
    }

    if (!ChangeServiceConfig(
            service,
            SERVICE_NO_CHANGE,
            type,
            SERVICE_NO_CHANGE,
            NULL, NULL, NULL, NULL, NULL, NULL, NULL)) {
        SPDLOG_ERROR("ChangeServiceConfig failed");
        goto svc_cleanup;
    }

    result = true;
    SPDLOG_INFO("Service enabled successfully.");

svc_cleanup:
    if (service)
        CloseServiceHandle(service);
    if (manager)
        CloseServiceHandle(manager);
    return result;
}

std::optional<WsmSvcStatus> WsmApp::GetStatus()
{
    std::optional<WsmSvcStatus> result = std::nullopt;

    SC_HANDLE manager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!manager) {
        SPDLOG_ERROR("OpenSCManager on start failed! WinApi@");
        goto svc_cleanup;
    }

    SC_HANDLE service = OpenService(manager, name_.data(), SERVICE_CHANGE_CONFIG);
    if (!service) {
        SPDLOG_ERROR("OpenService failed! WinApi@");
        goto svc_cleanup;
    }

    DWORD bytesNeeded;
    SERVICE_STATUS_PROCESS statusProcess;
    if (!QueryServiceStatusEx(
            service,
            SC_STATUS_PROCESS_INFO,
            (LPBYTE) &statusProcess,
            sizeof(SERVICE_STATUS_PROCESS),
            &bytesNeeded)) {
        SPDLOG_ERROR("QueryServiceStatusEx failed! WinApi@");
        goto svc_cleanup;
    }

    result = std::make_optional<WsmSvcStatus>();
    result.value().init(name_, alias_, statusProcess);

svc_cleanup:
    if (service)
        CloseServiceHandle(service);
    if (manager)
        CloseServiceHandle(manager);
    return std::move(result);
}

bool WsmApp::StopDependents()
{
    bool result = false;
    SC_HANDLE manager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!manager) {
        SPDLOG_ERROR("OpenSCManager on start failed! WinApi@");
        goto svc_cleanup;
    }

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

svc_cleanup:
    if (manager)
        CloseServiceHandle(manager);
    return result;
}

std::vector<WsmSvcStatus> WsmApp::GetDependents()
{
    std::vector<WsmSvcStatus> result;

    SC_HANDLE manager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (!manager) {
        SPDLOG_ERROR("OpenSCManager on start failed! WinApi@");
        goto svcdepend_cleanup;
    }

    SC_HANDLE service = OpenService(manager, name_.data(), SERVICE_ALL_ACCESS);
    if (!service) {
        SPDLOG_ERROR("OpenService failed! WinApi@");
        goto svcdepend_cleanup;
    }

    DWORD bytesNeeded;
    DWORD returnedCount;
    LPENUM_SERVICE_STATUS lpDeps = NULL;
    if (EnumDependentServices(service, SERVICE_STATE_ALL, lpDeps, 0, &bytesNeeded, &returnedCount)) {
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

    if (!EnumDependentServices(service, SERVICE_STATE_ALL, lpDeps, bytesNeeded, &bytesNeeded, &returnedCount)) {
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
    if (service)
        CloseServiceHandle(service);
    if (manager)
        CloseServiceHandle(manager);
    return std::move(result);
}
