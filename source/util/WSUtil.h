#pragma once

#include <aclapi.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <windows.h>
#ifdef _DEBUG
#include <DbgHelp.h>
#endif
#include <cstdio>
#include <codecvt>
#include <iostream>
#include <locale>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include "spdlog/spdlog.h"

#define SERVICE_WIN32_APP_AS_SERVICE 0x55FFAAFF
#define SPDLOG_COUT(...) SPDLOG_LOGGER_INFO(spdlog::get("cout").get(), __VA_ARGS__)

struct WSvcBase
{
    std::string serviceName;
    std::string displayName;
    unsigned long serviceType;

    std::string GetServiceType() const {
        switch (serviceType) {
            case SERVICE_KERNEL_DRIVER:
                return "Driver";
            case SERVICE_FILE_SYSTEM_DRIVER:
                return "FSDriver";
            case SERVICE_WIN32_OWN_PROCESS:
            case SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS:
                return "OwnProcess";
            case SERVICE_WIN32_SHARE_PROCESS:
            case SERVICE_WIN32_SHARE_PROCESS | SERVICE_INTERACTIVE_PROCESS:
                return "ShareProcess";
            default:
                return "Application";
        }
    }

    void SetServiceType(const std::string& svcType, bool isInteractive) {
	    DWORD dwInteractiveBit = isInteractive ? SERVICE_INTERACTIVE_PROCESS : 0;
        if (svcType == "Driver")
            serviceType = SERVICE_KERNEL_DRIVER;
        else if (svcType == "FSDriver")
            serviceType = SERVICE_FILE_SYSTEM_DRIVER;
        else if (svcType == "OwnProcess")
            serviceType = SERVICE_WIN32_OWN_PROCESS | dwInteractiveBit;
        else if (svcType == "ShareProcess")
            serviceType = SERVICE_WIN32_SHARE_PROCESS | dwInteractiveBit;
        else if (svcType == "Application")
            serviceType = SERVICE_WIN32_APP_AS_SERVICE;
    }
};

struct WSvcStatus : public WSvcBase
{
    unsigned long currentState;
    unsigned long controlsAccepted;
    unsigned long win32ExitCode;
    unsigned long serviceSpecificExitCode;
    unsigned long checkPoint;
    unsigned long waitHint;
    unsigned long processId;
    unsigned long serviceFlags;

    WSvcStatus(const std::string& serviceName,
        const std::string& displayName,
        const SERVICE_STATUS& ss) {
        this->serviceName = serviceName;
        this->displayName = displayName;
        serviceType = ss.dwServiceType;
        currentState = ss.dwCurrentState;
        controlsAccepted = ss.dwControlsAccepted;
        win32ExitCode = ss.dwWin32ExitCode;
        serviceSpecificExitCode = ss.dwServiceSpecificExitCode;
        checkPoint = ss.dwCheckPoint;
        waitHint = ss.dwWaitHint;
        processId = 0;
        serviceFlags = 0;
    }

    WSvcStatus(const std::string& serviceName,
        const std::string& displayName,
        const SERVICE_STATUS_PROCESS& ssp) {
        this->serviceName = serviceName;
        this->displayName = displayName;
        serviceType = ssp.dwServiceType;
        currentState = ssp.dwCurrentState;
        controlsAccepted = ssp.dwControlsAccepted;
        win32ExitCode = ssp.dwWin32ExitCode;
        serviceSpecificExitCode = ssp.dwServiceSpecificExitCode;
        checkPoint = ssp.dwCheckPoint;
        waitHint = ssp.dwWaitHint;
        processId = ssp.dwProcessId;
        serviceFlags = ssp.dwServiceFlags;
    }

    std::string GetCurrentState() const {
        return GetState(currentState);
    }

    bool IsRunInSystemProcess() {
        return bool(serviceFlags & SERVICE_RUNS_IN_SYSTEM_PROCESS);
    }

    static std::string GetState(unsigned long state) {
        switch (state) {
            case SERVICE_CONTINUE_PENDING:
                return "ContinuePending";
            case SERVICE_PAUSE_PENDING:
                return "PausePending";
            case SERVICE_PAUSED:
                return "Paused";
            case SERVICE_RUNNING:
                return "Running";
            case SERVICE_START_PENDING:
                return "StartPending";
            case SERVICE_STOP_PENDING:
                return "StopPending";
            case SERVICE_STOPPED:
                return "Stopped";
            default:
                return "Unknown";
        }
    }
};

struct WSvcConfig : public WSvcBase
{
    std::string serviceStartName;
    std::string description;
    std::string binaryPathName;
    std::string loadOrderGroup;
    std::string dependencies;
    unsigned long startType;
    unsigned long errorControl;
    unsigned long tagId;

    WSvcConfig(const std::string& serviceName,
        const QUERY_SERVICE_CONFIG& qsc,
        const SERVICE_DESCRIPTION& sd) {
        this->serviceName = serviceName;
        serviceType = qsc.dwServiceType;
        startType = qsc.dwStartType;
        tagId = qsc.dwTagId;
        errorControl = qsc.dwErrorControl;
        if (qsc.lpDisplayName)
            displayName = qsc.lpDisplayName;
        if (qsc.lpServiceStartName)
            serviceStartName = qsc.lpServiceStartName;
        if (qsc.lpBinaryPathName)
            binaryPathName = qsc.lpBinaryPathName;
        if (qsc.lpDependencies)
            dependencies = qsc.lpDependencies;
        if (sd.lpDescription)
            description = sd.lpDescription;
        if (qsc.lpLoadOrderGroup)
            loadOrderGroup = qsc.lpLoadOrderGroup;
    }

    std::string GetStartType() const {
        return GetStartType(startType);
    }

    static std::string GetStartType(unsigned long startType) {
        switch (startType) {
            case SERVICE_BOOT_START:
                return "Boot";
            case SERVICE_SYSTEM_START:
                return "System";
            case SERVICE_AUTO_START:
                return "Automatic";
            case SERVICE_DEMAND_START:
                return "Manual";
            case SERVICE_DISABLED:
                return "Disabled";
            default:
                return "Unknown";
        }
    }

    static unsigned long GetStartType(const std::string& startType) {
        if (startType == "Boot")
            return SERVICE_BOOT_START;
        else if (startType == "System")
            return SERVICE_SYSTEM_START;
        else if (startType == "Automatic")
            return SERVICE_AUTO_START;
        else if (startType == "Manual")
            return SERVICE_DEMAND_START;
        else if (startType == "Disabled")
            return SERVICE_DISABLED;
        else
            return 0;
    }
};

std::string UTF8toANSI(const std::string& utf8);
std::string ANSItoUTF8(const std::string& gbk);

void InitSpdlog(bool isGui, bool enableFile);
void PrintStackContext(CONTEXT* ctx);
struct RtlContextException
{
    RtlContextException(){
        RtlCaptureContext(&Context);
        PrintStackContext(&Context);
    }

    CONTEXT Context;
};
