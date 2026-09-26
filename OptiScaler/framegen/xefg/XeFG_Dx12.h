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
    // (stale history from before the pause must not seed new bursts) and on
    // camera-cut detection (see Dispatch). Reset frames cost no presents and
    // no latency: only the interpolated content of that burst goes history-free.
    bool _forceResetNext = false;

    // Previous view matrix for camera-cut detection, with validity flag.
    float _prevViewMatrix[16] = {};
    bool _hasPrevViewMatrix = false;

    // Dynamic MFG policy state: base-frame budget accumulator over the eval
    // window, consecutive up-votes for hysteresis (fast down, slow up), and
    // the base frame time recorded at the last step-up (climbing stops if it
    // degrades the base frame by more than 20%).
    double _autoMfgAccumMs = 0.0;
    uint32_t _autoMfgSamples = 0;
    uint32_t _autoMfgUpVotes = 0;
    double _autoMfgBaseAtStepMs = 0.0;

    // Frame-time budget policy: returns the wanted interpolation count.
    int EvaluateAutoMFG(int fIndex);
    // Runtime count switch without the 10-frame toggle pause. On provider
    // error falls back to the legacy WAR toggle path.
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
