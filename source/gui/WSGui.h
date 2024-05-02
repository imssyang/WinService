#pragma once

#include <d3d11.h>
#include "util/WSUtil.h"
#include "imgui/imgui.h"

class D3D11Device
{
public:
    D3D11Device(HWND hwnd);
    ~D3D11Device();

    void SetViewColor(float r, float g, float b, float alpha);
    void ResizeBuffer(int width, int height);
    void SwapBuffer(bool hasVsync);

private:
    void CreateView();
    void ReleaseView();

public:
    ID3D11Device* device;
    ID3D11DeviceContext* deviceContext;
    ID3D11RenderTargetView* targetView;
    IDXGISwapChain* swapChain;
};

class ImGuiEngine;

class ImGuiServiceItem
{
public:
    static void Init(const ImGuiTableSortSpecs* sortSpecs);
    static int Compare(const void* lhs, const void* rhs);

    ImGuiServiceItem(int id, WSvcStatus status, WSvcConfig config);
    int GetID() const { return id_; }
    const WSvcStatus& GetSvcStatus() const { return status_; }
    const WSvcConfig& GetSvcConfig() const { return config_; }
    std::string GetName() const { return status_.serviceName; }
    std::string GetAlias() const { return status_.displayName; }
    std::string GetType() const { return status_.GetType(); }
    std::string GetStartup() const { return config_.GetStartType(); }
    std::string GetState() const { return status_.GetCurrentState(); }
    uint32_t GetPID() const { return (uint32_t) status_.processId; }
    std::string GetPath() const { return config_.binaryPathName; }
    std::string GetDesc() const { return ANSItoUTF8(config_.description); }

private:
    int id_;
    WSvcStatus status_;
    WSvcConfig config_;
    static const ImGuiTableSortSpecs* sortSpecs_;
};

class ImGuiBaseWnd
{
public:
    static std::string ToMultiLine(const std::string& line, int charNum);
    static std::string ToOneLine(const std::string& multiline);
    static float CharWidth;
    static std::vector<std::string> TypeIDs;
    static std::vector<std::string> StartupIDs;
    static std::vector<std::string> StateIDs;

    ImGuiBaseWnd(ImGuiEngine* engine)
        : engine_(engine), wndFlags_(ImGuiWindowFlags_None) {}

    ImGuiEngine& GetEngine() const { return *engine_; }
    const ImVec2& GetPos() const { return wndPos_; }
    const ImVec2& GetSize() const { return wndSize_; }
    template<typename... Args> void HelpTip(Args... args);

protected:
    ImGuiEngine *engine_;
    ImVec2 wndPos_;
    ImVec2 wndSize_;
    ImGuiWindowFlags wndFlags_;
};

class ImGuiPropertyWnd : public ImGuiBaseWnd
{
public:
    ImGuiPropertyWnd(ImGuiEngine* engine, const std::string& title);
    std::string GetTitle() const { return title_; }

    void Reset();
    void Show(const ImGuiServiceItem* item = nullptr);

private:
    std::string title_;
    int typeID_;
    int startupID_;
    char svcID_[256];
    char svcType_[256];
    char svcName_[256];
    char svcAlias_[256];
    char svcStartup_[256];
    char svcState_[256];
    char svcPath_[1024];
    char svcDesc_[2048];
};

class ImGuiServiceWnd : public ImGuiBaseWnd
{
public:
    enum ColumnID {
        ColumnID_ID,
        ColumnID_Name,
        ColumnID_Alias,
        ColumnID_Type,
        ColumnID_Startup,
        ColumnID_State,
        ColumnID_PID,
        ColumnID_Path,
        ColumnID_Desc
    };

    ImGuiServiceWnd(ImGuiEngine* engine);

    const std::vector<std::string>& GetColumnIDs() const { return columnIDs_; }
    const std::vector<ImGuiServiceItem> GetItems() const { return items_; }
    const ImVector<int> GetSelections() const { return selections_; }
    void SyncItems();

    void Show();

private:
    int startupID_;
    int stateID_;
    std::vector<std::string> columnIDs_;
    std::vector<ImGuiServiceItem> items_;
    ImVector<int> selections_;
    ImGuiTableFlags servTableFlags_;
    ImGuiPropertyWnd propertyWnd_;
};

class ImGuiNavigationWnd : public ImGuiBaseWnd
{
public:
    enum ColumnID {
        ColumnID_Mode,
        ColumnID_Control,
        ColumnID_Filter
    };

    enum Mode { Mode_Self, Mode_All };

    ImGuiNavigationWnd(ImGuiEngine* engine);

    ImGuiTextFilter& GetFilter() { return filter_; }
    Mode GetMode() const { return static_cast<Mode>(mode_); }
    ImGuiServiceWnd::ColumnID GetColumnID() const {
        return static_cast<ImGuiServiceWnd::ColumnID>(columnID_);
    }

    void Show();

private:
    int mode_;
    int columnID_;
    ImGuiTextFilter filter_;
    ImGuiTableFlags mainTableFlags_;
    ImGuiComboFlags filterComboFlags_;
    ImGuiPropertyWnd propertyWnd_;
};

class ImGuiEngine
{
public:
    enum Font { Font_Default, Font_STZhongsong, Font_STXihei, Font_MSYaHei, Font_MSYaHeiLight, Font_MSYaheiBold };

    ImGuiEngine(HWND hwnd);
    ~ImGuiEngine();

    ImFont* GetFont(Font id) { return fonts_[id]; }
    ImGuiNavigationWnd& GetNavigationWnd() { return navWnd_; }
    ImGuiServiceWnd& GetServiceWnd() { return servWnd_; }

    void ResetMainWnd();
    void SetMainSize(int width, int height);
    void SetMainColor(float x, float y, float z, float w);
    void ShowMainWnd(bool hasVsync);
    void ShowWidgetWnd();

private:
    std::map<int, ImFont*> fonts_;
    D3D11Device dx11_;
    ImGuiNavigationWnd navWnd_;
    ImGuiServiceWnd servWnd_;
};

class GuiWindow
{
public:
    GuiWindow(const std::string& name, int x, int y, int width, int height);
    ~GuiWindow();

    void PollMessage();

private:
    static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void InitIcon(HWND hwnd);
    void UpdateSize(UINT width, UINT height);

private:
    HWND hwnd_;
    static std::unique_ptr<WNDCLASSEX> wcx_;
    static std::unique_ptr<ImGuiEngine> imgui_;
};

int GuiMain(int argc, char *argv[]);
