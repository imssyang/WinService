#pragma once

#include <d3d11.h>
#include "util/WsmUtil.h"

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

class ImGuiEngine
{
public:
    ImGuiEngine(HWND hwnd);
    ~ImGuiEngine();

    void ResetMainWnd();
    void SetMainSize(int width, int height);
    void SetMainColor(float x, float y, float z, float w);
    void ShowMainWnd(bool hasVsync);

    void ShowDemoWnd();
    void ShowTableWnd();

private:
    std::unique_ptr<D3D11Device> dx11;
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

private:
    HWND hwnd_;
    UINT resizeWidth_;
    UINT resizeHeight_;
    static std::unique_ptr<WNDCLASSEX> wcx_;
    static std::unique_ptr<ImGuiEngine> imgui_;
};

int GuiMain(int argc, char *argv[]);
