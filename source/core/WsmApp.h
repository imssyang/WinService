#pragma once

#include "util/WsmUtil.h"

class WsmApp
{
public:
    WsmApp(const std::string& name, const std::string& alias = "");
    virtual ~WsmApp();

    virtual bool Install(const std::string& path);
    bool Uninstall();
    bool Start();
    bool Stop();
    bool Enable();
    bool Disable();
    std::optional<WsmSvcStatus> GetStatus();
    std::optional<WsmSvcConfig> GetConfig(bool hasDesc = false);
    std::vector<WsmSvcStatus> GetDependents();
    bool SetDescription(const std::string& desc);
    bool SetDacl(const std::string& trustee);

    std::string GetName() const { return name_; }
    virtual std::string GetPath() const { return path_; }

private:
    bool Init(DWORD scDesiredAccess = SC_MANAGER_ALL_ACCESS, DWORD svDesiredAccess = SERVICE_ALL_ACCESS);
    bool SetStartup(DWORD type);
    bool StopDependents();

private:
    std::string name_;
    std::string alias_;
    std::string desc_;
    std::string path_;
};
