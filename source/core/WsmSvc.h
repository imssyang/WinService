#pragma once

#include "util/WsmUtil.h"

class WsmSvc final
{
public:
    static WsmSvc& Inst() {
        static WsmSvc instance;
        return instance;
    }

    SC_HANDLE GetManager(DWORD desiredAccess = SC_MANAGER_ALL_ACCESS);
    std::vector<WsmSvcStatus> GetServices();

private:
    WsmSvc();
    WsmSvc(const WsmSvc&) = delete;
    WsmSvc& operator=(const WsmSvc&) = delete;
    ~WsmSvc();

private:
    SC_HANDLE manager_;
    DWORD desiredAccess_;
};
