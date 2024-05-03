#include "gui/resource.h"
#include "gui/WSGui.h"
#include "core/WSGeneral.h"
#include "core/WSAgent.h"
#include "cmd/WSCmd.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx11.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

D3D11Device::D3D11Device(HWND hwnd)
    : device(nullptr), deviceContext(nullptr), targetView(nullptr), swapChain(nullptr)
{
    DXGI_SWAP_CHAIN_DESC scd;
    ZeroMemory(&scd, sizeof(scd));
    scd.BufferCount = 2;
    scd.BufferDesc.Width = 0;
    scd.BufferDesc.Height = 0;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT deviceFlags = 0; // D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevels[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, deviceFlags, featureLevels, 2, D3D11_SDK_VERSION, &scd, &swapChain, &device, &featureLevel, &deviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, deviceFlags, featureLevels, 2, D3D11_SDK_VERSION, &scd, &swapChain, &device, &featureLevel, &deviceContext);
    if (res != S_OK) {
        SPDLOG_ERROR("D3D11CreateDeviceAndSwapChain failed.");
        exit(0);
    }

    CreateView();
}

D3D11Device::~D3D11Device()
{
    ReleaseView();
    if (swapChain) {
        swapChain->Release();
        swapChain = nullptr;
    }
    if (deviceContext) {
        deviceContext->Release();
        deviceContext = nullptr;
    }
    if (device) {
        device->Release();
        device = nullptr;
    }
}

void D3D11Device::CreateView()
{
    ID3D11Texture2D* texture2d;
    swapChain->GetBuffer(0, IID_PPV_ARGS(&texture2d));
    device->CreateRenderTargetView(texture2d, nullptr, &targetView);
    texture2d->Release();
}

void D3D11Device::ReleaseView()
{
    if (targetView) {
        targetView->Release();
        targetView = nullptr;
    }
}

void D3D11Device::SetViewColor(float r, float g, float b, float alpha)
{
    const float color[4] = { r, g, b, alpha };
    deviceContext->OMSetRenderTargets(1, &targetView, nullptr);
    deviceContext->ClearRenderTargetView(targetView, color);
}

void D3D11Device::ResizeBuffer(int width, int height)
{
    ReleaseView();
    swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    CreateView();
}

void D3D11Device::SwapBuffer(bool hasVsync)
{
    if (hasVsync)
        swapChain->Present(1, 0);
    else
        swapChain->Present(0, 0);
}

const ImGuiTableSortSpecs* ImGuiServiceItem::sortSpecs_ = nullptr;

void ImGuiServiceItem::Init(const ImGuiTableSortSpecs* sortSpecs)
{
    sortSpecs_ = sortSpecs;
}

int ImGuiServiceItem::Compare(const void* lhs, const void* rhs)
{
    auto* a = (const ImGuiServiceItem*)lhs;
    auto* b = (const ImGuiServiceItem*)rhs;
    for (int n = 0; n < sortSpecs_->SpecsCount; n++) {
        const ImGuiTableColumnSortSpecs* columeSpec = &sortSpecs_->Specs[n];

        int delta = 0;
        switch (columeSpec->ColumnUserID) {
            case ImGuiServiceWnd::ColumnID_ID: delta = a->GetID() - b->GetID(); break;
            case ImGuiServiceWnd::ColumnID_Name: delta = a->GetName().compare(b->GetName()); break;
            case ImGuiServiceWnd::ColumnID_Alias: delta = a->GetAlias().compare(b->GetAlias()); break;
            case ImGuiServiceWnd::ColumnID_Type: delta = a->GetType().compare(b->GetType()); break;
            case ImGuiServiceWnd::ColumnID_Startup: delta = a->GetStartup().compare(b->GetStartup()); break;
            case ImGuiServiceWnd::ColumnID_State: delta = a->GetState().compare(b->GetState()); break;
            case ImGuiServiceWnd::ColumnID_PID: delta = a->GetPID() - b->GetPID(); break;
            case ImGuiServiceWnd::ColumnID_Path: delta = a->GetPath().compare(b->GetPath()); break;
            case ImGuiServiceWnd::ColumnID_Desc: delta = a->GetDesc().compare(b->GetDesc()); break;
            default: IM_ASSERT(0); break;
        }
        if (delta > 0)
            return (columeSpec->SortDirection == ImGuiSortDirection_Ascending) ? +1 : -1;
        if (delta < 0)
            return (columeSpec->SortDirection == ImGuiSortDirection_Ascending) ? -1 : +1;
    }
    return (a->GetID() - b->GetID());
}

ImGuiServiceItem::ImGuiServiceItem(int id, WSvcStatus status, WSvcConfig config)
    : id_(id), status_(std::move(status)), config_(std::move(config))
{
}

float ImGuiBaseWnd::CharWidth = 0;

std::vector<std::string> ImGuiBaseWnd::TypeIDs({
    WSvcBase::GetType(SERVICE_KERNEL_DRIVER),
    WSvcBase::GetType(SERVICE_FILE_SYSTEM_DRIVER),
    WSvcBase::GetType(SERVICE_WIN32),
    WSvcBase::GetType(SERVICE_WIN32_AS_SERVICE)
});

std::vector<std::string> ImGuiBaseWnd::StartupIDs({
    WSvcConfig::GetStartType(SERVICE_BOOT_START),
    WSvcConfig::GetStartType(SERVICE_SYSTEM_START),
    WSvcConfig::GetStartType(SERVICE_AUTO_START),
    WSvcConfig::GetStartType(SERVICE_DEMAND_START),
    WSvcConfig::GetStartType(SERVICE_DISABLED)
});

std::vector<std::string> ImGuiBaseWnd::StateIDs({
    WSvcStatus::GetState(SERVICE_CONTINUE_PENDING),
    WSvcStatus::GetState(SERVICE_PAUSE_PENDING),
    WSvcStatus::GetState(SERVICE_PAUSED),
    WSvcStatus::GetState(SERVICE_RUNNING),
    WSvcStatus::GetState(SERVICE_START_PENDING),
    WSvcStatus::GetState(SERVICE_STOP_PENDING),
    WSvcStatus::GetState(SERVICE_STOPPED)
});


std::string ImGuiBaseWnd::ToMultiLine(const std::string& line, int charNum)
{
    std::string multiline(line);
    int count = 0;
    for (int i = 0; i < multiline.size(); i++) {
        if (count >= charNum) {
            if (std::isspace(multiline[i])) {
                multiline[i] = '\n';
                count = 0;
            }
        }
        count++;
    }
    return std::move(multiline);
}

std::string ImGuiBaseWnd::ToOneLine(const std::string& multiline)
{
    std::string line(multiline);
    for (int i = 0; i < line.size(); i++) {
        if (line[i] == '\n') {
            line[i] = ' ';
        }
    }
    auto it = std::find_if_not(line.rbegin(), line.rend(), [](unsigned char ch) {
        return std::isspace(ch);
    });
    line.erase(it.base(), line.end());
    return std::move(line);
}

template<typename... Args>
void ImGuiBaseWnd::HelpTip(Args... args)
{
    std::stringstream ss;
    (ss << ... << args);

    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(ss.str().data());
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

ImGuiPropertyWnd::ImGuiPropertyWnd(
    ImGuiEngine* engine,
    const std::string& title
) : ImGuiBaseWnd(engine), title_(title)
{
    Reset();
}

void ImGuiPropertyWnd::Reset()
{
    typeID_ = TypeIDs.size()-1;
    startupID_ = StartupIDs.size()-2;
    sprintf_s(svcID_, IM_ARRAYSIZE(svcID_), "");
    sprintf_s(svcType_, IM_ARRAYSIZE(svcType_), "");
    sprintf_s(svcName_, IM_ARRAYSIZE(svcName_), "");
    sprintf_s(svcAlias_, IM_ARRAYSIZE(svcAlias_), "");
    sprintf_s(svcStartup_, IM_ARRAYSIZE(svcStartup_), "");
    sprintf_s(svcState_, IM_ARRAYSIZE(svcState_), "");
    sprintf_s(svcPath_, IM_ARRAYSIZE(svcPath_), "");
    sprintf_s(svcDesc_, IM_ARRAYSIZE(svcDesc_), "");
}

void ImGuiPropertyWnd::Show(const ImGuiServiceItem* item)
{
    std::string readFlag;
    ImGuiInputTextFlags itemFlags = ImGuiInputTextFlags_None;
    ImGui::SetNextWindowSize(ImVec2(CharWidth * 100, ImGui::GetTextLineHeight() * 20), ImGuiCond_Once);
    if (ImGui::BeginPopupModal(title_.data(), NULL, ImGuiWindowFlags_None)) {
        if (item) {
            readFlag = "*";
            itemFlags = ImGuiInputTextFlags_ReadOnly;
            if (0 == strcmp(svcName_, "") || 0 != strcmp(svcName_, item->GetName().data())) {
                auto itStartup = std::find(StartupIDs.begin(), StartupIDs.end(), item->GetStartup());
                if (itStartup != StartupIDs.end()) {
                    startupID_ = std::distance(StartupIDs.begin(), itStartup);
                }

                sprintf_s(svcID_, IM_ARRAYSIZE(svcID_), "%d", item->GetID());
                sprintf_s(svcType_, IM_ARRAYSIZE(svcType_), "%s", item->GetType().data());
                sprintf_s(svcName_, IM_ARRAYSIZE(svcName_), "%s", item->GetName().data());
                sprintf_s(svcAlias_, IM_ARRAYSIZE(svcAlias_), "%s", item->GetAlias().data());
                sprintf_s(svcStartup_, IM_ARRAYSIZE(svcStartup_), "%s", item->GetStartup().data());
                sprintf_s(svcState_, IM_ARRAYSIZE(svcState_), "%s (%d)", item->GetState().data(), item->GetPID());
                if (item->GetSvcConfig().serviceType == SERVICE_WIN32_AS_SERVICE) {
                    sprintf_s(svcPath_, IM_ARRAYSIZE(svcPath_), "%s", WSAgent::GetPath(item->GetPath()).data());
                } else {
                    sprintf_s(svcPath_, IM_ARRAYSIZE(svcPath_), "%s", item->GetPath().data());
                }
                sprintf_s(svcDesc_, IM_ARRAYSIZE(svcDesc_), "%s", item->GetDesc().data());
                SPDLOG_INFO("Item {}|{}|{}|{}|{}",
                    TypeIDs[typeID_], StartupIDs[startupID_], svcName_, svcAlias_, svcPath_);
            }
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.6f);
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 100);
        if (item) {
            ImGui::InputTextWithHint("ID*", "service's id", svcID_, IM_ARRAYSIZE(svcID_), ImGuiInputTextFlags_ReadOnly);
        }
        ImGui::InputTextWithHint(std::string("Name" + readFlag).data(), "service's name", svcName_, IM_ARRAYSIZE(svcName_), itemFlags);
        ImGui::InputTextWithHint(std::string("Alias" + readFlag).data(), "service's alias", svcAlias_, IM_ARRAYSIZE(svcAlias_), itemFlags);
        if (item) {
            ImGui::InputTextWithHint("Type*", "service's type", svcType_, IM_ARRAYSIZE(svcType_), itemFlags);
        } else {
            if (ImGui::BeginCombo("Type", TypeIDs[typeID_].data(), ImGuiComboFlags_None)) {
                for (int i = 0; i < TypeIDs.size(); i++) {
                    if (TypeIDs[i] == WSvcBase::GetType(SERVICE_KERNEL_DRIVER)
                        || TypeIDs[i] == WSvcBase::GetType(SERVICE_FILE_SYSTEM_DRIVER)) {
                        continue;
                    }
                    bool isSelected = (typeID_ == i);
                    if (ImGui::Selectable(TypeIDs[i].data(), isSelected)) {
                        if (typeID_ != i) {
                            SPDLOG_INFO("Service type: {} -> {}", TypeIDs[typeID_], TypeIDs[i]);
                            typeID_ = i;
                        }
                    }
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
        if (item) {
            ImGui::InputTextWithHint("Startup*", "service's startup", svcStartup_, IM_ARRAYSIZE(svcStartup_), itemFlags);
            ImGui::InputTextWithHint("State*", "service's state", svcState_, IM_ARRAYSIZE(svcState_), itemFlags);
        } else {
            if (ImGui::BeginCombo("Startup", StartupIDs[startupID_].data(), ImGuiComboFlags_None)) {
                for (int i = 0; i < StartupIDs.size(); i++) {
                    if (StartupIDs[i] == WSvcConfig::GetStartType(SERVICE_BOOT_START)
                        || StartupIDs[i] == WSvcConfig::GetStartType(SERVICE_SYSTEM_START)) {
                        continue;
                    }
                    bool isSelected = (startupID_ == i);
                    if (ImGui::Selectable(StartupIDs[i].data(), isSelected)) {
                        if (startupID_ != i) {
                            SPDLOG_INFO("Service startup: {} -> {}", StartupIDs[startupID_], StartupIDs[i]);
                            startupID_ = i;
                        }
                    }
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
        ImGui::InputTextWithHint(std::string("Execute" + readFlag).data(), "service's path", svcPath_, IM_ARRAYSIZE(svcPath_), itemFlags);
        if (ImGui::InputTextMultiline("Description", svcDesc_, IM_ARRAYSIZE(svcDesc_),
            ImVec2(0, ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeight() * 2.2),
            ImGuiInputTextFlags_None)) {
            int charNum = (ImGui::GetContentRegionAvail().x - 200) / CharWidth;
            std::string oneLine = ToOneLine(svcDesc_);
            std::string multiDesc = ToMultiLine(oneLine, charNum);
            sprintf_s(svcDesc_, IM_ARRAYSIZE(svcDesc_), "%s", multiDesc.data());
        }
        if (ImGui::IsItemClicked()) {
            std::string freshDesc(svcDesc_);
            freshDesc += " ";
            sprintf_s(svcDesc_, IM_ARRAYSIZE(svcDesc_), "%s", freshDesc.data());
        }
        ImGui::PopItemWidth();
        ImGui::PopStyleVar();
        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(120, 0))) {
            SPDLOG_INFO("Ok @ {}|{}|{}|{}|{}",
                TypeIDs[typeID_], StartupIDs[startupID_], svcName_, svcAlias_, svcPath_);
            if (TypeIDs[typeID_] == WSvcBase::GetType(SERVICE_WIN32_AS_SERVICE)) {
                WSAgent app(svcName_, svcAlias_);
                if (!item) {
                    app.Install(svcPath_);
                }
                app.SetDescription(svcDesc_);
            } else if (TypeIDs[typeID_] == WSvcBase::GetType(SERVICE_WIN32)) {
                WSApp app(svcName_, svcAlias_);
                if (!item) {
                    app.Install(svcPath_);
                }
                app.SetDescription(svcDesc_);
            } else {
                SPDLOG_ERROR("{} unsupport type: 0X{:X}", svcName_, typeID_);
            }
            Reset();
            GetEngine().GetServiceWnd().SyncItems();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            SPDLOG_INFO("Cancel @ {}|{}|{}|{}|{}",
                TypeIDs[typeID_], StartupIDs[startupID_], svcName_, svcAlias_, svcPath_);
            Reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

ImGuiServiceWnd::ImGuiServiceWnd(ImGuiEngine* engine)
    : ImGuiBaseWnd(engine), startupID_(-1), stateID_(-1)
    , propertyWnd_(engine_, "Edit Service Properties")
{
    wndFlags_ = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoCollapse;

    servTableFlags_ = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable
        | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_SortTristate
        | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_RowBg | ImGuiTableFlags_PadOuterX
        | ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_SizingFixedFit
        | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY;

    columnIDs_.swap(std::vector<std::string>(
        {"ID", "Name", "Alias", "Type", "Startup", "State", "PID", "Path", "Desc"}
    ));

    SyncItems();
}

void ImGuiServiceWnd::SyncItems()
{
    items_.clear();
    auto& navWnd = GetEngine().GetNavigationWnd();
    auto& filter = navWnd.GetFilter();
    auto mode = navWnd.GetMode();
    auto columnID = navWnd.GetColumnID();
    std::vector<WSvcStatus> svcStatuses = WSGeneral::Inst().GetServices();
    for (int i = 0; i < svcStatuses.size(); i++) {
        auto& status = svcStatuses[i];
        WSApp app(status.serviceName);
        auto wscOpt = app.GetConfig(true);
        if (!wscOpt) {
            SPDLOG_WARN("{} get config failed!", status.serviceName);
            continue;
        }

        auto& config = wscOpt.value();
        ImGuiServiceItem item(items_.size() + 1, status, config);
        if (filter.IsActive()) {
            std::string filterText;
            switch (columnID) {
            case ColumnID_ID:
                filterText = std::to_string(item.GetID());
                break;
            case ColumnID_Name:
                filterText = item.GetName();
                break;
            case ColumnID_Alias:
                filterText = item.GetAlias();
                break;
            case ColumnID_Type:
                filterText = item.GetType();
                break;
            case ColumnID_Startup:
                filterText = item.GetStartup();
                break;
            case ColumnID_State:
                filterText = item.GetState();
                break;
            case ColumnID_PID:
                filterText = std::to_string(item.GetPID());
                break;
            case ColumnID_Path:
                filterText = item.GetPath();
                break;
            case ColumnID_Desc:
                filterText = item.GetDesc();
                break;
            default:
                SPDLOG_ERROR("columnID:{} is invalid!", columnID);
                break;
            }

            if (!filter.PassFilter(filterText.data())) {
                continue;
            }
        }

        if (mode == ImGuiNavigationWnd::Mode_Self) {
            if (config.binaryPathName.find("winsvc") != std::string::npos
                || config.binaryPathName.find("srvman") != std::string::npos) {
                if (config.binaryPathName.find("RunAsService") != std::string::npos) {
                    items_.push_back(std::move(item));
                }
            }
        } else if (mode == ImGuiNavigationWnd::Mode_All) {
            items_.push_back(std::move(item));
        }
    }
    SPDLOG_INFO("SyncItems: {}", items_.size());
}

void ImGuiServiceWnd::Show()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    auto& navWnd = engine_->GetNavigationWnd();
    wndPos_.x = viewport->WorkPos.x;
    wndPos_.y = navWnd.GetPos().y + navWnd.GetSize().y;
    wndSize_.x = viewport->WorkSize.x;
    wndSize_.y = viewport->WorkSize.y - wndPos_.y;

    ImGui::SetNextWindowPos(wndPos_, ImGuiCond_Always);
    ImGui::SetNextWindowSize(wndSize_, ImGuiCond_Always);
    ImGui::Begin("TableWindow", nullptr, wndFlags_);

    if (ImGui::BeginTable("table_services", 9, servTableFlags_)) {
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 66.0f, ImGuiServiceWnd::ColumnID_ID);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 100.0f, ImGuiServiceWnd::ColumnID_Name);
        ImGui::TableSetupColumn("Alias", ImGuiTableColumnFlags_WidthFixed, 150.0f, ImGuiServiceWnd::ColumnID_Alias);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 70.0f, ImGuiServiceWnd::ColumnID_Type);
        ImGui::TableSetupColumn("Startup", ImGuiTableColumnFlags_WidthFixed, 0.0f, ImGuiServiceWnd::ColumnID_Startup);
        ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 0.0f, ImGuiServiceWnd::ColumnID_State);
        ImGui::TableSetupColumn("PID",  ImGuiTableColumnFlags_WidthFixed, 60.0f, ImGuiServiceWnd::ColumnID_PID);
        ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthFixed, 1600.0f, ImGuiServiceWnd::ColumnID_Path);
        ImGui::TableSetupColumn("Desc", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultHide, 0.0f, ImGuiServiceWnd::ColumnID_Desc);
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableHeadersRow();

        ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs();
        if (sortSpecs) {
            if (sortSpecs->SpecsDirty) {
                if (items_.size() > 1) {
                    ImGuiServiceItem::Init(sortSpecs);
                    qsort(&items_[0], (size_t)items_.size(), sizeof(items_[0]), ImGuiServiceItem::Compare);
                    sortSpecs->SpecsDirty = false;
                    SPDLOG_INFO("qsort: {}", items_.size());
                }
            }
        }

        ImGui::PushButtonRepeat(true);

        ImGuiListClipper clipper;
        clipper.Begin(items_.size());
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                ImGuiServiceItem& item = items_[row];
                ImGui::TableNextRow();
                ImGui::PushID(item.GetID());

                if (ImGui::TableSetColumnIndex(ImGuiServiceWnd::ColumnID_ID)) {
                    if (ImGui::Button("Edit"))
                        ImGui::OpenPopup(propertyWnd_.GetTitle().data());

                    propertyWnd_.Show(&item);
                    ImGui::SameLine();

                    bool isSelected = selections_.contains(item.GetID());
                    ImGuiSelectableFlags selectFlags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;
                    if (ImGui::Selectable(std::to_string(item.GetID()).data(), isSelected, selectFlags)) {
                        if (ImGui::GetIO().KeyCtrl) {
                            if (isSelected) {
                                selections_.find_erase_unsorted(item.GetID());
                            } else {
                                selections_.push_back(item.GetID());
                            }
                        } else {
                            selections_.clear();
                            selections_.push_back(item.GetID());
                        }
                    }
                }

                if (ImGui::TableSetColumnIndex(ImGuiServiceWnd::ColumnID_Name))
                    ImGui::TextUnformatted(item.GetName().data());

                if (ImGui::TableSetColumnIndex(ImGuiServiceWnd::ColumnID_Alias)) {
                    ImGui::TextUnformatted(item.GetAlias().data());
                }

                if (ImGui::TableSetColumnIndex(ImGuiServiceWnd::ColumnID_Type)) {
                    ImGui::TextUnformatted(item.GetType().data());
                }

                if (ImGui::TableSetColumnIndex(ImGuiServiceWnd::ColumnID_Startup)) {
                    auto it = std::find(StartupIDs.begin(), StartupIDs.end(), item.GetStartup());
                    startupID_ = std::distance(StartupIDs.begin(), it);
                    ImGui::SetNextItemWidth(CharWidth * 15);
                    if (ImGui::BeginCombo("##Startup", StartupIDs[startupID_].data(), ImGuiComboFlags_None)) {
                        for (int i = 0; i < StartupIDs.size(); i++) {
                            if (StartupIDs[i] == WSvcConfig::GetStartType(SERVICE_BOOT_START)
                                || StartupIDs[i] == WSvcConfig::GetStartType(SERVICE_SYSTEM_START)) {
                                continue;
                            }
                            bool isSelected = (startupID_ == i);
                            if (ImGui::Selectable(StartupIDs[i].data(), isSelected)) {
                                if (startupID_ != i) {
                                    SPDLOG_INFO("{} startup: {} -> {}", item.GetName(), StartupIDs[startupID_], StartupIDs[i]);
                                    WSApp app(item.GetName());
                                    app.SetStartup(WSvcConfig::GetStartType(StartupIDs[i]));
                                    SyncItems();
                                    startupID_ = i;
                                }
                            }
                            if (isSelected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
                if (ImGui::TableSetColumnIndex(ImGuiServiceWnd::ColumnID_State)) {
                    auto it = std::find(StateIDs.begin(), StateIDs.end(), item.GetState());
                    stateID_ = std::distance(StateIDs.begin(), it);
                    ImGui::SetNextItemWidth(CharWidth * 15);
                    if (ImGui::BeginCombo("##State", StateIDs[stateID_].data(), ImGuiComboFlags_None)) {
                        for (int i = 0; i < StateIDs.size(); i++) {
                            if (StateIDs[i] == WSvcStatus::GetState(SERVICE_CONTINUE_PENDING)
                                || StateIDs[i] == WSvcStatus::GetState(SERVICE_PAUSE_PENDING)
                                || StateIDs[i] == WSvcStatus::GetState(SERVICE_START_PENDING)
                                || StateIDs[i] == WSvcStatus::GetState(SERVICE_STOP_PENDING)
                                || StateIDs[i] == WSvcStatus::GetState(SERVICE_PAUSED)) {
                                continue;
                            }
                            bool isSelected = (stateID_ == i);
                            if (ImGui::Selectable(StateIDs[i].data(), isSelected)) {
                                if (stateID_ != i) {
                                    SPDLOG_INFO("{} state: {} -> {}", item.GetName(), StateIDs[stateID_], StateIDs[i]);
                                    WSApp app(item.GetName());
                                    if (StateIDs[i] == WSvcStatus::GetState(SERVICE_RUNNING))
                                        app.Start();
                                    else if (StateIDs[i] == WSvcStatus::GetState(SERVICE_STOPPED))
                                        app.Stop(3000);
                                    SyncItems();
                                    stateID_ = i;
                                }
                            }
                            if (isSelected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
                if (ImGui::TableSetColumnIndex(ImGuiServiceWnd::ColumnID_PID)) {
                    ImGui::Text("%d", item.GetPID());
                }
                if (ImGui::TableSetColumnIndex(ImGuiServiceWnd::ColumnID_Path)) {
                    char* inputBuf = strdup(item.GetPath().data());
                    if (inputBuf) {
                        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                        ImGui::InputText("##Path", inputBuf, item.GetPath().length() + 1, ImGuiInputTextFlags_ReadOnly);
                        ImGui::PopItemWidth();
                        free(inputBuf);
                    }
                }
                if (ImGui::TableSetColumnIndex(ImGuiServiceWnd::ColumnID_Desc)) {
                    ImGui::TextUnformatted(item.GetDesc().data());
                }

                ImGui::PopID();
            }
        }
        ImGui::PopButtonRepeat();

        ImGui::EndTable();
    }

    ImGui::End();
}

ImGuiNavigationWnd::ImGuiNavigationWnd(ImGuiEngine* engine)
    : ImGuiBaseWnd(engine), mode_(Mode_Self)
    , columnID_(ImGuiServiceWnd::ColumnID_Path)
    , propertyWnd_(engine_, "New Service Properties")
{
    wndFlags_ = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    mainTableFlags_ = ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_PadOuterX
        | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit;

    filterComboFlags_ = ImGuiComboFlags_None;
}

void ImGuiNavigationWnd::Show()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    wndPos_.x = viewport->WorkPos.x;
    wndPos_.y = viewport->WorkPos.y;
    wndSize_.x = viewport->WorkSize.x;

    ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);
    ImGui::SetNextWindowPos(wndPos_, ImGuiCond_Always);
    ImGui::SetNextWindowSize(wndSize_, ImGuiCond_Always);
    ImGui::Begin("Tool", nullptr, wndFlags_);

    const float frameHeight = ImGui::GetFrameHeight();
    if (ImGui::IsWindowCollapsed())
        wndSize_.y = frameHeight * 1;
    else
        wndSize_.y = frameHeight * 3;

    if (ImGui::BeginTable("table_tool", 3, mainTableFlags_)) {
        ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_NoHide, 0.0f, ImGuiNavigationWnd::ColumnID_Mode);
        ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_NoHide, 0.0f, ImGuiNavigationWnd::ColumnID_Control);
        ImGui::TableSetupColumn("Search", ImGuiTableColumnFlags_WidthStretch, 0.0f, ImGuiNavigationWnd::ColumnID_Filter);
        ImGui::TableNextRow();

        if (ImGui::TableSetColumnIndex(ImGuiNavigationWnd::ColumnID_Mode)) {
            ImVec4 windowBgColor = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
            ImGui::PushStyleColor(ImGuiCol_Button, windowBgColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, windowBgColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, windowBgColor);
            ImGui::Button("Mode:");
            ImGui::PopStyleColor(3);
            ImGui::SameLine();
            ImGui::RadioButton("agent", &mode_, Mode_Self);
            if (ImGui::IsItemClicked()) {
                if (mode_ != Mode_Self) {
                    mode_ = Mode_Self;
                    GetEngine().GetServiceWnd().SyncItems();
                }
            }
            ImGui::SameLine();
            ImGui::RadioButton("all", &mode_, Mode_All);
            if (ImGui::IsItemClicked()) {
                if (mode_ != Mode_All) {
                    mode_ = Mode_All;
                    GetEngine().GetServiceWnd().SyncItems();
                }
            }
        }

        if (ImGui::TableSetColumnIndex(ImGuiNavigationWnd::ColumnID_Control)) {
            ImGui::Text("Control:");
            ImGui::SameLine();

            if (ImGui::Button("Add", ImVec2(60, 0)))
                ImGui::OpenPopup(propertyWnd_.GetTitle().data());

            propertyWnd_.Show();
            ImGui::SameLine();

            if (ImGui::Button("Delete", ImVec2(60, 0)))
                ImGui::OpenPopup("Delete?");

            ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            if (ImGui::BeginPopupModal("Delete?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                auto& serviceWnd = GetEngine().GetServiceWnd();
                auto& selections = serviceWnd.GetSelections();
                if (selections.Size != 1) {
                    SPDLOG_WARN("Incorrect mumber of selections: {}", selections.Size);
                    ImGui::CloseCurrentPopup();
                } else {
                    auto& items = serviceWnd.GetItems();
                    int selectID = selections[0];
                    auto selectIt = std::find_if(items.begin(), items.end(), [&selectID](const ImGuiServiceItem& item) {
                        return item.GetID() == selectID;
                    });
                    if (selectIt == items.end()) {
                        SPDLOG_ERROR("selectID:{} not exist!", selectID);
                        ImGui::CloseCurrentPopup();
                    } else {
                        auto& selectItem = *selectIt;
                        auto& selectName = selectItem.GetName() + ", " + selectItem.GetAlias();
                        ImGui::Text("\"%s\" service will be deleted.\nThis operation cannot be undone!", selectName.data());
                        ImGui::Separator();

                        if (ImGui::Button("OK", ImVec2(120, 0))) {
                            SPDLOG_INFO("Delete @ {}", selectName);
                            WSApp app(selectItem.GetName());
                            app.Uninstall();
                            GetEngine().GetServiceWnd().SyncItems();
                            ImGui::CloseCurrentPopup();
                        }

                        ImGui::SetItemDefaultFocus();
                        ImGui::SameLine();

                        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                            SPDLOG_INFO("Cancel @ {}", selectName);
                            ImGui::CloseCurrentPopup();
                        }
                    }
                }

                ImGui::EndPopup();
            }
        }

        if (ImGui::TableSetColumnIndex(ImGuiNavigationWnd::ColumnID_Filter)) {
            ImGui::Text("Filter:");
            ImGui::SameLine();

            filter_.Draw("##Filter:", wndSize_.x - CharWidth * 89);
            if (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                GetEngine().GetServiceWnd().SyncItems();
            }
            ImGui::SameLine();

            ImGui::Text("@");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(CharWidth * 10);

            auto& items = engine_->GetServiceWnd().GetColumnIDs();
            if (ImGui::BeginCombo("##Item", items[columnID_].data(), filterComboFlags_)) {
                for (int i = 0; i < items.size(); i++) {
                    bool isSelected = (columnID_ == i);
                    if (ImGui::Selectable(items[i].data(), isSelected)) {
                        columnID_ = i;
                        GetEngine().GetServiceWnd().SyncItems();
                    }

                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

ImGuiEngine::ImGuiEngine(HWND hwnd)
    : servWnd_(this), navWnd_(this), dx11_(hwnd)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = NULL;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

#ifdef IMGUI_ENABLE_FREETYPE
    auto* fontAtlas = io.Fonts;
    auto* glyphRanges = fontAtlas->GetGlyphRangesChineseSimplifiedCommon();
    fonts_[Font_Default] = fontAtlas->AddFontDefault();
    fonts_[Font_STZhongsong] = fontAtlas->AddFontFromFileTTF("source/gui/res/font/STZhongsong.ttf", 18.0f, nullptr, glyphRanges);
    fonts_[Font_STXihei] = fontAtlas->AddFontFromFileTTF("source/gui/res/font/STXihei.ttf", 18.0f, nullptr, glyphRanges);
    fonts_[Font_MSYaHei] = fontAtlas->AddFontFromFileTTF("source/gui/res/font/MSYaHei.ttc", 18.0f, nullptr, glyphRanges);
    fonts_[Font_MSYaHeiLight] = fontAtlas->AddFontFromFileTTF("source/gui/res/font/MSYaHeiLight.ttc", 18.0f, nullptr, glyphRanges);
    fonts_[Font_MSYaheiBold] = fontAtlas->AddFontFromFileTTF("source/gui/res/font/MSYaheiBold.ttc", 18.0f, nullptr, glyphRanges);
    io.FontDefault = fonts_[Font_Default];
#endif

    ImGui::StyleColorsLight();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(dx11_.device, dx11_.deviceContext);
}

ImGuiEngine::~ImGuiEngine()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiEngine::ResetMainWnd()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiEngine::SetMainSize(int width, int height)
{
    dx11_.ResizeBuffer(width, height);

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    viewport->Size.x = static_cast<float>(width);
    viewport->Size.y = static_cast<float>(height);
}

void ImGuiEngine::SetMainColor(float x, float y, float z, float w)
{
    dx11_.SetViewColor(x*w, y*w, z*w, w);
}

void ImGuiEngine::ShowMainWnd(bool hasVsync)
{
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    dx11_.SwapBuffer(hasVsync);
}

void ImGuiEngine::ShowWidgetWnd()
{
    if (!ImGuiBaseWnd::CharWidth)
        ImGuiBaseWnd::CharWidth = ImGui::CalcTextSize("A").x;

    navWnd_.Show();
    servWnd_.Show();
}

std::unique_ptr<WNDCLASSEX> GuiWindow::wcx_;
std::unique_ptr<ImGuiEngine> GuiWindow::imgui_;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI GuiWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    GuiWindow* this_ = (GuiWindow*)::GetWindowLongPtr(hWnd, GWLP_USERDATA);
    switch (msg) {
        case WM_SIZE:
            if (wParam == SIZE_MINIMIZED)
                return 0;
            this_->UpdateSize((UINT)LOWORD(lParam), (UINT)HIWORD(lParam));
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU)
                return 0;
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

GuiWindow::GuiWindow(const std::string& name, int x, int y, int width, int height)
{
    wcx_ = std::make_unique<WNDCLASSEX>();
    wcx_->cbSize = sizeof(WNDCLASSEX);
    wcx_->style = CS_CLASSDC;
    wcx_->lpfnWndProc = WndProc;
    wcx_->cbClsExtra = 0;
    wcx_->cbWndExtra = 0;
    wcx_->hInstance = GetModuleHandle(nullptr);
    wcx_->hIcon = nullptr;
    wcx_->hCursor = nullptr;
    wcx_->hbrBackground = nullptr;
    wcx_->lpszMenuName = nullptr;
    wcx_->lpszClassName = "GuiWindow";
    wcx_->hIconSm = nullptr;
    ::RegisterClassEx(wcx_.get());

    hwnd_ = ::CreateWindow(wcx_->lpszClassName, name.data(), WS_OVERLAPPEDWINDOW, x, y, width, height, nullptr, nullptr, wcx_->hInstance, nullptr);
    ::SetWindowLongPtr(hwnd_, GWLP_USERDATA, (LONG_PTR)this);
    SPDLOG_INFO("hwnd: {}", (void*)hwnd_);

    InitIcon(hwnd_);

    imgui_ = std::make_unique<ImGuiEngine>(hwnd_);

    ::ShowWindow(hwnd_, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd_);
}

GuiWindow::~GuiWindow()
{
    ::DestroyWindow(hwnd_);
    ::UnregisterClass(wcx_->lpszClassName, wcx_->hInstance);
}

void GuiWindow::InitIcon(HWND hwnd)
{
    HICON hIcon = (HICON)::LoadImage(wcx_->hInstance, MAKEINTRESOURCE(IDR_MAINFRAME), IMAGE_ICON,
        ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

    HICON hIconSmall = (HICON)::LoadImage(wcx_->hInstance, MAKEINTRESOURCE(IDR_MAINFRAME), IMAGE_ICON,
        ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
}

void GuiWindow::UpdateSize(UINT width, UINT height)
{
    SPDLOG_DEBUG("WndSize: {}x{}", width, height);
    imgui_->SetMainSize(width, height);
}

void GuiWindow::PollMessage()
{
    bool done = false;
    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        imgui_->ResetMainWnd();
        imgui_->ShowWidgetWnd();
        imgui_->SetMainColor(0.45f, 0.55f, 0.60f, 1.00f);
        imgui_->ShowMainWnd(true);
    }
}

int GuiMain(int argc, char *argv[])
{
    GuiWindow gui(UTF8toANSI("WinService"), 100, 100, 1280, 800);
    gui.PollMessage();
    return 0;
}

bool ResetConsolePosition(HANDLE stdOut)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(stdOut, &csbi)) {
        SPDLOG_ERROR("GetConsoleScreenBufferInfo failed. WinApi@");
        return false;
    }

    if (csbi.dwCursorPosition.X == 0 && csbi.dwCursorPosition.Y == 0) {
        SPDLOG_WARN("GetConsoleScreenBufferInfo({},{}) ignore.",
            csbi.dwCursorPosition.X, csbi.dwCursorPosition.Y);
        return false;
    }

    DWORD charsWritten;
    csbi.dwCursorPosition.X = 0;
    FillConsoleOutputCharacter(stdOut, ' ', csbi.dwSize.X, csbi.dwCursorPosition, &charsWritten);

    csbi.dwCursorPosition.Y -= 1;
    FillConsoleOutputCharacter(stdOut, ' ', csbi.dwSize.X, csbi.dwCursorPosition, &charsWritten);

    if (!SetConsoleCursorPosition(stdOut, csbi.dwCursorPosition)) {
        SPDLOG_ERROR("SetConsoleCursorPosition({},{}) failed. WinApi@",
            csbi.dwCursorPosition.X, csbi.dwCursorPosition.Y);
        return false;
    }
    return true;
}

int ConsoleMainProxy(int argc, char *argv[])
{
    bool hasConsole = (AttachConsole(ATTACH_PARENT_PROCESS) != FALSE);
    if (!hasConsole)
        AllocConsole();

    HANDLE stdIn = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    ResetConsolePosition(stdOut);
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleMode(stdIn, ENABLE_INSERT_MODE);

    int ret = ConsoleMain(argc, argv, hasConsole);
    FreeConsole();
    return ret;
}

int ExceptionHandler(_EXCEPTION_POINTERS* ex)
{
    SPDLOG_INFO("*** Exception 0X{:X} occured ***", ex->ExceptionRecord->ExceptionCode);
    PrintStackContext(ex->ContextRecord);
    return EXCEPTION_EXECUTE_HANDLER;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    InitSpdlog(true, true);

    CHAR workDir[MAX_PATH] = {0,};
    GetCurrentDirectoryA(MAX_PATH, workDir);
    SPDLOG_INFO("Work directory at: [{}]", workDir);

    __try
    {
        if (__argc > 1) {
            ConsoleMainProxy(__argc, __argv);
        } else {
            GuiMain(__argc, __argv);
        }
    }
    __except(ExceptionHandler(GetExceptionInformation()))
    {
        SPDLOG_ERROR("Exception at WinMain!");
    }

    return 0;
}
