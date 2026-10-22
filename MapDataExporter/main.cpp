// 맵 데이터 추출 프로그램. Dear ImGui의 Win32 + DirectX 11 예제를 바탕으로 창과 장치를 만든다.
#include "ExporterApp.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <d3d11.h>
#include <windows.h>

static ID3D11Device* g_device = nullptr;
static ID3D11DeviceContext* g_device_context = nullptr;
static IDXGISwapChain* g_swap_chain = nullptr;
static ID3D11RenderTargetView* g_render_target_view = nullptr;
static bool g_is_swap_chain_occluded = false;
static UINT g_resize_width = 0;
static UINT g_resize_height = 0;

static void CreateRenderTarget()
{
    ID3D11Texture2D* back_buffer = nullptr;
    g_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    g_device->CreateRenderTargetView(back_buffer, nullptr, &g_render_target_view);
    back_buffer->Release();
}

static void CleanupRenderTarget()
{
    if (g_render_target_view)
    {
        g_render_target_view->Release();
        g_render_target_view = nullptr;
    }
}

static bool CreateDeviceD3D(HWND window)
{
    DXGI_SWAP_CHAIN_DESC desc = {};
    desc.BufferCount = 2;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferDesc.RefreshRate.Numerator = 60;
    desc.BufferDesc.RefreshRate.Denominator = 1;
    desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = window;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    const D3D_FEATURE_LEVEL feature_levels[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL feature_level;
    HRESULT result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, feature_levels, 2, D3D11_SDK_VERSION, &desc, &g_swap_chain, &g_device, &feature_level, &g_device_context);
    if (result == DXGI_ERROR_UNSUPPORTED)
    {
        result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, feature_levels, 2, D3D11_SDK_VERSION, &desc, &g_swap_chain, &g_device, &feature_level, &g_device_context);
    }
    if (result != S_OK)
    {
        return false;
    }
    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_swap_chain)
    {
        g_swap_chain->Release();
        g_swap_chain = nullptr;
    }
    if (g_device_context)
    {
        g_device_context->Release();
        g_device_context = nullptr;
    }
    if (g_device)
    {
        g_device->Release();
        g_device = nullptr;
    }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM w_param, LPARAM l_param);

static LRESULT WINAPI WindowProc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
    if (ImGui_ImplWin32_WndProcHandler(window, message, w_param, l_param))
    {
        return true;
    }
    switch (message)
    {
    case WM_SIZE:
        if (w_param == SIZE_MINIMIZED)
        {
            return 0;
        }
        g_resize_width = LOWORD(l_param);
        g_resize_height = HIWORD(l_param);
        return 0;
    case WM_SYSCOMMAND:
        if ((w_param & 0xfff0) == SC_KEYMENU)
        {
            return 0;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, w_param, l_param);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    ImGui_ImplWin32_EnableDpiAwareness();
    const float dpi_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));
    WNDCLASSEXW window_class = { sizeof(window_class), CS_CLASSDC, WindowProc, 0, 0, instance, nullptr, nullptr, nullptr, nullptr, L"MapDataExporter", nullptr };
    RegisterClassExW(&window_class);
    HWND window = CreateWindowW(window_class.lpszClassName, L"맵 데이터 추출", WS_OVERLAPPEDWINDOW, 100, 100, static_cast<int>(1200 * dpi_scale), static_cast<int>(820 * dpi_scale), nullptr, nullptr, instance, nullptr);
    if (!CreateDeviceD3D(window))
    {
        CleanupDeviceD3D();
        UnregisterClassW(window_class.lpszClassName, instance);
        return 1;
    }
    ShowWindow(window, SW_SHOWDEFAULT);
    UpdateWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(dpi_scale);
    io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/malgun.ttf", 17.0f * dpi_scale, nullptr, io.Fonts->GetGlyphRangesKorean());
    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(g_device, g_device_context);

    {
        ExporterApp app;
        bool is_done = false;
        while (!is_done)
        {
            MSG message;
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&message);
                DispatchMessageW(&message);
                is_done |= message.message == WM_QUIT;
            }
            if (is_done)
            {
                break;
            }
            if (g_is_swap_chain_occluded && g_swap_chain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
            {
                Sleep(10);
                continue;
            }
            g_is_swap_chain_occluded = false;
            if (g_resize_width != 0 && g_resize_height != 0)
            {
                CleanupRenderTarget();
                g_swap_chain->ResizeBuffers(0, g_resize_width, g_resize_height, DXGI_FORMAT_UNKNOWN, 0);
                g_resize_width = 0;
                g_resize_height = 0;
                CreateRenderTarget();
            }

            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            app.Draw();
            ImGui::Render();
            const float clear_color[4] = { 0.1f, 0.1f, 0.12f, 1.0f };
            g_device_context->OMSetRenderTargets(1, &g_render_target_view, nullptr);
            g_device_context->ClearRenderTargetView(g_render_target_view, clear_color);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            g_is_swap_chain_occluded = g_swap_chain->Present(1, 0) == DXGI_STATUS_OCCLUDED;
        }
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(window);
    UnregisterClassW(window_class.lpszClassName, instance);
    return 0;
}
