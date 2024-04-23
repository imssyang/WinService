#pragma once

#include <aclapi.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <windows.h>
#include <cstdio>
#include <codecvt>
#include <iostream>
#include <locale>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include "spdlog/spdlog.h"

#define SERVICE_WIN32_APP_AS_SERVICE 0x55FFAAFF
#define SPDLOG_COUT(...) SPDLOG_LOGGER_INFO(spdlog::get("cout").get(), __VA_ARGS__)

struct WsmSvcBase
{
    std::string serviceName;
    std::string displayName;
    unsigned long serviceType;

    std::string getServiceType() {
        switch (serviceType) {
            case SERVICE_KERNEL_DRIVER:
                return "KernelDriver";
            case SERVICE_FILE_SYSTEM_DRIVER:
                return "FileSystemDriver";
            case SERVICE_WIN32_OWN_PROCESS:
            case SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS:
                return "Win32OwnProcess";
            case SERVICE_WIN32_SHARE_PROCESS:
            case SERVICE_WIN32_SHARE_PROCESS | SERVICE_INTERACTIVE_PROCESS:
                return "Win32ShareProcess";
            default: return "Application";
        }
    }

    void setServiceType(const std::string& st, bool isInteractive) {
	    DWORD dwInteractiveBit = isInteractive ? SERVICE_INTERACTIVE_PROCESS : 0;
        if (st == "KernelDriver")
            serviceType = SERVICE_KERNEL_DRIVER;
        else if (st == "FileSystemDriver")
            serviceType = SERVICE_FILE_SYSTEM_DRIVER;
        else if (st == "Win32OwnProcess")
            serviceType = SERVICE_WIN32_OWN_PROCESS | dwInteractiveBit;
        else if (st == "Win32ShareProcess")
            serviceType = SERVICE_WIN32_SHARE_PROCESS | dwInteractiveBit;
        else if (st == "Application")
            serviceType = SERVICE_WIN32_APP_AS_SERVICE;
    }
};

struct WsmSvcStatus : public WsmSvcBase
{
    unsigned long currentState;
    unsigned long controlsAccepted;
    unsigned long win32ExitCode;
    unsigned long serviceSpecificExitCode;
    unsigned long checkPoint;
    unsigned long waitHint;
    unsigned long processId;
    unsigned long serviceFlags;

    void init(const std::string& serviceName,
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

    void init(const std::string& serviceName,
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

    std::string getCurrentState() {
        switch (currentState) {
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
            default: return "Unknown";
        }
    }

    bool isRunInSystemProcess() {
        return bool(serviceFlags & SERVICE_RUNS_IN_SYSTEM_PROCESS);
    }
};

struct WsmSvcConfig : public WsmSvcBase
{
    std::string serviceStartName;
    std::string description;
    std::string binaryPathName;
    std::string loadOrderGroup;
    std::string dependencies;
    unsigned long startType;
    unsigned long errorControl;
    unsigned long tagId;

    void init(const std::string& serviceName,
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

    std::string getStartType() {
        switch (startType) {
            case SERVICE_BOOT_START:
                return "BootStart";
            case SERVICE_SYSTEM_START:
                return "SystemStart";
            case SERVICE_AUTO_START:
                return "AutoStart";
            case SERVICE_DEMAND_START:
                return "DemandStart";
            case SERVICE_DISABLED:
                return "Disabled";
            default:
                return "Unknown";
        }
    }
};

void InitSpdlog(bool isGui, bool enableFile);
std::string UTF8toGBK(const std::string& utf8);
std::string GBKtoUTF8(const std::string& gbk);
