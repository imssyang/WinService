#pragma once

#include "util/WSUtil.h"

class WSApp
{
public:
    WSApp(const std::string& name, const std::string& alias = "");
    virtual ~WSApp();

    virtual bool Install(const std::string& path);
    bool Uninstall();
    bool Start();
    bool Stop();
    bool Enable();
    bool Disable();
    std::optional<WSvcStatus> GetStatus();
    std::optional<WSvcConfig> GetConfig(bool hasDesc = false);
    std::vector<WSvcStatus> GetDependents();
    bool SetDescription(const std::string& desc);
    bool SetDacl(const std::string& trustee);

    std::string GetName() const { return name_; }
    virtual std::string GetPath() const { return path_; }

private:
    bool SetStartup(DWORD type);
    bool StopDependents(SC_HANDLE manager);

private:
    std::string name_;
    std::string alias_;
    std::string desc_;
    std::string path_;
};
