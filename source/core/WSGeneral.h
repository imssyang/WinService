#pragma once

#include "util/WSUtil.h"

struct WSHandle final
{
    WSHandle(
        DWORD mngDesiredAccess = 0x0,
        DWORD svcDesiredAccess = 0x0,
        const std::string& svcName = ""
    );
    ~WSHandle();

    bool Check() const;

    std::string Name;
    SC_HANDLE Manager;
    SC_HANDLE Service;
};

class WSGeneral final
{
public:
    static WSGeneral& Inst() {
        static WSGeneral instance;
        return instance;
    }

    std::vector<WSvcStatus> GetServices();

private:
    WSGeneral() {}
    WSGeneral(const WSGeneral&) = delete;
    WSGeneral& operator=(const WSGeneral&) = delete;
};
