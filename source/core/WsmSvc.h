#pragma once

#include "util/WsmUtil.h"

class WsmSvc
{
public:
    static WsmSvc& Inst() {
        static WsmSvc instance;
        return instance;
    }

    static SC_HANDLE Manager(DWORD desiredAccess = SC_MANAGER_ALL_ACCESS) {
        if (desiredAccess_ == desiredAccess) {
            
        }
        return Inst().manager_;
    }

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
