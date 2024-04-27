#include "core/WsmSvc.h"

#pragma comment(lib, "advapi32.lib")

WsmSvc::WsmSvc(): manager_(NULL)
{
    //manager_ = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    //if (!manager_) {
    //    SPDLOG_ERROR("OpenSCManager failed! WinApi@");
    //}
}

WsmSvc::~WsmSvc()
{
    if (manager_)
        CloseServiceHandle(manager_);
}

std::vector<WsmSvcStatus> WsmSvc::GetServices()
{
    std::vector<WsmSvcStatus> result;

    manager_ = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (!manager_) {
        SPDLOG_ERROR("OpenSCManager failed! WinApi@");
        goto svc_cleanup;
    }

    DWORD bufSize = 0;
    DWORD numServices = 0;
    DWORD resumeHandle = 0;
    BOOL okStatus = EnumServicesStatusEx(manager_, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, NULL,
                                         bufSize, &bufSize, &numServices, &resumeHandle, NULL);
    if (okStatus) {
        SPDLOG_ERROR("No services! EnumServicesStatusEx failed! WinApi@");
        goto svc_cleanup;
    }

    if (GetLastError() != ERROR_MORE_DATA) {
        SPDLOG_ERROR("EnumServicesStatusEx failed! WinApi@");
        goto svc_cleanup;
    }

    LPENUM_SERVICE_STATUS_PROCESS services = (LPENUM_SERVICE_STATUS_PROCESS)LocalAlloc(LPTR, bufSize);
    if (!services) {
        SPDLOG_ERROR("LocalAlloc {} byte failed! WinApi@", bufSize);
        goto svc_cleanup;
    }

    okStatus = EnumServicesStatusEx(manager_, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL,
                                    (LPBYTE)services, bufSize, &bufSize, &numServices, &resumeHandle, NULL);
    if (!okStatus) {
        SPDLOG_ERROR("EnumServicesStatusEx failed! WinApi@");
        goto svc_cleanup;
    }

    for (DWORD i = 0; i < numServices; i++) {
        ENUM_SERVICE_STATUS_PROCESS service = services[i];
        SERVICE_STATUS_PROCESS& ssp = service.ServiceStatusProcess;
        WsmSvcStatus svcStatus;
        svcStatus.init(service.lpServiceName, service.lpDisplayName, ssp);
        result.push_back(svcStatus);
    }

svc_cleanup:
    if (services)
        LocalFree(services);
    return std::move(result);
}
