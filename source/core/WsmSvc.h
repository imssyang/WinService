#pragma once

#include "util/WsmUtil.h"

class WsmSvc
{
public:
    static WsmSvc& Inst() {
        static WsmSvc instance;
        return instance;
    }

    SC_HANDLE GetManager() {
        return _manager;
    }

    std::vector<WsmSvcStatus> GetServices();

private:
    WsmSvc();
    WsmSvc(const WsmSvc&) = delete;
    WsmSvc& operator=(const WsmSvc&) = delete;
    ~WsmSvc();

private:
    SC_HANDLE _manager;
};
