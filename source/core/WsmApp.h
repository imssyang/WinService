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

    std::string GetName() const { return _name; }
    virtual std::string GetPath() const { return _path; }

private:
    bool Init(bool needManager, bool needOpenService, DWORD desiredAccess = SERVICE_ALL_ACCESS);
    bool SetStartup(DWORD type);
    bool StopDependents();

private:
    std::string _name;
    std::string _alias;
    std::string _desc;
    std::string _path;
    SC_HANDLE manager_;
    SC_HANDLE service_;
};
