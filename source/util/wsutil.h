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
#include <filesystem>
#include <iostream>
#include <locale>
#include <map>
#include <mutex>
#include <optional>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "spdlog/spdlog.h"

#define SERVICE_WIN32_AS_SERVICE 0x55FFAAFF
#define SPDLOG_COUT(...) SPDLOG_LOGGER_INFO(spdlog::get("cout").get(), __VA_ARGS__)

struct WSvcBase
{
    std::string serviceName;
    std::string displayName;
    unsigned long serviceType;

    std::string GetType() const {
        return GetType(serviceType);
    }

    static unsigned long GetType(const std::string& svcType, bool isInteractive = false) {
	    DWORD dwInteractiveBit = isInteractive ? SERVICE_INTERACTIVE_PROCESS : 0;
        if (svcType == "Kernel")
            return SERVICE_KERNEL_DRIVER;
        else if (svcType == "FileSystem")
            return SERVICE_FILE_SYSTEM_DRIVER;
        else if (svcType == "Recognizer")
            return SERVICE_RECOGNIZER_DRIVER;
        else if (svcType == "Adapter")
            return SERVICE_ADAPTER;
        else if (svcType == "WinOwn")
            return SERVICE_WIN32_OWN_PROCESS | dwInteractiveBit;
        else if (svcType == "WinShare")
            return SERVICE_WIN32_SHARE_PROCESS | dwInteractiveBit;
        else if (svcType == "Win32")
            return SERVICE_WIN32;
        else if (svcType == "AsService")
            return SERVICE_WIN32_AS_SERVICE;
        else
            return 0;
    }

    static std::string GetType(unsigned long svcType) {
        switch (svcType) {
            case SERVICE_KERNEL_DRIVER:
                return "Kernel";
            case SERVICE_FILE_SYSTEM_DRIVER:
                return "FileSystem";
            case SERVICE_RECOGNIZER_DRIVER:
                return "Recognizer";
            case SERVICE_DRIVER:
                return "Driver";
            case SERVICE_ADAPTER:
                return "Adapter";
            case SERVICE_WIN32_AS_SERVICE:
                return "AsService";
            case SERVICE_WIN32_OWN_PROCESS:
                return "WinOwn";
            case SERVICE_WIN32_SHARE_PROCESS:
                return "WinShare";
            case SERVICE_WIN32:
                return "Win32";
            case SERVICE_USER_OWN_PROCESS:
                return "UserOwn";
            case SERVICE_USER_SHARE_PROCESS:
                return "UserShare";
            default:
                std::stringstream ss;
                ss << std::hex << svcType;
                if (svcType & SERVICE_INTERACTIVE_PROCESS)
                    return "Inter(" + ss.str() + ")";
                else
                    return "Extend(" + ss.str() + ")";
        }
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
        if (binaryPathName.find("winsvc") != std::string::npos
            || binaryPathName.find("srvman") != std::string::npos)
            serviceType = SERVICE_WIN32_AS_SERVICE;
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


void InitSpdlog(bool isGui, bool enableFile);
void WriteServiceLog(const std::string& svcName, const std::string& logContext);
std::string GetWorkDirectory();
std::string GetLogDirectory();
std::string UTF8toANSI(const std::string& utf8);
std::string ANSItoUTF8(const std::string& gbk);
void ForceKillProcess(DWORD processId);
void PrintStackContext(CONTEXT* ctx);
struct RtlContextException
{
    RtlContextException(){
        RtlCaptureContext(&Context);
        PrintStackContext(&Context);
    }

    CONTEXT Context;
};
