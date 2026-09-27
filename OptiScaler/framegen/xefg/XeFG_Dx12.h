#pragma once

#include <framegen/IFGFeature_Dx12.h>

#include <proxies/XeLL_Proxy.h>
#include <proxies/XeFG_Proxy.h>

#include "shaders/depth_invert/DI_Dx12.h"

#include <xell.h>
#include <xell_d3d12.h>
#include <xefg_swapchain.h>
#include <xefg_swapchain_d3d12.h>
#include <xefg_swapchain_debug.h>

class XeFG_Dx12 : public virtual IFGFeature_Dx12
{
  private:
    xefg_swapchain_handle_t _swapChainContext = nullptr;
    xefg_swapchain_handle_t _fgContext = nullptr;

    uint32_t _width = 0;
    uint32_t _height = 0;
    bool _infiniteDepth = false;
    std::optional<bool> _haveHudless = std::nullopt;
    bool _uiComposition = false;

    // Last frameRenderTime fed to the provider. Used to slew-limit upward
    // steps and break the measured-period feedback loop (see Dispatch).
    float _lastFedFrameTimeMs = 0.0f;

    // Consecutive starved Presents (no NewFrame within the watchdog window).
    // A single hitch skews present IDs without meaning starvation.
    uint32_t _presentStarveCount = 0;

    // One-shot history reset consumed by the next Dispatch. Set on Activate
    // (stale history from before the pause must not seed new bursts) and on a
    // detected teleport-sized camera cut (see Dispatch). Reset frames cost no
    // presents and no latency: only the interpolated content of that burst
    // goes history-free.
    // Gear changes (ApplyInterpolationCountSmooth) do NOT reset history
    // to avoid dropping a frame of interpolation on every step.
    bool _forceResetNext = false;

    // Previous view matrix for camera-cut detection, with validity flag.
    float _prevViewMatrix[16] = {};
    bool _hasPrevViewMatrix = false;

    // Edge trigger for the teleport-sized cut reset: fires once per cut event
    // and re-arms when the delta drops back below the threshold.
    bool _cameraCutLatched = false;

    // Cached projection inputs: avoids rebuilding the perspective matrix when
    // camera params are unchanged (Dispatch runs every burst).
    float _projCacheNear = 0.0f;
    float _projCacheFar = 0.0f;
    float _projCacheVFov = 0.0f;
    float _projCacheAspect = 0.0f;
    bool _projCacheValid = false;

    // Consecutive bursts whose fed frameRenderTime hit the upper clamp. Two
    // clamps in a row mean the scene genuinely renders slower than the cap,
    // so the provider sizes its interval from a number that is not this
    // frame's period - request one history reset and re-anchor.
    uint32_t _fedClampStreak = 0;

    // Runtime count switch without the 10-frame toggle pause (manual 2X-8X
    // changes). On provider error falls back to the legacy WAR toggle path.
    void ApplyInterpolationCountSmooth(int count);

    std::unique_ptr<DI_Dx12> _depthInvert;

    static void xefgLogCallback(const char* message, xefg_swapchain_logging_level_t level, void* userData);

    bool CreateSwapchainContext(ID3D12Device* device);
    bool DestroySwapchainContext();
    xefg_swapchain_d3d12_resource_data_t GetResourceData(FG_ResourceType type, int index = -1);

    bool Dispatch();

  protected:
    void ReleaseObjects() override final;
    void CreateObjects(ID3D12Device* InDevice) override final;

  public:
    // IFGFeature
    const char* Name() override final;
    feature_version Version() override final;
    HWND Hwnd() override final;

    // IFGFeature_Dx12
    bool CreateSwapchainInternal(IDXGIFactory* factory, ID3D12CommandQueue* cmdQueue, DXGI_SWAP_CHAIN_DESC* desc,
                                 IDXGISwapChain** swapChain) override final;
    bool CreateSwapchain1Internal(IDXGIFactory* factory, ID3D12CommandQueue* cmdQueue, HWND hwnd,
                                  DXGI_SWAP_CHAIN_DESC1* desc, DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
                                  IDXGISwapChain1** swapChain) override final;

    bool ReleaseSwapchain(HWND hwnd) override final;

    void CreateContext(ID3D12Device* device, FG_Constants& fgConstants) override final;
    void Activate() override final;
    void Deactivate() override final;
    void DestroyFGContext() override final;
    bool Shutdown() override final;

    void EvaluateState(ID3D12Device* device, FG_Constants& fgConstants) override final;

    bool Present() override final;

    bool SetResource(Dx12Resource* inputResource) override final;
    void SetCommandQueue(FG_ResourceType type, ID3D12CommandQueue* queue) override final;

    void* FrameGenerationContext() override final;
    void* SwapchainContext() override final;

    XeFG_Dx12() : IFGFeature_Dx12(), IFGFeature()
    {
        if (XeFGProxy::Module() == nullptr)
            XeFGProxy::InitXeFG();
    }

    ~XeFG_Dx12();

    // Inherited via IFGFeature_Dx12
    bool SetInterpolatedFrameCount(UINT interpolatedFrameCount) override;
};
