#include "pch.h"
#include "XeFG_Dx12.h"

#include <hudfix/Hudfix_Dx11.h>
#include <hudfix/Hudfix_Dx12.h>

#include <menu/menu_overlay_dx.h>
#include <resource_tracking/ResTrack_dx12.h>

#include <nvapi/fakenvapi.h>
#include <proxies/XeFGPacing.h>

#include <magic_enum.hpp>

#include <DirectXMath.h>

using namespace DirectX;

void XeFG_Dx12::xefgLogCallback(const char* message, xefg_swapchain_logging_level_t level, void* userData)
{
    switch (level)
    {
    case XEFG_SWAPCHAIN_LOGGING_LEVEL_DEBUG:
        spdlog::debug("XeFG Log: {}", message);
        return;

    case XEFG_SWAPCHAIN_LOGGING_LEVEL_INFO:
        spdlog::info("XeFG Log: {}", message);
        return;

    case XEFG_SWAPCHAIN_LOGGING_LEVEL_WARNING:
        spdlog::warn("XeFG Log: {}", message);
        return;

    default:
        spdlog::error("XeFG Log: {}", message);
        return;
    }
}

bool XeFG_Dx12::CreateSwapchainContext(ID3D12Device* device)
{
    if (XeFGProxy::Module() == nullptr && !XeFGProxy::InitXeFG())
    {
        LOG_ERROR("XeFG proxy can't find libxess_fg.dll!");
        return false;
    }

    auto createResult = false;

#ifndef DONT_USE_XMX
    ScopedSkipSpoofingGlobal skipSpoofingGlobal {};
#endif // !DONT_USE_XMX

    do
    {
        auto result = XeFGProxy::D3D12CreateContext()(device, &_swapChainContext);

        if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
        {
            LOG_ERROR("D3D12CreateContext error: {} ({})", magic_enum::enum_name(result), (UINT) result);
            return false;
        }

        LOG_INFO("XeFG context created");
        result = XeFGProxy::SetLoggingCallback()(_swapChainContext, XEFG_SWAPCHAIN_LOGGING_LEVEL_DEBUG, xefgLogCallback,
                                                 nullptr);

        if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
        {
            LOG_ERROR("SetLoggingCallback error: {} ({})", magic_enum::enum_name(result), (UINT) result);
        }

#ifndef LOW_LATENCY_INPUTS
        // Force fakenvapi to create XeLL for us
        if (fakenvapi::forceMode(device, LowLatencyMode::XeLL))
        {
            xell_sleep_params_t sleepParams = {};
            sleepParams.bLowLatencyMode = true;
            sleepParams.bLowLatencyBoost = false;
            // Seed XeLL pacing from the configured frame limit. XellHooks::update()
            // only syncs the *game's* context, so our own context would otherwise
            // stay unlimited (0) forever. FramerateLimit <= 0 keeps 0: unchanged.
            if (const float fpsCap = Config::Instance()->FramerateLimit.value_or_default(); fpsCap > 0.0f)
                sleepParams.minimumIntervalUs = static_cast<uint32_t>(std::round(1000000.0f / fpsCap));
            else
                sleepParams.minimumIntervalUs = 0;

            auto xellResult =
                XeLLProxy::SetSleepMode()((xell_context_handle_t) fakenvapi::getCurrentContext(), &sleepParams);
            if (xellResult != XELL_RESULT_SUCCESS)
            {
                LOG_ERROR("SetSleepMode error: {} ({})", magic_enum::enum_name(xellResult), (UINT) xellResult);
                return false;
            }

            result = XeFGProxy::SetLatencyReduction()(_swapChainContext, fakenvapi::getCurrentContext());

            if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
            {
                LOG_ERROR("SetLatencyReduction error: {} ({})", magic_enum::enum_name(result), (UINT) result);
                return false;
            }
        }
#else
        InputXeLL::xell_input_handle_t localXellContext;
        if (InputXeLL::D3D12CreateContext(device, &localXellContext) == XELL_RESULT_SUCCESS)
        {
            localXellContext->inputContext.localContext = true; // We created this context

            xell_sleep_params_t sleepParams = {};
            sleepParams.bLowLatencyMode = true;
            sleepParams.bLowLatencyBoost = false;
            // Same seeding as the fakenvapi branch above (see comment there).
            if (const float fpsCap = Config::Instance()->FramerateLimit.value_or_default(); fpsCap > 0.0f)
                sleepParams.minimumIntervalUs = static_cast<uint32_t>(std::round(1000000.0f / fpsCap));
            else
                sleepParams.minimumIntervalUs = 0;

            auto xellResult = InputXeLL::SetSleepMode(localXellContext, &sleepParams);
            if (xellResult != XELL_RESULT_SUCCESS)
            {
                LOG_ERROR("SetSleepMode error: {} ({})", magic_enum::enum_name(xellResult), (UINT) xellResult);
                return false;
            }

            result = XeFGProxy::SetLatencyReduction()(_swapChainContext, (xell_context_handle_t) localXellContext);

            if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
            {
                LOG_ERROR("SetLatencyReduction error: {} ({})", magic_enum::enum_name(result), (UINT) result);
                return false;
            }
        }
#endif
        else
        {
            LOG_ERROR("Couldn't create XeLL");
            return false;
        }

        // FG/XeLL ceiling check: both DLLs gate the multiplier independently
        // (FG reports it, XeLL validates it). FG unlocked past 4X while XeLL
        // still clamps at its stock ceiling is guaranteed judder - surface it
        // here instead of leaving it as mystery stutter.
        {
            const int32_t wantInterp = Config::Instance()->FGXeFGMaxInterpolatedFrames.value_or_default();

            if (wantInterp > 3 && XeFGUnlock::Applied() && !XeLLUnlock::Applied())
            {
                LOG_WARN("XeFG/XeLL ceiling mismatch: FG unlocked to {}X but the XeLL unlock did not apply "
                         "(unrecognised libxell.dll build?); expect judder above 4X",
                         wantInterp + 1);
            }
        }

        createResult = true;

    } while (false);

    return createResult;
}

const char* XeFG_Dx12::Name()
{
    static std::string nameBuffer;

    if (_maxInterpolationCount == 1 || _framesToInterpolate < 0)
    {
        nameBuffer = "XeFG";
    }
    else
    {
        auto count = _framesToInterpolate + 1;
        nameBuffer = "XeFG " + std::to_string(count) + "x";
    }

    return nameBuffer.c_str();
}

feature_version XeFG_Dx12::Version()
{
    if (XeFGProxy::InitXeFG())
    {
        auto ver = XeFGProxy::Version();
        return feature_version(ver.major, ver.minor, ver.patch);
    }

    return { 0, 0, 0 };
}

HWND XeFG_Dx12::Hwnd() { return _hwnd; }

bool XeFG_Dx12::DestroySwapchainContext()
{
    LOG_DEBUG("");

    if (_swapChainContext != nullptr && !State::Instance().isShuttingDown)
    {
        auto context = _swapChainContext;
        _swapChainContext = nullptr;

        auto result = XeFGProxy::Destroy()(context);

        LOG_INFO("Destroy result: {} ({})", magic_enum::enum_name(result), (UINT) result);

        // Set it back because context is not destroyed
        if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
        {
            _swapChainContext = context;
        }
        else
        {
            State::Instance().currentFGSwapchain = nullptr;
        }
    }

    return true;
}

xefg_swapchain_d3d12_resource_data_t XeFG_Dx12::GetResourceData(FG_ResourceType type, int index)
{
    if (index < 0)
    {
        index = GetIndex();
        LOG_WARN("GetResourceData called with -1 index, using current index: {}", index);
    }

    xefg_swapchain_d3d12_resource_data_t resourceParam = {};

    if (!_frameResources[index].contains(type))
    {
        LOG_WARN("Resource type not found: {} for index: {}", magic_enum::enum_name(type), index);
        return resourceParam;
    }

    auto fResource = &_frameResources[index].at(type);

    resourceParam.validity = (fResource->validity == FG_ResourceValidity::ValidNow)
                                 ? XEFG_SWAPCHAIN_RV_ONLY_NOW
                                 : XEFG_SWAPCHAIN_RV_UNTIL_NEXT_PRESENT;

    resourceParam.resourceBase = { fResource->left, fResource->top };
    resourceParam.resourceSize = { static_cast<uint32_t>(fResource->width), fResource->height };
    resourceParam.pResource = fResource->GetResource();
    resourceParam.incomingState = fResource->state;

    switch (type)
    {
    case FG_ResourceType::Depth:
        resourceParam.type = XEFG_SWAPCHAIN_RES_DEPTH;
        break;

    case FG_ResourceType::HudlessColor:
        resourceParam.type = XEFG_SWAPCHAIN_RES_HUDLESS_COLOR;
        break;

    case FG_ResourceType::UIColor:
        resourceParam.type = XEFG_SWAPCHAIN_RES_UI;
        break;

    case FG_ResourceType::Velocity:
        resourceParam.type = XEFG_SWAPCHAIN_RES_MOTION_VECTOR;
        break;
    default:
        LOG_WARN("Unsupported resource type: {}", magic_enum::enum_name(type));
        return xefg_swapchain_d3d12_resource_data_t {};
    }

    return resourceParam;
}

bool XeFG_Dx12::CreateSwapchainInternal(IDXGIFactory* factory, ID3D12CommandQueue* cmdQueue, DXGI_SWAP_CHAIN_DESC* desc,
                                        IDXGISwapChain** swapChain)
{
    if (_swapChainContext == nullptr)
    {
        LOG_DEBUG("Creating swapchain context for the first time");

        if (State::Instance().currentD3D12Device == nullptr)
            return false;

        CreateSwapchainContext(State::Instance().currentD3D12Device);

        if (_swapChainContext == nullptr)
            return false;

        _width = desc->BufferDesc.Width;
        _height = desc->BufferDesc.Height;

        xefg_swapchain_properties_t props {};
        auto result = XeFGProxy::GetProperties()(_swapChainContext, &props);
        if (result == XEFG_SWAPCHAIN_RESULT_SUCCESS)
        {
            _maxInterpolationCount = props.maxSupportedInterpolations;
            LOG_INFO("Max supported interpolations: {}", props.maxSupportedInterpolations);
        }
        else
        {
            LOG_ERROR("Can't get swapchain properties: {} ({})", magic_enum::enum_name(result), (UINT) result);
        }
    }

    IDXGIFactory* realFactory = nullptr;
    ID3D12CommandQueue* realQueue = nullptr;

    if (!CheckForRealObject(__FUNCTION__, factory, (IUnknown**) &realFactory))
        realFactory = factory;

    if (!CheckForRealObject(__FUNCTION__, cmdQueue, (IUnknown**) &realQueue))
        realQueue = cmdQueue;

    IDXGIFactory2* factory12 = nullptr;
    if (realFactory->QueryInterface(IID_PPV_ARGS(&factory12)) != S_OK)
        return false;

    factory12->Release();

    HWND hwnd = desc->OutputWindow;
    DXGI_SWAP_CHAIN_DESC1 scDesc {};

    scDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE; // No info
    scDesc.BufferCount = desc->BufferCount;
    scDesc.BufferUsage = desc->BufferUsage;
    scDesc.Flags = desc->Flags;
    scDesc.Format = desc->BufferDesc.Format;
    scDesc.Height = desc->BufferDesc.Height;
    scDesc.SampleDesc = desc->SampleDesc;

    switch (desc->BufferDesc.Scaling)
    {
    case DXGI_MODE_SCALING_CENTERED:
        scDesc.Scaling = DXGI_SCALING_ASPECT_RATIO_STRETCH;
        break;

    case DXGI_MODE_SCALING_STRETCHED:
        scDesc.Scaling = DXGI_SCALING_STRETCH;
        break;

    case DXGI_MODE_SCALING_UNSPECIFIED:
        scDesc.Scaling = DXGI_SCALING_NONE;
        break;
    }

    scDesc.Stereo = false; // No info
    scDesc.SwapEffect = desc->SwapEffect;
    scDesc.Width = desc->BufferDesc.Width;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsDesc {};
    fsDesc.RefreshRate = desc->BufferDesc.RefreshRate;
    fsDesc.Scaling = desc->BufferDesc.Scaling;
    fsDesc.ScanlineOrdering = desc->BufferDesc.ScanlineOrdering;
    fsDesc.Windowed = desc->Windowed;

    xefg_swapchain_d3d12_init_params_t params {};

    int intTarget = _maxInterpolationCount;

    // For old libxess_fg versions we use max to control interpolation count
    if (XeFGProxy::SetNumInterpolatedFrames() == nullptr)
        intTarget = Config::Instance()->FGXeFGInterpolationCount.value_or_default();

    if (intTarget < 1 || intTarget > _maxInterpolationCount)
    {
        LOG_WARN("Invalid XeFG interpolation count: {}, max count: {}", intTarget, _maxInterpolationCount);

        intTarget = 1;
    }

    if (_framesToInterpolate > intTarget)
        Config::Instance()->FGXeFGInterpolationCount.set_volatile_value(intTarget);

    // ForceXeLL means latency-only: keep FG off the init path instead of
    // having the next line silently overwrite maxInterpolatedFrames = 1.
    params.maxInterpolatedFrames =
        Config::Instance()->ForceXeLL.value_or_default() ? 1 : intTarget;

    params.initFlags = XEFG_SWAPCHAIN_INIT_FLAG_NONE;

    if (Config::Instance()->FGXeFGDepthInverted.value_or_default())
        params.initFlags |= XEFG_SWAPCHAIN_INIT_FLAG_INVERTED_DEPTH;

    if (Config::Instance()->FGXeFGJitteredMV.value_or_default())
        params.initFlags |= XEFG_SWAPCHAIN_INIT_FLAG_JITTERED_MV;

    if (Config::Instance()->FGXeFGHighResMV.value_or_default())
        params.initFlags |= XEFG_SWAPCHAIN_INIT_FLAG_HIGH_RES_MV;

    if (!Config::Instance()->FGUIPremultipliedAlpha.value_or_default())
        params.initFlags |= XEFG_SWAPCHAIN_INIT_FLAG_UITEXTURE_NOT_PREMUL_ALPHA;

    LOG_DEBUG("Inverted Depth: {}", Config::Instance()->FGXeFGDepthInverted.value_or_default());
    LOG_DEBUG("Jittered Velocity: {}", Config::Instance()->FGXeFGJitteredMV.value_or_default());
    LOG_DEBUG("High Res MV: {}", Config::Instance()->FGXeFGHighResMV.value_or_default());

    if (Config::Instance()->FGXeFGDepthInverted.value_or_default())
        _constants.flags |= FG_Flags::InvertedDepth;

    if (Config::Instance()->FGXeFGJitteredMV.value_or_default())
        _constants.flags |= FG_Flags::JitteredMVs;

    if (Config::Instance()->FGXeFGHighResMV.value_or_default())
        _constants.flags |= FG_Flags::DisplayResolutionMVs;

#ifndef DONT_USE_XMX
    ScopedSkipSpoofingGlobal skipSpoofingGlobal {};
#endif // !DONT_USE_XMX

    xefg_swapchain_result_t result;
    result = XeFGProxy::D3D12InitFromSwapChainDesc()(_swapChainContext, hwnd, &scDesc, &fsDesc, realQueue, factory12,
                                                     &params);

    if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
    {
        LOG_ERROR("D3D12InitFromSwapChainDesc error: {} ({:X})", magic_enum::enum_name(result), (UINT) result);
        return false;
    }

    LOG_INFO("XeFG swapchain created");
    result = XeFGProxy::D3D12GetSwapChainPtr()(_swapChainContext, IID_PPV_ARGS(swapChain));
    if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
    {
        LOG_ERROR("D3D12GetSwapChainPtr error: {} ({})", magic_enum::enum_name(result), (UINT) result);
        return false;
    }

    // When forcing XeLL, always tell XeFG that FG is active, even tho we don't send anything
    if (State::Instance().activeFgInput == FGInput::ForceXeLL)
    {
        XeFGProxy::SetEnabled()(_swapChainContext, true);
    }

    _gameCommandQueue = realQueue;
    _swapChain = *swapChain;
    _hwnd = hwnd;

    return true;
}

bool XeFG_Dx12::CreateSwapchain1Internal(IDXGIFactory* factory, ID3D12CommandQueue* cmdQueue, HWND hwnd,
                                         DXGI_SWAP_CHAIN_DESC1* desc, DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
                                         IDXGISwapChain1** swapChain)
{
    if (_swapChainContext == nullptr)
    {
        if (State::Instance().currentD3D12Device == nullptr)
            return false;

        CreateSwapchainContext(State::Instance().currentD3D12Device);

        if (_swapChainContext == nullptr)
            return false;

        _width = desc->Width;
        _height = desc->Height;

        xefg_swapchain_properties_t props {};
        auto result = XeFGProxy::GetProperties()(_swapChainContext, &props);
        if (result == XEFG_SWAPCHAIN_RESULT_SUCCESS)
        {
            _maxInterpolationCount = props.maxSupportedInterpolations;
            LOG_INFO("Max supported interpolations: {}", props.maxSupportedInterpolations);
        }
        else
        {
            LOG_ERROR("Can't get swapchain properties: {} ({})", magic_enum::enum_name(result), (UINT) result);
        }
    }

    IDXGIFactory* realFactory = nullptr;
    ID3D12CommandQueue* realQueue = nullptr;

    if (!CheckForRealObject(__FUNCTION__, factory, (IUnknown**) &realFactory))
        realFactory = factory;

    if (!CheckForRealObject(__FUNCTION__, cmdQueue, (IUnknown**) &realQueue))
        realQueue = cmdQueue;

    IDXGIFactory2* factory12 = nullptr;
    if (realFactory->QueryInterface(IID_PPV_ARGS(&factory12)) != S_OK)
        return false;

    factory12->Release();

    xefg_swapchain_d3d12_init_params_t params {};

    int intTarget = _maxInterpolationCount;

    // For old libxess_fg versions we use max to control interpolation count
    if (XeFGProxy::SetNumInterpolatedFrames() == nullptr)
        intTarget = Config::Instance()->FGXeFGInterpolationCount.value_or_default();

    if (intTarget < 1 || intTarget > _maxInterpolationCount)
    {
        LOG_WARN("Invalid XeFG interpolation count: {}, max count: {}", intTarget, _maxInterpolationCount);

        intTarget = 1;
    }

    if (_framesToInterpolate > intTarget)
        Config::Instance()->FGXeFGInterpolationCount.set_volatile_value(intTarget);

    // ForceXeLL means latency-only: keep FG off the init path instead of
    // having the next line silently overwrite maxInterpolatedFrames = 1.
    params.maxInterpolatedFrames =
        Config::Instance()->ForceXeLL.value_or_default() ? 1 : intTarget;

    params.initFlags = XEFG_SWAPCHAIN_INIT_FLAG_NONE;

    if (Config::Instance()->FGXeFGDepthInverted.value_or_default())
        params.initFlags |= XEFG_SWAPCHAIN_INIT_FLAG_INVERTED_DEPTH;

    if (Config::Instance()->FGXeFGJitteredMV.value_or_default())
        params.initFlags |= XEFG_SWAPCHAIN_INIT_FLAG_JITTERED_MV;

    if (Config::Instance()->FGXeFGHighResMV.value_or_default())
        params.initFlags |= XEFG_SWAPCHAIN_INIT_FLAG_HIGH_RES_MV;

    if (!Config::Instance()->FGUIPremultipliedAlpha.value_or_default())
        params.initFlags |= XEFG_SWAPCHAIN_INIT_FLAG_UITEXTURE_NOT_PREMUL_ALPHA;

    LOG_DEBUG("Inverted Depth: {}", Config::Instance()->FGXeFGDepthInverted.value_or_default());
    LOG_DEBUG("Jittered Velocity: {}", Config::Instance()->FGXeFGJitteredMV.value_or_default());
    LOG_DEBUG("High Res MV: {}", Config::Instance()->FGXeFGHighResMV.value_or_default());

    if (Config::Instance()->FGXeFGDepthInverted.value_or_default())
        _constants.flags |= FG_Flags::InvertedDepth;

    if (Config::Instance()->FGXeFGJitteredMV.value_or_default())
        _constants.flags |= FG_Flags::JitteredMVs;

    if (Config::Instance()->FGXeFGHighResMV.value_or_default())
        _constants.flags |= FG_Flags::DisplayResolutionMVs;

    xefg_swapchain_result_t result;

    {
#ifndef DONT_USE_XMX
        ScopedSkipSpoofingGlobal skipSpoofingGlobal {};
#endif // !DONT_USE_XMX
        result = XeFGProxy::D3D12InitFromSwapChainDesc()(_swapChainContext, hwnd, desc, pFullscreenDesc, realQueue,
                                                         factory12, &params);
    }

    if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
    {
        LOG_ERROR("D3D12InitFromSwapChainDesc error: {} ({})", magic_enum::enum_name(result), (UINT) result);
        return false;
    }

    LOG_INFO("XeFG swapchain created");
    result = XeFGProxy::D3D12GetSwapChainPtr()(_swapChainContext, IID_PPV_ARGS(swapChain));
    if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
    {
        LOG_ERROR("D3D12GetSwapChainPtr error: {} ({})", magic_enum::enum_name(result), (UINT) result);
        return false;
    }

    // When forcing XeLL, always tell XeFG that FG is active, even tho we don't send anything
    if (State::Instance().activeFgInput == FGInput::ForceXeLL)
    {
        XeFGProxy::SetEnabled()(_swapChainContext, true);
    }

    _gameCommandQueue = realQueue;
    _swapChain = *swapChain;
    _hwnd = hwnd;

    return true;
}

void XeFG_Dx12::CreateContext(ID3D12Device* device, FG_Constants& fgConstants)
{
    LOG_DEBUG("");

    _device = device;
    CreateObjects(device);

    if (_fgContext == nullptr && _swapChainContext != nullptr)
    {
        _fgContext = _swapChainContext;
        _lastDispatchedFrame = 0;
    }

    if (_isActive)
    {
        LOG_INFO("FG context recreated while active, pausing");
        State::Instance().fgChanged = true;
        UpdateTarget();
        Deactivate();
    }
}

void XeFG_Dx12::Activate()
{
    LOG_DEBUG("");

    auto currentFeature = State::Instance().currentFeature;
    bool nativeAA = false;
    if (State::Instance().activeFgInput == FGInput::Upscaler && currentFeature != nullptr)
        nativeAA = currentFeature->RenderWidth() == currentFeature->DisplayWidth();

    if (_swapChainContext != nullptr && _fgContext != nullptr && !_isActive &&
        (IsLowResMV() || nativeAA || (State::Instance().gameQuirks & GameQuirk::ForceFGRenderSizeMVs) ||
         Config::Instance()->FGXeFGIgnoreInitChecks.value_or_default()))
    {
        auto result = XeFGProxy::SetEnabled()(_swapChainContext, true);

        if (result == XEFG_SWAPCHAIN_RESULT_SUCCESS)
        {
            _isActive = true;
            _lastDispatchedFrame = 0;

            // Fresh history for the new session: stale frames from before the
            // pause must not seed the first bursts (teleport smear).
            _forceResetNext = true;
            _hasPrevViewMatrix = false;

            // Fresh policy state: votes from the previous session (loading
            // sludge included) must not pin the first gear.
            _autoMfgAccumMs = 0.0;
            _autoMfgSamples = 0;
            _autoMfgUpVotes = 0;
            _autoMfgBaseAtStepMs = 0.0;
        }

        LOG_INFO("SetEnabled: true, result: {} ({})", magic_enum::enum_name(result), (UINT) result);
    }
}

void XeFG_Dx12::Deactivate()
{
    LOG_DEBUG("");

    if (_isActive)
    {
        auto fIndex = GetIndex();
        if (_uiCommandListResetted[fIndex])
        {
            LOG_DEBUG("Executing _uiCommandList[fIndex][{}]: {:X}", fIndex, (size_t) _uiCommandList[fIndex]);
            auto closeResult = _uiCommandList[fIndex]->Close();

            if (closeResult == S_OK)
                _gameCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList**) &_uiCommandList[fIndex]);
            else
                LOG_ERROR("_uiCommandList[{}]->Close() error: {:X}", fIndex, (UINT) closeResult);

            _gameCommandQueue->Signal(_uiFence, _uiAllocatorFenceValues[fIndex]);

            _uiCommandListResetted[fIndex] = false;
        }

        xefg_swapchain_result_t result = XEFG_SWAPCHAIN_RESULT_SUCCESS;

        if (_swapChainContext != nullptr)
        {
            result = XeFGProxy::SetEnabled()(_swapChainContext, false);
            if (result == XEFG_SWAPCHAIN_RESULT_SUCCESS)
                _isActive = false;
        }
        else
        {
            _isActive = false;
        }

        //_lastDispatchedFrame = 0;
        _waitingNewFrameData = false;

        // Drop the feed baseline so the next activation re-baselines instead
        // of slew-clamping a heavier scene against a stale value.
        _lastFedFrameTimeMs = 0.0f;

        LOG_INFO("SetEnabled: false, result: {} ({})", magic_enum::enum_name(result), (UINT) result);
    }
}

void XeFG_Dx12::DestroyFGContext()
{
    Deactivate();

    if (_fgContext != nullptr)
        _fgContext = nullptr;

    ReleaseObjects();
}

bool XeFG_Dx12::Shutdown()
{
    MenuOverlayDx::CleanupRenderTarget(true, NULL);

    if (_fgContext != nullptr)
        DestroyFGContext();

    ReleaseObjects();

    if (_swapChainContext != nullptr)
        DestroySwapchainContext();

    return true;
}

int XeFG_Dx12::EvaluateAutoMFG(int fIndex)
{
    int targetFps = Config::Instance()->FGXeFGAutoMFGTargetFps.value_or_default();

    if (targetFps < 30)
        targetFps = 30;
    else if (targetFps > 480)
        targetFps = 480;

    // The target is OUTPUT fps: outputNow = baseFps * (want + 1). Climb while
    // the output falls short of it; back off when the base frame itself
    // degrades (FG load included) or the output overshoots it.
    const double budgetMs = 1000.0 / targetFps;
    double dirtyMs = budgetMs * 3.0;

    if (dirtyMs < 50.0)
        dirtyMs = 50.0;

    const float sampleMs = (float) _ftDelta[fIndex];

    // Loading frames and hitch spikes must not vote: a 686 ms load would
    // otherwise pin the policy at the floor before gameplay even starts, and
    // a single hitch would panic-downgrade a healthy gear. Sustained
    // slowness still votes normally.
    if (sampleMs > 0.0f && sampleMs < 1000.0f && sampleMs <= dirtyMs)
    {
        _autoMfgAccumMs += sampleMs;
        _autoMfgSamples++;
    }

    int want = _framesToInterpolate;

    // One evaluation per ~30 dispatched frames. Downgrade fires on the first
    // bad window (fast down); upgrade needs two calm windows in a row (slow
    // up). A step costs one reset flash, so the bands stay wide.
    constexpr uint32_t EvalWindow = 30;

    if (_autoMfgSamples < EvalWindow)
        return want;

    const double avgMs = _autoMfgAccumMs / _autoMfgSamples;
    _autoMfgAccumMs = 0.0;
    _autoMfgSamples = 0;

    const int hi = _maxInterpolationCount;

    if (hi < 1)
        return want;

    // avgMs > 0 holds: every accumulated sample is > 0.
    int lo = Config::Instance()->FGXeFGAutoMFGMinFrames.value_or_default();

    if (lo < 1)
        lo = 1;

    if (lo > hi)
        lo = hi;

    // No `want < 1` fixup here: the final clamp already forces want >= lo >= 1.
    const double outputNow = (1000.0 / avgMs) * (want + 1);

    if (avgMs > 50.0 && want > lo)
    {
        // Base frame itself is unhealthy (< 20 fps): protect playability first.
        want--;
        _autoMfgUpVotes = 0;
    }
    else if (outputNow < targetFps * 0.9 && want < hi &&
             (_autoMfgBaseAtStepMs <= 0.0 || avgMs <= _autoMfgBaseAtStepMs * 1.2))
    {
        // Output falls short and climbing has not degraded the base frame:
        // step up after two consecutive calm windows.
        if (++_autoMfgUpVotes >= 2)
        {
            want++;
            _autoMfgUpVotes = 0;
            _autoMfgBaseAtStepMs = avgMs;
        }
    }
    else if (outputNow > targetFps * 1.5 && want > lo)
    {
        // Overshooting the target: save the GPU, the extra frames buy nothing.
        want--;
        _autoMfgUpVotes = 0;
    }
    else
    {
        _autoMfgUpVotes = 0;
    }

    if (want < lo)
        want = lo;
    else if (want > hi)
        want = hi;

    return want;
}

void XeFG_Dx12::ApplyInterpolationCountSmooth(int count)
{
    LOG_INFO("Interpolation count changed {} -> {} (smooth, no toggle pause)", _framesToInterpolate, count);

#ifndef DONT_USE_XMX
    ScopedSkipSpoofingGlobal skipSpoofingGlobal {};
#endif // !DONT_USE_XMX

    auto intResult = XeFGProxy::SetNumInterpolatedFrames()(_swapChainContext, count);

    // Mirror the legacy path on failure: keep the count but take the toggle
    // pause instead of leaving provider and shadow state disagreeing.
    _framesToInterpolate = count;

    if (intResult != XEFG_SWAPCHAIN_RESULT_SUCCESS)
    {
        LOG_ERROR("SetNumInterpolatedFrames error: {} ({}), falling back to toggle path",
                  magic_enum::enum_name(intResult), (UINT) intResult);
        State::Instance().WAR_xefgRequestFGToggle = true;
        return;
    }

    // Burst structure changed: reseed history on the next burst.
    if (Config::Instance()->FGXeFGAutoReset.value_or_default())
        _forceResetNext = true;
}

bool XeFG_Dx12::Dispatch()
{
    LOG_FUNC();

    UINT64 willDispatchFrame = 0;
    auto fIndex = GetDispatchIndex(willDispatchFrame);
    if (fIndex < 0)
        return false;

    if (!IsActive() || IsPaused())
        return false;

    LOG_DEBUG_ONLY("_frameCount: {}, willDispatchFrame: {}, fIndex: {}", _frameCount, willDispatchFrame, fIndex);

    if (!_resourceReady[fIndex].contains(FG_ResourceType::Depth) ||
        !_resourceReady[fIndex].at(FG_ResourceType::Depth) ||
        !_resourceReady[fIndex].contains(FG_ResourceType::Velocity) ||
        !_resourceReady[fIndex].at(FG_ResourceType::Velocity))
    {
        LOG_WARN("Depth or Velocity is not ready, skipping");
        return false;
    }

    auto& state = State::Instance();

    if (XeFGProxy::SetUiCompositionState() != nullptr &&
        Config::Instance()->FGXeFGUIComposition.value_or_default() != _uiComposition && IsUsingHudless(fIndex))
    {
        // To prevent XeLL issues
        LOG_DEBUG("UI Composition state changed {}, skipping rendering for 10 frames", _uiComposition);
        state.WAR_xefgRequestFGToggle = true;

        _uiComposition = Config::Instance()->FGXeFGUIComposition.value_or_default();

        auto uiState =
            _uiComposition ? XEFG_SWAPCHAIN_UI_COMPOSITION_STATE_ENABLED : XEFG_SWAPCHAIN_UI_COMPOSITION_STATE_DISABLED;

#ifndef DONT_USE_XMX
        ScopedSkipSpoofingGlobal skipSpoofingGlobal {};
#endif // !DONT_USE_XMX

        auto uiResult = XeFGProxy::SetUiCompositionState()(_swapChainContext, uiState);

        if (uiResult != XEFG_SWAPCHAIN_RESULT_SUCCESS)
            LOG_ERROR("SetUiCompositionState error: {} ({})", magic_enum::enum_name(uiResult), (UINT) uiResult);
    }

    if (XeFGProxy::SetNumInterpolatedFrames() != nullptr)
    {
        if (Config::Instance()->FGXeFGInterpolationCount.value_or_default() > _maxInterpolationCount)
        {
            Config::Instance()->FGXeFGInterpolationCount = _maxInterpolationCount;
            LOG_WARN("Requested interpolation count is higher than max supported, setting to max: {}",
                     _maxInterpolationCount);
        }

        int wantCount = Config::Instance()->FGXeFGInterpolationCount.value_or_default();

        // Dynamic MFG overrides the static target from the base-frame budget.
        // Reached only while active: Dispatch early-returns when paused.
        if (Config::Instance()->FGXeFGAutoMFG.value_or_default())
            wantCount = EvaluateAutoMFG(fIndex);

        if (_framesToInterpolate != wantCount)
            ApplyInterpolationCountSmooth(wantCount);
    }
    else if (Config::Instance()->FGXeFGAutoMFG.value_or_default())
    {
        // Old provider without the runtime count API: the policy has nothing
        // to drive. Say so once instead of leaving dead sliders.
        static bool autoMfgWarned = false;

        if (!autoMfgWarned)
        {
            autoMfgWarned = true;
            LOG_WARN("AutoMFG needs a libxess_fg with SetNumInterpolatedFrames; staying static");
        }
    }

    // Workaround for wrong frame limit
    if (state.WAR_xefgRequestFGToggle)
    {
        state.WAR_xefgRequestFGToggle = false;

        state.fgChanged = true;
        UpdateTarget();
        Deactivate();
    }

    if (!_haveHudless.has_value())
    {
        _haveHudless = IsUsingHudless(fIndex);
    }
    else
    {
        auto usingHudless = IsUsingHudless(fIndex);
        static auto version = Version();

        // SDK version 2.1.1 fixed this
        // https://github.com/intel/xess/issues/48
        if (version < feature_version { 1, 2, 2 } && _haveHudless.value() != usingHudless)
        {
            LOG_INFO("Hudless state changed {} -> {}, skipping rendering for 10 frames", _haveHudless.value(),
                     usingHudless);

            _haveHudless = usingHudless;
            state.fgChanged = true;
            UpdateTarget();
            Deactivate();

            return false;
        }
    }

    if (!_noHudless[fIndex])
    {
        auto res = &_frameResources[fIndex][FG_ResourceType::HudlessColor];
        if (res->validity != FG_ResourceValidity::ValidNow)
        {
            res->validity = FG_ResourceValidity::UntilPresentFromDispatch;
            res->frameIndex = fIndex;
            SetResource(res);
        }
    }

    if (!_noDistortionField[fIndex])
    {
        auto res = &_frameResources[fIndex][FG_ResourceType::Distortion];
        if (res->validity != FG_ResourceValidity::ValidNow)
        {
            res->validity = FG_ResourceValidity::UntilPresentFromDispatch;
            res->frameIndex = fIndex;
            SetResource(res);
        }
    }

    XeFGProxy::EnableDebugFeature()(_swapChainContext, XEFG_SWAPCHAIN_DEBUG_FEATURE_TAG_INTERPOLATED_FRAMES,
                                    Config::Instance()->FGXeFGDebugView.value_or_default(), nullptr);
    XeFGProxy::EnableDebugFeature()(_swapChainContext, XEFG_SWAPCHAIN_DEBUG_FEATURE_SHOW_ONLY_INTERPOLATION,
                                    state.fgOnlyGenerated, nullptr);

    xefg_swapchain_frame_constant_data_t constData = {};

    bool viewValid = false;

    if (_cameraPosition[fIndex][0] != 0.0f || _cameraPosition[fIndex][1] != 0.0f || _cameraPosition[fIndex][2] != 0.0f)
    {
        XMVECTOR right = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(_cameraRight[fIndex]));
        XMVECTOR up = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(_cameraUp[fIndex]));
        XMVECTOR forward = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(_cameraForward[fIndex]));
        XMVECTOR pos = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(_cameraPosition[fIndex]));

        float x = -XMVectorGetX(XMVector3Dot(pos, right));
        float y = -XMVectorGetX(XMVector3Dot(pos, up));
        float z = -XMVectorGetX(XMVector3Dot(pos, forward));

        XMMATRIX view = { XMVectorSet(XMVectorGetX(right), XMVectorGetX(up), XMVectorGetX(forward), 0.0f),
                          XMVectorSet(XMVectorGetY(right), XMVectorGetY(up), XMVectorGetY(forward), 0.0f),
                          XMVectorSet(XMVectorGetZ(right), XMVectorGetZ(up), XMVectorGetZ(forward), 0.0f),
                          XMVectorSet(x, y, z, 1.0f) };

        memcpy(constData.viewMatrix, view.r, sizeof(view));
        viewValid = true;
    }

    // Camera-cut self detection: games often forget to signal resetHistory on
    // cuts/teleports, and the stale history then smears the old scene over the
    // new one (teleport flash). Normal per-frame motion moves view elements by
    // ~0.01-0.2; a cut moves them by whole units, so 2.0 stays far above even
    // fast camera whips. Nanoseconds of CPU, no waits, no present-rate change.
    // Gated by [XeFG] AutoReset: off trusts the game's reset signal only.
    const bool autoReset = Config::Instance()->FGXeFGAutoReset.value_or_default();
    bool cameraCut = false;

    if (autoReset && viewValid)
    {
        if (_hasPrevViewMatrix)
        {
            float maxDelta = 0.0f;
            const auto* cur = reinterpret_cast<const float*>(constData.viewMatrix);

            for (int i = 0; i < 16; i++)
            {
                const float d = fabsf(cur[i] - _prevViewMatrix[i]);

                if (d > maxDelta)
                    maxDelta = d;
            }

            cameraCut = maxDelta > 2.0f;

            if (cameraCut)
                LOG_DEBUG("Camera cut detected (view delta {:.2f}), forcing history reset", maxDelta);
        }

        memcpy(_prevViewMatrix, constData.viewMatrix, sizeof(_prevViewMatrix));
        _hasPrevViewMatrix = true;
    }

    if (Config::Instance()->FGXeFGDepthInverted.value_or_default())
        std::swap(_cameraNear[fIndex], _cameraFar[fIndex]);

    if (_infiniteDepth && _cameraFar[fIndex] > _cameraNear[fIndex])
        _cameraFar[fIndex] = std::numeric_limits<float>::infinity();
    else if (_infiniteDepth && _cameraNear[fIndex] > _cameraFar[fIndex])
        _cameraNear[fIndex] = std::numeric_limits<float>::infinity();

    // Cyberpunk seems to be sending LH so do the same
    // it also sends some extra data in usually empty spots but no idea what that is
    if (_cameraNear[fIndex] > 0.f && _cameraFar[fIndex] > 0.f &&
        !XMScalarNearEqual(_cameraVFov[fIndex], 0.0f, 0.00001f) &&
        !XMScalarNearEqual(_cameraAspectRatio[fIndex], 0.0f, 0.00001f))
    {
        if (XMScalarNearEqual(_cameraNear[fIndex], _cameraFar[fIndex], 0.00001f))
            _cameraFar[fIndex]++;

        auto projectionMatrix = XMMatrixPerspectiveFovLH(_cameraVFov[fIndex], _cameraAspectRatio[fIndex],
                                                         _cameraNear[fIndex], _cameraFar[fIndex]);
        memcpy(constData.projectionMatrix, projectionMatrix.r, sizeof(projectionMatrix));
    }
    else
    {
        LOG_WARN("Can't calculate projectionMatrix");
    }

    constData.jitterOffsetX = _jitterX[fIndex];
    constData.jitterOffsetY = _jitterY[fIndex];
    constData.motionVectorScaleX = _mvScaleX[fIndex];
    constData.motionVectorScaleY = _mvScaleY[fIndex];

    if (!Config::Instance()->FGSkipReset.value_or_default())
    {
        // Game signal OR (fresh activation / detected camera cut when AutoReset
        // is on). All three only affect the interpolated content of this burst
        // - present count, waits and resolution are untouched.
        constData.resetHistory = (_reset[fIndex] != 0) || (autoReset && (_forceResetNext || cameraCut));
    }
    else
    {
        constData.resetHistory = false;
    }

    _forceResetNext = false;

    switch (Config::Instance()->FTInput.value_or_default())
    {
    case FrameTimeSource::Input:
        // Ask the pacing first: _ftDelta is filled with lastFGFrameTime on this backend
        // (Upscaler_Inputs_Dx12), i.e. the same self-referential present-to-present number
        // under another name. Tried first it always wins and RenderTimeMs() is never reached.
        // (Ported from Coldwood1026/OptiScalerDp4aUnlock c0ec7979)
        constData.frameRenderTime = static_cast<float>(XeFGPacing::RenderTimeMs());
        if (!(constData.frameRenderTime > 0.0f))
        {
            constData.frameRenderTime = (float) _ftDelta[fIndex];
        }
        else if (_lastFedFrameTimeMs > 0.0f && constData.frameRenderTime > _lastFedFrameTimeMs)
        {
            // Slew-limit upward steps: the measured period contains this
            // burst's own pacing block, so feeding it back raw compounds
            // burst-over-burst into a high-latency fixed point. Falls stay
            // immediate; genuine scene load still converges within a few
            // bursts (+15% / +0.25 ms per burst).
            const float riseCap = _lastFedFrameTimeMs * 1.15f + 0.25f;

            if (constData.frameRenderTime > riseCap)
                constData.frameRenderTime = riseCap;
        }

        _lastFedFrameTimeMs = constData.frameRenderTime;
        break;

    case FrameTimeSource::Opti:
        constData.frameRenderTime = static_cast<float>(state.lastFGFrameTime);
        break;

    case FrameTimeSource::Zero:
        constData.frameRenderTime = 0.0f;
        break;
    }

    XeFGPacing::NoteFedFrameTime(constData.frameRenderTime);

    LOG_DEBUG_ONLY("Reset: {}, Opti FT: {}, Source FT: {}, Set FT: {}, Opti Id: {}, Reflex Id: {}", _reset[fIndex],
                   constData.frameRenderTime, _ftDelta[fIndex], constData.frameRenderTime, _frameCount,
                   State::Instance().reflexFrameId);

    auto frameId = static_cast<uint32_t>(willDispatchFrame);

    auto result = XeFGProxy::TagFrameConstants()(_swapChainContext, frameId, &constData);
    if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
    {
        // Single-frame tag miss: skip this burst only. Deactivating here
        // costs 10 paused frames and tanks 1% low for a transient miss.
        LOG_ERROR("TagFrameConstants error: {} ({})", magic_enum::enum_name(result), (UINT) result);

        return false;
    }

    result = XeFGProxy::SetPresentId()(_swapChainContext, frameId);
    if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
    {
        // Same as above: skip the burst, stay active.
        LOG_ERROR("SetPresentId error: {} ({})", magic_enum::enum_name(result), (UINT) result);

        return false;
    }

    // When using Hudfix, we always copy hudless as swapchain size
    if (state.activeFgInput != FGInput::Upscaler)
    {
        uint32_t left = 0;
        uint32_t top = 0;

        if (_interpolationWidth[fIndex] == 0 && _interpolationHeight[fIndex] == 0)
        {
            LOG_WARN("Interpolation size is 0, using swapchain size");
            _interpolationWidth[fIndex] = state.currentSwapchainDesc.BufferDesc.Width;
            _interpolationHeight[fIndex] = state.currentSwapchainDesc.BufferDesc.Height;
        }
        else
        {
            auto calculatedLeft =
                ((int) state.currentSwapchainDesc.BufferDesc.Width - (int) _interpolationWidth[fIndex]) / 2;
            if (calculatedLeft > 0)
                left = Config::Instance()->FGRectLeft.value_or(_interpolationLeft[fIndex].value_or(calculatedLeft));

            auto calculatedTop =
                ((int) state.currentSwapchainDesc.BufferDesc.Height - (int) _interpolationHeight[fIndex]) / 2;
            if (calculatedTop > 0)
                top = Config::Instance()->FGRectTop.value_or(_interpolationTop[fIndex].value_or(calculatedTop));
        }

        LOG_DEBUG_ONLY("SwapChain Res: {}x{}, Interpolation Res: {}x{}", state.currentSwapchainDesc.BufferDesc.Width,
                       state.currentSwapchainDesc.BufferDesc.Height, _interpolationWidth[fIndex],
                       _interpolationHeight[fIndex]);

        xefg_swapchain_d3d12_resource_data_t backbuffer = {};
        backbuffer.type = XEFG_SWAPCHAIN_RES_BACKBUFFER;
        backbuffer.validity = XEFG_SWAPCHAIN_RV_UNTIL_NEXT_PRESENT;
        backbuffer.resourceBase = { (UINT) Config::Instance()->FGRectLeft.value_or(left),
                                    (UINT) Config::Instance()->FGRectTop.value_or(top) };
        backbuffer.resourceSize = { static_cast<uint32_t>(
                                        Config::Instance()->FGRectWidth.value_or(_interpolationWidth[fIndex])),
                                    (UINT) Config::Instance()->FGRectHeight.value_or(_interpolationHeight[fIndex]) };

        result = XeFGProxy::D3D12TagFrameResource()(_swapChainContext, (ID3D12CommandList*) 1, frameId, &backbuffer);

        if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
        {
            // Single-frame backbuffer miss: skip this burst only, same as above.
            LOG_ERROR("D3D12TagFrameResource Backbuffer error: {} ({})", magic_enum::enum_name(result), (UINT) result);

            return false;
        }
    }

    LOG_DEBUG_ONLY("Result: Ok");

    return true;
}

void* XeFG_Dx12::FrameGenerationContext() { return _fgContext; }

void* XeFG_Dx12::SwapchainContext() { return _swapChainContext; }

XeFG_Dx12::~XeFG_Dx12() { Shutdown(); }

bool XeFG_Dx12::SetInterpolatedFrameCount(UINT interpolatedFrameCount) { return true; }

void XeFG_Dx12::EvaluateState(ID3D12Device* device, FG_Constants& fgConstants)
{
    LOG_FUNC();

    OwnedLockGuard lock(Mutex, 555);

    auto& state = State::Instance();

    // If needed hooks are missing or XeFG proxy is not inited or FG swapchain is not created
    if (!XeFGProxy::InitXeFG() || state.currentFGSwapchain == nullptr)
        return;

    if (state.isShuttingDown)
    {
        DestroyFGContext();
        return;
    }

    _infiniteDepth = static_cast<bool>(fgConstants.flags & FG_Flags::InfiniteDepth);

    // If FG Enabled from menu
    if (Config::Instance()->FGEnabled.value_or_default())
    {
        // If FG context is nullptr
        if (_fgContext == nullptr)
        {
            // Create it again
            CreateContext(device, fgConstants);

            // Pause for 10 frames
            UpdateTarget();
        }
        // If there is a change deactivate it
        else if (state.fgChanged)
        {
            LOG_DEBUG("FGChanged");
            Deactivate();

            // Pause for 10 frames
            UpdateTarget();

            // Destroy if Swapchain has a change destroy FG Context too
            if (state.scChanged)
                DestroyFGContext();
        }

        if (_fgContext != nullptr && State::Instance().activeFgInput == FGInput::Upscaler && !IsPaused() && !IsActive())
            Activate();
    }
    else
    {
        LOG_DEBUG("!FGEnabled");
        Deactivate();

        state.clearCapturedHudlesses = true;
        Hudfix_Dx12::ResetCounters();
        Hudfix_Dx11::ResetCounters();
    }

    if (state.fgChanged)
    {
        LOG_DEBUG("FGchanged");

        state.fgChanged = false;

        Hudfix_Dx12::ResetCounters();
        Hudfix_Dx11::ResetCounters();

        // Pause for 10 frames
        UpdateTarget();

        // Release FG mutex
        if (Mutex.getOwner() == 2)
            Mutex.unlockThis(2);
    }

    state.scChanged = false;
}

void XeFG_Dx12::ReleaseObjects()
{
    for (size_t i = 0; i < BUFFER_COUNT; i++)
    {
        SAFE_RELEASE(_uiCommandAllocator[i]);
        SAFE_RELEASE(_uiCommandList[i]);
        SAFE_RELEASE(_scCommandAllocator[i]);
        SAFE_RELEASE(_scCommandList[i]);

        // Reset command list state
        _scCommandListResetted[i] = false;
        _scAllocatorFenceValues[i] = 0;

        _uiCommandListResetted[i] = false;
        _uiAllocatorFenceValues[i] = 0;
    }

    _renderUI.reset();
    _hudlessCompare.reset();
    _mvFlip.reset();
    _depthFlip.reset();
    _depthInvert.reset();
}

void XeFG_Dx12::CreateObjects(ID3D12Device* InDevice)
{
    _device = InDevice;

    if (_uiCommandAllocator[0] != nullptr)
        return;

    LOG_DEBUG("");

    do
    {
        HRESULT result;
        ID3D12CommandAllocator* allocator = nullptr;
        ID3D12GraphicsCommandList* cmdList = nullptr;
        ID3D12CommandQueue* cmdQueue = nullptr;

        // FG
        for (size_t i = 0; i < BUFFER_COUNT; i++)
        {
            // Reset command list state
            _scCommandListResetted[i] = false;
            _scAllocatorFenceValues[i] = 0;

            _uiCommandListResetted[i] = false;
            _uiAllocatorFenceValues[i] = 0;

            result =
                InDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_uiCommandAllocator[i]));
            if (result != S_OK)
            {
                LOG_ERROR("CreateCommandAllocators _uiCommandAllocator[{}]: {:X}", i, (unsigned long) result);
                break;
            }

            _uiCommandAllocator[i]->SetName(std::format(L"_uiCommandAllocator[{}]", i).c_str());
            if (CheckForRealObject(__FUNCTION__, _uiCommandAllocator[i], (IUnknown**) &allocator))
                _uiCommandAllocator[i] = allocator;

            result = InDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _uiCommandAllocator[i], NULL,
                                                 IID_PPV_ARGS(&_uiCommandList[i]));
            if (result != S_OK)
            {
                LOG_ERROR("CreateCommandList _hudlessCommandList[{}]: {:X}", i, (unsigned long) result);
                break;
            }
            _uiCommandList[i]->SetName(std::format(L"_uiCommandList[{}]", i).c_str());
            if (CheckForRealObject(__FUNCTION__, _uiCommandList[i], (IUnknown**) &cmdList))
                _uiCommandList[i] = cmdList;

            result = _uiCommandList[i]->Close();
            if (result != S_OK)
            {
                LOG_ERROR("_uiCommandList[{}]->Close: {:X}", i, (unsigned long) result);
                break;
            }

            if (_uiFence == nullptr)
            {
                result = InDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_uiFence));
                if (FAILED(result))
                {
                    LOG_ERROR("Create UI fence failed: {:X}", (UINT) result);
                    break;
                }
            }

            if (_uiFenceEvent == nullptr)
            {
                _uiFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
                if (_uiFenceEvent == nullptr)
                {
                    LOG_ERROR("CreateEvent for UI fence failed");
                    break;
                }
            }

            result =
                InDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_scCommandAllocator[i]));
            if (result != S_OK)
            {
                LOG_ERROR("CreateCommandAllocators _scCommandAllocator[{}]: {:X}", i, (unsigned long) result);
                break;
            }

            _scCommandAllocator[i]->SetName(std::format(L"_scCommandAllocator[{}]", i).c_str());
            if (CheckForRealObject(__FUNCTION__, _scCommandAllocator[i], (IUnknown**) &allocator))
                _scCommandAllocator[i] = allocator;

            result = InDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _scCommandAllocator[i], NULL,
                                                 IID_PPV_ARGS(&_scCommandList[i]));
            if (result != S_OK)
            {
                LOG_ERROR("CreateCommandList _hudlessCommandList[{}]: {:X}", i, (unsigned long) result);
                break;
            }
            _scCommandList[i]->SetName(std::format(L"_scCommandList[{}]", i).c_str());
            if (CheckForRealObject(__FUNCTION__, _scCommandList[i], (IUnknown**) &cmdList))
                _scCommandList[i] = cmdList;

            result = _scCommandList[i]->Close();
            if (result != S_OK)
            {
                LOG_ERROR("_scCommandList[{}]->Close: {:X}", i, (unsigned long) result);
                break;
            }

            if (_scFence == nullptr)
            {
                result = InDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_scFence));
                if (FAILED(result))
                {
                    LOG_ERROR("Create SC fence failed: {:X}", (UINT) result);
                    break;
                }
            }

            if (_scFenceEvent == nullptr)
            {
                _scFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
                if (_scFenceEvent == nullptr)
                {
                    LOG_ERROR("CreateEvent for SC fence failed");
                    break;
                }
            }
        }

    } while (false);
}

bool XeFG_Dx12::Present()
{
    auto fIndex = GetIndexWillBeDispatched();
    LOG_DEBUG_ONLY("fIndex: {}", fIndex);

    if (Config::Instance()->FGDrawUIOverFG.value_or_default())
    {
        auto ui = GetResource(FG_ResourceType::UIColor, fIndex);
        if (ui && (ui->validity == FG_ResourceValidity::UntilPresent ||
                   ui->validity == FG_ResourceValidity::UntilPresentFromDispatch))
        {
            LOG_DEBUG_ONLY("UI[{}] resource: {:X}, copy: {}", fIndex, (size_t) ui->resource, (size_t) ui->copy);
            if (_renderUI.get() == nullptr)
            {
                _renderUI = std::make_unique<RUI_Dx12>("RenderUI", _device,
                                                       Config::Instance()->FGUIPremultipliedAlpha.value_or_default());
            }
            else
            {
                if (Config::Instance()->FGUIPremultipliedAlpha.value_or_default() != _renderUI->IsPreMultipliedAlpha())
                {
                    LOG_INFO("UI premultiplied alpha changed, recreating RenderUI");
                    _renderUI = std::make_unique<RUI_Dx12>(
                        "RenderUI", _device, Config::Instance()->FGUIPremultipliedAlpha.value_or_default());
                }
                else if (_renderUI->IsInit())
                {
                    auto commandList = GetSCCommandList(fIndex);
                    _renderUI->Dispatch((IDXGISwapChain3*) _swapChain, commandList, ui->GetResource(), ui->state);
                }
            }
        }
        else if (!ui)
        {
            LOG_WARN("UI resource is nullptr");
        }
    }

    if (IsActive() && !IsPaused())
    {
        if (State::Instance().fgHudlessCompare)
        {
            auto hudless = GetResource(FG_ResourceType::HudlessColor, fIndex);
            if (hudless && (hudless->validity == FG_ResourceValidity::UntilPresent ||
                            hudless->validity == FG_ResourceValidity::UntilPresentFromDispatch))
            {
                LOG_DEBUG_ONLY("Hudless[{}] resource: {:X}, copy: {}", fIndex, (size_t) hudless->resource,
                               (size_t) hudless->copy);
                if (_hudlessCompare.get() == nullptr)
                {
                    _hudlessCompare = std::make_unique<HC_Dx12>("HudlessCompare", _device);
                }
                else
                {
                    if (_hudlessCompare->IsInit())
                    {
                        auto commandList = GetSCCommandList(fIndex);
                        _hudlessCompare->Dispatch((IDXGISwapChain3*) _swapChain, commandList, hudless->GetResource(),
                                                  hudless->state);
                    }
                }
            }
            else if (!hudless)
            {
                LOG_WARN("Hudless resource is nullptr");
            }
        }
    }

    bool result = false;

    // if (IsActive() && !IsPaused())
    {
        if (_uiCommandListResetted[fIndex])
        {
            LOG_DEBUG_ONLY("Executing _uiCommandList[{}]: {:X}", fIndex, (size_t) _uiCommandList[fIndex]);
            auto closeResult = _uiCommandList[fIndex]->Close();

            if (closeResult == S_OK)
                _gameCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList**) &_uiCommandList[fIndex]);
            else
                LOG_ERROR("_uiCommandList[{}]->Close() error: {:X}", fIndex, (UINT) closeResult);

            _gameCommandQueue->Signal(_uiFence, _uiAllocatorFenceValues[fIndex]);

            _uiCommandListResetted[fIndex] = false;
        }

        if (_scCommandListResetted[fIndex])
        {
            LOG_DEBUG_ONLY("Executing _scCommandList[{}]: {:X}", fIndex, (size_t) _scCommandList[fIndex]);
            auto closeResult = _scCommandList[fIndex]->Close();

            if (closeResult == S_OK)
                _gameCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList**) &_scCommandList[fIndex]);
            else
                LOG_ERROR("_scCommandList[{}]->Close() error: {:X}", fIndex, (UINT) closeResult);

            _scCommandListResetted[fIndex] = false;
        }
    }

    if ((_fgFramePresentId - _lastFGFramePresentId) > 3 && IsActive() && !_waitingNewFrameData)
    {
        // Hysteresis: a single hitch skews present IDs without meaning the
        // game stopped feeding frames. Pause only after consecutive starved
        // Presents (~9+ without a NewFrame) instead of on the first trip.
        if (++_presentStarveCount >= 3)
        {
            LOG_DEBUG("Pausing FG");
            Deactivate();
            _waitingNewFrameData = true;
            _presentStarveCount = 0;
            return false;
        }
    }
    else
    {
        _presentStarveCount = 0;
    }

    _fgFramePresentId++;

    return Dispatch();
}

bool XeFG_Dx12::SetResource(Dx12Resource* inputResource)
{
    if (inputResource == nullptr || inputResource->resource == nullptr ||
        (inputResource->type != FG_ResourceType::UIColor && (!IsActive() || IsPaused())))
    {
        return false;
    }

    // For late sent SL resources
    // we use provided frame index
    auto fIndex = inputResource->frameIndex;
    if (fIndex < 0)
        fIndex = GetIndex();

    auto& type = inputResource->type;

    std::unique_lock<std::shared_mutex> lock(_resourceMutex[fIndex]);

    // This is mostly useful for cases where the user has manually set validity as ValidNow
    if (!inputResource->cmdList && inputResource->validity != FG_ResourceValidity::UntilPresent &&
        inputResource->validity != FG_ResourceValidity::UntilPresentFromDispatch)
    {
        LOG_WARN("XeFG needs cmdList for ValidNow resources, YOLOing");
        inputResource->validity = FG_ResourceValidity::UntilPresent;
    }

    if (type == FG_ResourceType::HudlessColor)
    {
        if (Config::Instance()->FGDisableHudless.value_or_default())
            return false;

        // Making a copy if it's just valid now to be able to use it later
        if (State::Instance().fgHudlessCompare && inputResource->validity == FG_ResourceValidity::ValidNow)
            inputResource->validity = FG_ResourceValidity::ValidButMakeCopy;

        if (!_noHudless[fIndex] && (_frameResources[fIndex][type].validity == FG_ResourceValidity::ValidNow))
        {
            return false;
        }

        if (!_noHudless[fIndex] && Config::Instance()->FGOnlyAcceptFirstHudless.value_or_default() &&
            inputResource->validity != FG_ResourceValidity::UntilPresentFromDispatch)
        {
            return false;
        }
    }

    if (type == FG_ResourceType::UIColor)
    {
        if (Config::Instance()->FGDisableUI.value_or_default())
            return false;

        // Making a copy if it's just valid now
        if (Config::Instance()->FGDrawUIOverFG.value_or_default() &&
            inputResource->validity == FG_ResourceValidity::ValidNow)
        {
            inputResource->validity = FG_ResourceValidity::ValidButMakeCopy;
        }

        if (!_noUi[fIndex] && (_frameResources[fIndex][type].validity == FG_ResourceValidity::ValidNow))
        {
            return false;
        }
    }

    if (type == FG_ResourceType::Distortion)
    {
        if (!_noDistortionField[fIndex] && (_frameResources[fIndex][type].validity == FG_ResourceValidity::ValidNow))
        {
            return false;
        }
    }

    if ((type == FG_ResourceType::Depth || type == FG_ResourceType::Velocity) && _frameResources[fIndex].contains(type))
    {
        return false;
    }

    if (inputResource->cmdList == nullptr && inputResource->validity == FG_ResourceValidity::ValidNow)
    {
        LOG_ERROR("{}, validity == ValidNow but cmdList is nullptr!", magic_enum::enum_name(type));
        return false;
    }

    if (type == FG_ResourceType::Distortion)
    {
        LOG_TRACE("Distortion field is not supported by XeFG");
        return false;
    }

    auto fResource = &_frameResources[fIndex][type];
    fResource->type = type;
    fResource->state = inputResource->state;
    fResource->validity = inputResource->validity;
    fResource->resource = inputResource->resource;
    fResource->top = inputResource->top;
    fResource->left = inputResource->left;
    fResource->width = inputResource->width;
    fResource->height = inputResource->height;
    fResource->cmdList = inputResource->cmdList;

    auto willFlip = State::Instance().activeFgInput == FGInput::Upscaler &&
                    Config::Instance()->FGResourceFlip.value_or_default() &&
                    (type == FG_ResourceType::Velocity || type == FG_ResourceType::Depth);

    // Resource flipping
    if (willFlip && _device != nullptr)
        FlipResource(fResource);

    // Depth Invert
    // https://github.com/intel/xess/issues/50
    static auto version = Version();

    // SDK version 2.1.1 fixed this
    if (version < feature_version { 1, 2, 2 } && _device != nullptr && type == FG_ResourceType::Depth &&
        !Config::Instance()->FGXeFGDepthInverted.value_or_default())
    {
        if (_depthInvert.get() == nullptr)
        {
            _depthInvert = std::make_unique<DI_Dx12>("DepthInvert", _device);
        }
        else if (_depthInvert->IsInit())
        {
            if (_depthInvert->CreateBufferResource(_device, fResource->GetResource(), fResource->width,
                                                   fResource->height, fResource->state) &&
                _depthInvert->Buffer() != nullptr)
            {
                auto cmdList = (fResource->cmdList != nullptr) ? fResource->cmdList : GetUICommandList(fIndex);

                _depthInvert->SetBufferState(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

                if (_depthInvert->Dispatch(cmdList, fResource->GetResource(), _depthInvert->Buffer()))
                {
                    fResource->copy = _depthInvert->Buffer();
                }

                _depthInvert->SetBufferState(cmdList, fResource->state);
            }
        }
    }

    // We usually don't copy any resources for XeFG, the ones with this tag are the exception
    if (inputResource->cmdList != nullptr && fResource->validity == FG_ResourceValidity::ValidButMakeCopy)
    {
        LOG_DEBUG_ONLY("Making a resource copy of: {}", magic_enum::enum_name(type));

        ID3D12Resource* copyOutput = nullptr;

        if (_resourceCopy[fIndex].contains(type))
            copyOutput = _resourceCopy[fIndex][type];

        ID3D12Resource* beforeCopy = copyOutput;

        if (!CopyResource(inputResource->cmdList, inputResource->resource, &copyOutput, inputResource->state))
        {
            LOG_ERROR("{}, CopyResource error!", magic_enum::enum_name(type));
            return false;
        }

        _resourceCopy[fIndex][type] = copyOutput;

        // Name once at creation: SetName is a kernel call plus string formatting,
        // pointless to redo every frame for a reused buffer (also covers desc-mismatch realloc).
        if (copyOutput != beforeCopy)
            copyOutput->SetName(std::format(L"_resourceCopy[{}][{}]", fIndex, (UINT) type).c_str());
        fResource->copy = copyOutput;
        fResource->state = D3D12_RESOURCE_STATE_COPY_DEST;

        fResource->validity = FG_ResourceValidity::UntilPresent;
    }

    if (type == FG_ResourceType::UIColor)
        _noUi[fIndex] = false;
    else if (type == FG_ResourceType::Distortion)
        _noDistortionField[fIndex] = false;
    else if (type == FG_ResourceType::HudlessColor)
        _noHudless[fIndex] = false;

    if ((type == FG_ResourceType::Depth || type == FG_ResourceType::Velocity) ||
        fResource->validity != FG_ResourceValidity::UntilPresent)
    {
        fResource->validity = (fResource->validity != FG_ResourceValidity::ValidNow || willFlip)
                                  ? FG_ResourceValidity::UntilPresent
                                  : FG_ResourceValidity::ValidNow;

        if (type == FG_ResourceType::HudlessColor)
        {
            static DXGI_FORMAT lastFormat[BUFFER_COUNT] = {};
            auto desc = fResource->GetResource()->GetDesc();

            if (lastFormat[fIndex] != DXGI_FORMAT_UNKNOWN && lastFormat[fIndex] != desc.Format)
            {
                State::Instance().fgChanged = true;
                return false;
            }

            lastFormat[fIndex] = desc.Format;
        }

        xefg_swapchain_d3d12_resource_data_t resourceParam = GetResourceData(type, fIndex);

        // SDK version 2.1.1 fixes those issues
        if (version < feature_version { 1, 2, 2 })
        {
            // HACK: XeFG docs lie and cmd list is technically required as it checks for it
            // But it doesn't seem to use it when the validity is UNTIL_NEXT_PRESENT
            // https://github.com/intel/xess/issues/45
            if (fResource->cmdList == nullptr && resourceParam.validity == XEFG_SWAPCHAIN_RV_UNTIL_NEXT_PRESENT)
                fResource->cmdList = (ID3D12GraphicsCommandList*) 1;

            // HACK: XeFG seems to crash if the resource is in COPY_SOURCE state
            // even though the docs say it's the preferred state
            // https://github.com/intel/xess/issues/47
            if (inputResource->state == D3D12_RESOURCE_STATE_COPY_SOURCE)
            {
                ResourceBarrier(inputResource->cmdList, inputResource->resource, inputResource->state,
                                D3D12_RESOURCE_STATE_COPY_DEST);

                resourceParam.incomingState = D3D12_RESOURCE_STATE_COPY_DEST;
            }
        }

        int indexDiff = GetIndex() - fIndex;
        if (indexDiff < 0)
            indexDiff += BUFFER_COUNT;

        // We will us UI color later with Render UI
        if (type != FG_ResourceType::UIColor ||
            (XeFGProxy::SetUiCompositionState() != nullptr || Config::Instance()->FGDrawUIOverFG.value_or_default()))
        {
            auto frameId = static_cast<uint32_t>(_frameCount - indexDiff);
            auto result =
                XeFGProxy::D3D12TagFrameResource()(_swapChainContext, fResource->cmdList, frameId, &resourceParam);
            LOG_DEBUG_ONLY("D3D12TagFrameResource, frameId: {}, type: {} result: {} ({})", frameId,
                           magic_enum::enum_name(type), magic_enum::enum_name(result), (int32_t) result);

            if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
            {
                State::Instance().fgChanged = true;
                UpdateTarget();
                Deactivate();

                return false;
            }
        }

        // Potentially we don't need to restore but do it just to be safe
        if (inputResource->state == D3D12_RESOURCE_STATE_COPY_SOURCE)
        {
            ResourceBarrier(inputResource->cmdList, inputResource->resource, D3D12_RESOURCE_STATE_COPY_DEST,
                            inputResource->state);
        }

        SetResourceReady(type, fIndex);
    }

    LOG_TRACE("_frameResources[{}][{}]: {:X}", fIndex, magic_enum::enum_name(type), (size_t) fResource->GetResource());

    return true;
}

void XeFG_Dx12::SetCommandQueue(FG_ResourceType type, ID3D12CommandQueue* queue) { _gameCommandQueue = queue; }

bool XeFG_Dx12::ReleaseSwapchain(HWND hwnd)
{
    if (hwnd != _hwnd || _hwnd == NULL)
        return false;

    LOG_DEBUG("");

    if (Config::Instance()->FGUseMutexForSwapchain.value_or_default())
    {
        if (Mutex.getOwner() == 1)
        {
            LOG_WARN("Skipping Mutex we are already in ReleaseSwapchain");
            return true;
        }

        LOG_TRACE("Waiting Mutex 1, current: {}", Mutex.getOwner());
        Mutex.lock(1);
        LOG_TRACE("Accuired Mutex: {}", Mutex.getOwner());
    }

    MenuOverlayDx::CleanupRenderTarget(true, NULL);

    if (_fgContext != nullptr)
        DestroyFGContext();

    if (!State::Instance().isShuttingDown)
    {
        if (_swapChainContext != nullptr)
            DestroySwapchainContext();

        _swapChainContext = nullptr;
        State::Instance().currentFGSwapchain = nullptr;
    }

    ReleaseObjects();

    if (Config::Instance()->FGUseMutexForSwapchain.value_or_default())
    {
        LOG_TRACE("Releasing Mutex: {}", Mutex.getOwner());
        Mutex.unlockThis(1);
    }

    return true;
}
