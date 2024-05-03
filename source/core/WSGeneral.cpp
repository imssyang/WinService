#include "core/WSGeneral.h"

#pragma comment(lib, "advapi32.lib")

WSHandle::WSHandle(
    DWORD mngDesiredAccess,
    DWORD svcDesiredAccess,
    const std::string& svcName)
    : Manager(NULL), Service(NULL)
{
    Manager = OpenSCManager(NULL, NULL, mngDesiredAccess);
    if (!Manager) {
        SPDLOG_ERROR("OpenSCManager failed! WinApi@");
        return;
    }

    if (!svcName.empty()) {
        Service = OpenService(Manager, svcName.data(), svcDesiredAccess);
        if (!Service) {
            SPDLOG_ERROR("OpenService({}, 0X{:X}) failed! WinApi@", svcName, svcDesiredAccess);
            return;
        }

        Name = svcName;
    }
}

WSHandle::~WSHandle()
{
    if (Service)
        CloseServiceHandle(Service);
    if (Manager)
        CloseServiceHandle(Manager);
}

bool WSHandle::Check() const
{
    if (!Manager)
        return false;
    if (!Name.empty()) {
        if (!Service) {
            return false;
        }
    }
    return true;
}

std::vector<WSvcStatus> WSGeneral::GetServices()
{
    std::vector<WSvcStatus> result;

    WSHandle wsHandle(SC_MANAGER_ENUMERATE_SERVICE);
    if (!wsHandle.Check())
        return result;

    DWORD bufSize = 0;
    DWORD numServices = 0;
    DWORD resumeHandle = 0;
    BOOL okStatus = EnumServicesStatusEx(wsHandle.Manager, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, NULL,
                                         bufSize, &bufSize, &numServices, &resumeHandle, NULL);
    if (okStatus) {
        SPDLOG_ERROR("No services for EnumServicesStatusEx!");
        return result;
    }

    if (GetLastError() != ERROR_MORE_DATA) {
        SPDLOG_ERROR("EnumServicesStatusEx failed! WinApi@");
        return result;
    }

    LPENUM_SERVICE_STATUS_PROCESS services = (LPENUM_SERVICE_STATUS_PROCESS)LocalAlloc(LPTR, bufSize);
    if (!services) {
        SPDLOG_ERROR("LocalAlloc {} byte failed! WinApi@", bufSize);
        return result;
    }

    okStatus = EnumServicesStatusEx(wsHandle.Manager, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL,
                                    (LPBYTE)services, bufSize, &bufSize, &numServices, &resumeHandle, NULL);
    if (!okStatus) {
        SPDLOG_ERROR("EnumServicesStatusEx failed! WinApi@");
        LocalFree(services);
        return result;
    }

    for (DWORD i = 0; i < numServices; i++) {
        ENUM_SERVICE_STATUS_PROCESS service = services[i];
        SERVICE_STATUS_PROCESS& ssp = service.ServiceStatusProcess;
        WSvcStatus svcStatus(service.lpServiceName, service.lpDisplayName, ssp);
        result.push_back(std::move(svcStatus));
    }

    LocalFree(services);
    return std::move(result);
}
