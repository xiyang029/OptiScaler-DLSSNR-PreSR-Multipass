#include "pch.h"
#include <dlssnr/DlssNr_MenuOverlay.h>
#include "menu_common.h"
#if defined(OPTISCALER_RTX40_MFG)
#include <framegen/dlssg/MfgUnlock.h>
#endif

#include <algorithm>
#include <cfloat>

#include <dlssnr/DlssNr.h>

#include "input/input_system.h"

#include "font/Hack_Compressed.h"

#include <proxies/XeSS_Proxy.h>
#include <proxies/XeFG_Proxy.h>
#include <proxies/FfxApi_Proxy.h>
#include <proxies/Streamline_Proxy.h>

#include <framegen/nvngx/Nvngx_FG.h>

#include <nvapi/fakenvapi.h>
#include <hooks/Reflex_Hooks.h>

#include <BuildInfo.h>
#include <version_check.h>

#include <upscaler_time/UpscalerTime_Vk.h>

#include <imgui/imgui_internal.h>
#include <imgui/ImGuiNotify.hpp>
#include <imgui/imgui_impl_win32.h>
#include <imgui/imgui_impl_uwp.h>

#include <mutex>
#include <cstdarg>

#include <array>
#include <chrono>
#include <memory>
#include <type_traits>
#include <misc/IdentifyGpu.h>
#include <hooks/Xell_Hooks.h>
#include <low_latency/input/input_common.h>

enum class UiTargetMode
{
    SDR,
    LinearHDR,
    ScRGB,
    PQ,
    HLG
};

#define MARK_ALL_BACKENDS_CHANGED()                                                                                    \
    for (auto& singleChangeBackend : State::Instance().changeBackend)                                                  \
        singleChangeBackend.second = true;

static float fontSize = 14.0f; // just changing this doesn't make other elements scale ideally
static ImVec2 overlaySize(0.0f, 0.0f);
static ImVec2 overlayPosition(-1000.0f, -1000.0f);
static bool _hdrTonemapApplied = false;
static ImVec4 SdrColors[ImGuiCol_COUNT];

static bool inputMenu = false;
static bool inputFG = false;
static bool inputFps = false;
static bool inputFpsCycle = false;
static uint64_t lastInputTick = 0;
constexpr uint64_t debounceThreshold = 1000;

static bool hasGamepad = false;
static bool ffxInitTried = false;
static bool xefgInitTried = false;
static std::string windowTitle;
static std::string selectedUpscalerName = "";
static Upscaler currentBackend = Upscaler::Reset;
static std::string currentBackendName = "";
static int refreshRate = 0;
static ImVec2 lastPosition(-1000.0f, -1000.0f);

static ImVec2 splashPosition(-1000.0f, -1000.0f);
static ImVec2 splashSize(0.0f, 0.0f);
static double splashStart = 0.0;
static double splashLimit = 0.0;
static std::vector<std::string> splashText = { "Cope smarter, not harder",
                                               "Coping is strong with this one...",
                                               "This is where the fun begins...",
                                               "Got any more of them scalers?...",
                                               "Fake pixels and even faker frames...",
                                               "Fake frames, get your fake frames...",
                                               "I'm here to kick pixels and chew frames...",
                                               "I find your lack of supersampling disturbing...",
                                               "Frame by frame, I scale-up!",
                                               "Resistance is futile. Your pixels will be upscaled.",
                                               "I've got 99 problems, but low-res ain't one.",
                                               "It's over, DLSS, I have the higher ground!",
                                               "This isn't the resolution you're looking for",
                                               "To infinity and beyond... with ray tracing off",
                                               "I have a bad feeling about this frame pacing",
                                               "It's Dangerous to Go Alone-Take This Upscaler",
                                               "Upscaled beyond recognition.",
                                               "Trust the process. Ignore the shimmer.",
                                               "Real fake frames. Certified.",
                                               "The illusion of performance",
                                               "This upscaler belongs in a museum!",
                                               "Because native rendering is overrated.",
                                               "The more you upscaler, the more you save",
                                               "It's never too late to buy a better GPU",
                                               "We don't need real pixels where we're going",
                                               "Did you know that Intel released XeFG for everyone?",
                                               "MFG totally works with Nukem's 100%% no scam",
                                               "Some of those pixels might even be real!",
                                               "Just don't look too closely at the image",
                                               "Even supports \"software\" XeSS!",
                                               "It's too blurry to go alone, take RCAS with you",
                                               "Thanks nitec, back to you nitec",
                                               "Tested and approved by By-U",
                                               "0.8 was an inside job",
                                               "FSR4 DP4a wenETA, AMD plz",
                                               "OptiCopers, assemble!",
                                               "The Way It's Meant To Be Upscaled",
                                               "Your game may not even crash today",
                                               "Expanded and Enhanced",
                                               "It's only my 5th crash today",
                                               "Latency with FG? But I have good internet",
                                               "Console peasants can't do that",
                                               "Hope you don't have a good eyesight",
                                               "Such an aggressive upscaling? A bold move",
                                               "I almost don't feel the input lag",
                                               "And that's how you get to 60 FPS",
                                               "Together We Upscale",
                                               "For upscalers, by upscalers",
                                               "Opti Sports, it's in the sampling",
                                               "Render in your world. Upscale in ours",
                                               "All your pixels are belong to us",
                                               "Upscaling for the masses, not the classes",
                                               "Generating discord since 2023",
                                               "Enabling DLSS since 2023",
                                               "[REDACTED] never looked better",
                                               "Free and always free",
                                               "Getting unshackled from green chains in progress...",
                                               "Who's Nukem anyway?",
                                               "Compiling shaders... ETA: 05h:49m",
                                               "Did you really just pay 70 EUR for this game?!",
                                               "Guess who forgot about a nullptr check again",
                                               "AI can't outslop this",
                                               "Guess we're pre-alpha build demos now",
                                               "New app on the block - TH",
                                               "One more stutter and I might lose it",
                                               "Mostly stable, unlike the driver",
                                               "Vul... what? ~AMD",
                                               "My 8 points are floating",
                                               "No floating here - I'm strictly between -128 and 127",
                                               "Fake it til you bake it",
                                               "Worst case just turn it off and on",
                                               "*On a generative damage control mode at geometry level*",
                                               "Deep Learning Slop Sampling 5",
                                               "2D AI filters, now powered by just 2x 5090s",
                                               "Neural Slop Sampling with DLSS5",
                                               "DLSS 5 - the way it's meant to be slopped",
                                               "Just when I think I'm out, they scale me back in",
                                               "Like going in the first gear on the highway",
                                               "Nitec's Bizarre Upscaling",
                                               "\"Framegen really attracts some strange clientelle\"",
                                               "How to remove those corny messages?!",
                                               "<Your funny text goes here>" };

static std::string updateNoticeTag;
static std::string updateNoticeUrl;
static float lastMenuScale = 0.0f;
static CustomOptional<uint32_t> comboPreset { 0 };
static int lastKey = 0;
static bool inputDlssNr = false;
static bool capturingKey = false;

template <typename T, size_t N> struct RingBuffer
{
    std::array<T, N> data {};
    size_t head { 0 };
    size_t count { N };
    double sum { 0.0 };

    RingBuffer() { data.fill(static_cast<T>(0)); }

    void Push(T v)
    {
        if (count == N)
        {
            sum -= data[head];
        }
        else
        {
            ++count;
        }
        data[head] = v;
        sum += v;
        head = (head + 1) % N;
    }

    size_t Size() const { return N; }

    T At(size_t i) const
    {
        size_t start = head;
        return data[(start + i) % N];
    }

    float Average() const { return static_cast<float>(sum / static_cast<double>(N)); }
};

const int plotWidth = 360;
static RingBuffer<float, plotWidth> gFrameTimes;
static RingBuffer<float, plotWidth> gUpscalerTimes;

struct FsExistsCache
{
    std::wstring lastPath;
    bool cached { false };
    std::chrono::steady_clock::time_point nextRefresh { std::chrono::steady_clock::time_point::min() };
    std::chrono::milliseconds interval { 2000 };

    bool Get(const std::filesystem::path& path)
    {
        auto now = std::chrono::steady_clock::now();
        if (path != lastPath || now >= nextRefresh)
        {
            lastPath = path;
            cached = std::filesystem::exists(path);
            nextRefresh = now + interval;
        }
        return cached;
    }
};

static FsExistsCache nukemsExists;
static FsExistsCache enablerExists;

struct FlagDefinition
{
    std::string name;
    uint32_t mask;
    std::string description;
};

inline std::string StrFmt(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len = std::vsnprintf(nullptr, 0, fmt, args);
    va_end(args);
    std::string out(len, '\0');
    va_start(args, fmt);
    std::vsnprintf(out.data(), len + 1, fmt, args);
    va_end(args);
    return out;
}

void MenuCommon::UpdateManualInput(HWND targetHwnd)
{
    OptiInput::BeginFrame(targetHwnd);

    const auto config = Config::Instance();

    auto CheckShortcut =
        [&](int vk, bool& inputFlag, const char* logMessage, bool requireCtrl = false, bool requireAlt = false)
    {
        if (inputFlag)
            return;

        if (vk <= 0 || vk >= 256)
            return;

        // Checked before the release edge below, not folded into it: modifiers must still be held
        // at the moment the trigger key is released, the same convention every OS shortcut chord
        // uses (release the letter while the modifiers are down, not "were down at some point").
        //
        // Checks the generic code and both L/R-specific ones: raw keyboard input
        // (NormalizeRawKeyboardVirtualKey, input_system_raw.cpp) rewrites VK_CONTROL/VK_MENU into
        // VK_LCONTROL/VK_RCONTROL/VK_LMENU/VK_RMENU before this table is ever touched, so the
        // plain generic code alone would never read as down on that path - checking only it would
        // make this feature silently never fire depending on which input path is active.
        if (requireCtrl && !OptiInput::IsKeyDown(VK_CONTROL) && !OptiInput::IsKeyDown(VK_LCONTROL) &&
            !OptiInput::IsKeyDown(VK_RCONTROL))
            return;
        if (requireAlt && !OptiInput::IsKeyDown(VK_MENU) && !OptiInput::IsKeyDown(VK_LMENU) &&
            !OptiInput::IsKeyDown(VK_RMENU))
            return;

        if (OptiInput::IsKeyReleased(vk))
        {
            lastKey = vk;
            // receivingWmInputs = false;
            inputFlag = true;
            LOG_DEBUG("{}", logMessage);
        }
    };

    const auto currentTick = GetTickCount64();
    const bool canAcceptInputs = lastInputTick + debounceThreshold < currentTick;

    if (!capturingKey && canAcceptInputs)
    {
        CheckShortcut(config->ShortcutKey.value_or_default(), inputMenu, "Menu key pressed, will be switching menu",
                      config->ShortcutKeyRequireCtrl.value_or_default(),
                      config->ShortcutKeyRequireAlt.value_or_default());
        CheckShortcut(config->FpsShortcutKey.value_or_default(), inputFps, "Menu key pressed, will be switching FPS");
        CheckShortcut(config->FGShortcutKey.value_or_default(), inputFG, "Menu key pressed, will be switching FG mode");
        CheckShortcut(config->FpsCycleShortcutKey.value_or_default(), inputFpsCycle,
                      "Menu key pressed, will be switching FPS mode");
        CheckShortcut(config->DlssNrToggleKey.value_or_default(), inputDlssNr,
                      "Neural Rendering key pressed, will be toggling the pass");
    }
    else if (capturingKey)
    {
        lastInputTick = currentTick;
    }

    lastKey = OptiInput::GetLastPressedKey();
}

void MenuCommon::ShowTooltip(const char* tip)
{
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(tip);
        ImGui::EndTooltip();
    }
}

void MenuCommon::ShowHelpMarker(const char* tip)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    ShowTooltip(tip);
}

void MenuCommon::ShowResetButton(CustomOptional<bool, NoDefault>* initFlag, std::string buttonName)
{
    ImGui::SameLine();

    ImGui::BeginDisabled(!initFlag->has_value());

    if (ImGui::Button(buttonName.c_str()))
    {
        initFlag->reset();
        ReInitUpscaler();
    }

    ImGui::EndDisabled();
}

inline void MenuCommon::ReInitUpscaler()
{
    if (!State::Instance().currentFeature)
        return;

    if (State::Instance().currentFeature->GetUpscalerType() == Upscaler::DLSSD)
        State::Instance().newBackend = Upscaler::DLSSD;
    else
        State::Instance().newBackend = currentBackend;

    MARK_ALL_BACKENDS_CHANGED();
}

void MenuCommon::SeparatorWithHelpMarker(const char* label, const char* tip)
{
    auto marker = "(?) ";
    ImGui::SeparatorTextEx(0, label, ImGui::FindRenderedTextEnd(label),
                           ImGui::CalcTextSize(marker, ImGui::FindRenderedTextEnd(marker)).x);
    ShowHelpMarker(tip);
}

class Keybind
{
    std::string name;
    int id;
    bool waitingForKey = false;

  public:
    Keybind(std::string name, int id) : name(name), id(id) {}

    static std::string KeyNameFromVirtualKeyCode(USHORT virtualKey)
    {
        if (virtualKey == (USHORT) UnboundKey)
            return "未绑定";

        UINT scanCode = MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC);

        // Keys like Home would display as Num 0 without this fix
        switch (virtualKey)
        {
        case VK_INSERT:
        case VK_DELETE:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_NUMLOCK:
        case VK_DIVIDE:
        case VK_RCONTROL:
        case VK_RMENU:
            scanCode |= 0xE000;
            break;
        }

        LONG lParam = (scanCode & 0xFF) << 16;
        if (scanCode & 0xE000)
            lParam |= 1 << 24;

        wchar_t buf[64] = {};
        if (GetKeyNameTextW(lParam, buf, static_cast<int>(std::size(buf))) != 0)
            return wstring_to_string(buf);

        return "未知";
    }

    static std::string ShortcutLabel(int virtualKey, bool requireCtrl, bool requireAlt)
    {
        std::string label = KeyNameFromVirtualKeyCode(static_cast<USHORT>(virtualKey));
        if (requireAlt)
            label = "Alt+" + label;
        if (requireCtrl)
            label = "Ctrl+" + label;
        return label;
    }

    void Render(CustomOptional<int>& configKey, bool requireCtrl = false, bool requireAlt = false)
    {
        ImGui::PushID(id);
        if (ImGui::Button(name.c_str()))
        {
            waitingForKey = true;
            capturingKey = true;
            lastKey = 0;
        }
        ImGui::PopID();

        if (waitingForKey)
        {
            ImGui::SameLine();
            ImGui::Text("按任意键...");

            if (lastKey == 0 || lastKey == VK_LBUTTON || lastKey == VK_RBUTTON || lastKey == VK_MBUTTON)
                return;

            if (lastKey == VK_ESCAPE)
            {
                waitingForKey = false;
                capturingKey = false;
                return;
            }

            if (lastKey == VK_BACK)
                lastKey = UnboundKey;

            configKey = lastKey;
            waitingForKey = false;
            capturingKey = false;
            return;
        }

        ImGui::SameLine();
        ImGui::Text(ShortcutLabel(configKey.value_or_default(), requireCtrl, requireAlt).c_str());

        ImGui::SameLine();
        ImGui::PushID(id);
        if (ImGui::Button("R"))
        {
            configKey.reset();
        }
        ImGui::PopID();
    }
};

Upscaler MenuCommon::GetBackendCode(const API api)
{
    if (auto feature = State::Instance().currentFeature)
        return feature->GetUpscalerType();

    Upscaler upscaler;

    if (api == DX11)
        upscaler = Config::Instance()->Dx11Upscaler.value_or_default();
    else if (api == DX12)
        upscaler = Config::Instance()->Dx12Upscaler.value_or_default();
    else
        upscaler = Config::Instance()->VulkanUpscaler.value_or_default();

    return upscaler;
}

void MenuCommon::GetCurrentBackendInfo(const API api, Upscaler& upscaler, std::string* name)
{
    upscaler = GetBackendCode(api);
    *name = UpscalerDisplayName(upscaler, api);
}

void MenuCommon::RenderUpscalerCombo(const API api, Upscaler currentUpscaler, const std::vector<Upscaler>& options)
{
    auto primaryGpu = IdentifyGpu::getPrimaryGpu();

    // Determine display name
    Upscaler targetBackend = State::Instance().newBackend;
    if (targetBackend == Upscaler::Reset)
        targetBackend = currentUpscaler;

    std::string selectedName = UpscalerDisplayName(targetBackend, api);

    if (ImGui::BeginCombo("##升频器Combo", selectedName.c_str()))
    {
        for (auto opt : options)
        {
            // Check if GPU is capable of a given backend
            if (opt == Upscaler::DLSS && !primaryGpu.dlssCapable)
                continue;

            // Not all Intel GPUs support native DX11 XeSS but don't think we have a good way to check exactly
            if (opt == Upscaler::XeSS && api == API::DX11 && primaryGpu.vendorId != VendorId::Intel)
                continue;

            bool isSelected = (currentUpscaler == opt);
            if (ImGui::Selectable(UpscalerDisplayName(opt, api).c_str(), isSelected))
            {
                State::Instance().newBackend = opt;
            }
        }
        ImGui::EndCombo();
    }
}

void MenuCommon::AddDx11Backends(Upscaler upscaler)
{
    RenderUpscalerCombo(API::DX11, upscaler,
                        { Upscaler::XeSS, Upscaler::FSR22, Upscaler::FSR31, Upscaler::XeSS_on12, Upscaler::FSR21_on12,
                          Upscaler::FSR22_on12, Upscaler::FFX_on12, Upscaler::DLSS, Upscaler::DLSS_on12 });
}

void MenuCommon::AddDx12Backends(Upscaler upscaler)
{
    RenderUpscalerCombo(API::DX12, upscaler,
                        { Upscaler::XeSS, Upscaler::FSR21, Upscaler::FSR22, Upscaler::FFX, Upscaler::DLSS });
}

void MenuCommon::AddVulkanBackends(Upscaler upscaler)
{
    RenderUpscalerCombo(API::Vulkan, upscaler,
                        { Upscaler::XeSS, Upscaler::FSR21, Upscaler::FSR22, Upscaler::FFX, Upscaler::FSR21_on12,
                          Upscaler::FFX_on12, Upscaler::DLSS });
}

template <HasDefaultValue B> void MenuCommon::AddResourceBarrier(std::string name, CustomOptional<int32_t, B>* value)
{
    const char* states[] = { "AUTO",
                             "COMMON",
                             "VERTEX_AND_CONSTANT_BUFFER",
                             "INDEX_BUFFER",
                             "RENDER_TARGET",
                             "UNORDERED_ACCESS",
                             "DEPTH_WRITE",
                             "DEPTH_READ",
                             "NON_PIXEL_SHADER_RESOURCE",
                             "PIXEL_SHADER_RESOURCE",
                             "STREAM_OUT",
                             "INDIRECT_ARGUMENT",
                             "COPY_DEST",
                             "COPY_SOURCE",
                             "RESOLVE_DEST",
                             "RESOLVE_SOURCE",
                             "RAYTRACING_ACCELERATION_STRUCTURE",
                             "SHADING_RATE_SOURCE",
                             "GENERIC_READ",
                             "ALL_SHADER_RESOURCE",
                             "PRESENT",
                             "PREDICATION",
                             "VIDEO_DECODE_READ",
                             "VIDEO_DECODE_WRITE",
                             "VIDEO_PROCESS_READ",
                             "VIDEO_PROCESS_WRITE",
                             "VIDEO_ENCODE_READ",
                             "VIDEO_ENCODE_WRITE" };
    const int values[] = { -1,  0,   1,     2,      4,      8,      16,      32,       64,   128,
                           256, 512, 1024,  2048,   4096,   8192,   4194304, 16777216, 2755, 192,
                           0,   310, 65536, 131072, 262144, 524288, 2097152, 8388608 };

    int selected = value->value_or(-1);

    const char* selectedName = "";

    for (int n = 0; n < 28; n++)
    {
        if (values[n] == selected)
        {
            selectedName = states[n];
            break;
        }
    }

    if (ImGui::BeginCombo(name.c_str(), selectedName))
    {
        if (ImGui::Selectable(states[0], !value->has_value()))
            value->reset();

        for (int n = 1; n < 28; n++)
        {
            if (ImGui::Selectable(states[n], selected == values[n]))
                *value = values[n];
        }

        ImGui::EndCombo();
    }
}

static uint32_t GetPresetIndex(IFeature* feature, bool dlssd = false)
{
    auto ratio = (float) feature->TargetWidth() / (float) feature->RenderWidth();

    if (!dlssd)
    {
        if (State::Instance().dlssPresetsOverridenByOpti)
        {
            LOG_DEBUG("DLSS Presets overridden by Opti, using Opti preset indices with ratio: {}", ratio);

            if (ratio <= (Config::Instance()->QualityRatio_UltraPerformance.value_or_default() + 0.01f))
            {
                return Config::Instance()->RenderPresetForAll.value_or(
                    Config::Instance()->RenderPresetUltraPerformance.value_or_default());
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Performance.value_or_default() + 0.01f))
            {
                return Config::Instance()->RenderPresetForAll.value_or(
                    Config::Instance()->RenderPresetPerformance.value_or_default());
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Balanced.value_or_default() + 0.01f))
            {
                return Config::Instance()->RenderPresetForAll.value_or(
                    Config::Instance()->RenderPresetBalanced.value_or_default());
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Quality.value_or_default() + 0.01f))
            {
                return Config::Instance()->RenderPresetForAll.value_or(
                    Config::Instance()->RenderPresetQuality.value_or_default());
            }
            else if (ratio <= (Config::Instance()->QualityRatio_UltraQuality.value_or_default() + 0.01f))
            {
                return Config::Instance()->RenderPresetForAll.value_or(
                    Config::Instance()->RenderPresetUltraQuality.value_or_default());
            }
            else
            {
                return Config::Instance()->RenderPresetForAll.value_or(
                    Config::Instance()->RenderPresetDLAA.value_or_default());
            }
        }
        else if (State::Instance().dlssPresetsOverriddenExternally)
        {
            LOG_DEBUG("DLSS Presets overridden externally, using external preset index: {}",
                      State::Instance().dlssRenderPresetExternal);

            return State::Instance().dlssRenderPresetExternal;
        }
        else
        {
            if (ratio <= (Config::Instance()->QualityRatio_UltraPerformance.value_or_default() + 0.01f))
            {
                return State::Instance().dlssRenderPresetUltraPerformance;
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Performance.value_or_default() + 0.01f))
            {
                return State::Instance().dlssRenderPresetPerformance;
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Balanced.value_or_default() + 0.01f))
            {
                return State::Instance().dlssRenderPresetBalanced;
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Quality.value_or_default() + 0.01f))
            {
                return State::Instance().dlssRenderPresetQuality;
            }
            else if (ratio <= (Config::Instance()->QualityRatio_UltraQuality.value_or_default() + 0.01f))
            {
                return State::Instance().dlssRenderPresetUltraQuality;
            }
            else
            {
                return State::Instance().dlssRenderPresetDLAA;
            }
        }
    }
    else
    {
        if (State::Instance().dlssdPresetsOverridenByOpti)
        {
            if (ratio <= (Config::Instance()->QualityRatio_UltraPerformance.value_or_default() + 0.01f))
            {
                return Config::Instance()->DLSSDRenderPresetForAll.value_or(
                    Config::Instance()->DLSSDRenderPresetUltraPerformance.value_or_default());
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Performance.value_or_default() + 0.01f))
            {
                return Config::Instance()->DLSSDRenderPresetForAll.value_or(
                    Config::Instance()->DLSSDRenderPresetPerformance.value_or_default());
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Balanced.value_or_default() + 0.01f))
            {
                return Config::Instance()->DLSSDRenderPresetForAll.value_or(
                    Config::Instance()->DLSSDRenderPresetBalanced.value_or_default());
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Quality.value_or_default() + 0.01f))
            {
                return Config::Instance()->DLSSDRenderPresetForAll.value_or(
                    Config::Instance()->DLSSDRenderPresetQuality.value_or_default());
            }
            else if (ratio <= (Config::Instance()->QualityRatio_UltraQuality.value_or_default() + 0.01f))
            {
                return Config::Instance()->DLSSDRenderPresetForAll.value_or(
                    Config::Instance()->DLSSDRenderPresetUltraQuality.value_or_default());
            }
            else
            {
                return Config::Instance()->DLSSDRenderPresetForAll.value_or(
                    Config::Instance()->DLSSDRenderPresetDLAA.value_or_default());
            }
        }
        else if (State::Instance().dlssdPresetsOverriddenExternally)
        {
            return State::Instance().dlssdRenderPresetExternal;
        }
        else
        {
            if (ratio <= (Config::Instance()->QualityRatio_UltraPerformance.value_or_default() + 0.01f))
            {
                return State::Instance().dlssdRenderPresetUltraPerformance;
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Performance.value_or_default() + 0.01f))
            {
                return State::Instance().dlssdRenderPresetPerformance;
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Balanced.value_or_default() + 0.01f))
            {
                return State::Instance().dlssdRenderPresetBalanced;
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Quality.value_or_default() + 0.01f))
            {
                return State::Instance().dlssdRenderPresetQuality;
            }
            else if (ratio <= (Config::Instance()->QualityRatio_UltraQuality.value_or_default() + 0.01f))
            {
                return State::Instance().dlssdRenderPresetUltraQuality;
            }
            else
            {
                return State::Instance().dlssdRenderPresetDLAA;
            }
        }
    }

    return 0;
}

// TODO: disable presets based on the detected DLSS version
template <HasDefaultValue B> void MenuCommon::AddDLSSRenderPreset(std::string name, CustomOptional<uint32_t, B>* value)
{
    // clang-format off
    static const std::vector<MenuOption<uint32_t>> presets = {
        { NVSDK_NGX_DLSS_Hint_Render_Preset_Default, "DEFAULT", 
            "使用游戏默认设置" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_A, "PRESET A",
            "适用于性能/平衡/质量模式。\n较旧变体，适合抑制重影……\n新版本已移除！" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_B, "PRESET B",
            "适用于超级性能模式。\n类似预设 A……\n新版本已移除！" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_C, "PRESET C",
            "适用于性能/平衡/质量模式。\n通常更侧重当前帧信息……\n新版本已移除！" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_D, "PRESET D",
            "性能/平衡/质量模式的默认预设；\n通常更侧重画面稳定性。\n新版本已移除！" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_E, "PRESET E",
            "DLSS 3.7+，更好的 D 预设\n新版本已移除！" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_F, "PRESET F",
            "超级性能与 DLAA 模式的默认预设\n新版本已移除！" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_G, "PRESET G",
            "未使用" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_H_Reserved, "PRESET H",
            "未使用" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_I_Reserved, "PRESET I",
            "未使用" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_J, "PRESET J",
            "类似预设 K。预设 J 重影可能略少……\n第一代 Transformer" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_K, "PRESET K",
            "DLAA/平衡/质量模式的默认预设……\n第一代 Transformer" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_L, "PRESET L",
            "超级性能模式默认\n第二代 Transformer" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_M, "PRESET M",
            "性能模式默认\n第二代 Transformer" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_N, "PRESET N",
            "未使用" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_O, "PRESET O",
            "未使用" },
        { NV_PRESET_LATEST, "Latest",
            "该 dll 支持的最新版" }
    };
    // clang-format on

    PopulateCombo(name, *value, presets);
}

template <HasDefaultValue B> void MenuCommon::AddDLSSDRenderPreset(std::string name, CustomOptional<uint32_t, B>* value)
{
    // We don't have DLSSD definitions so using raw values
    static const std::vector<MenuOption<uint32_t>> presets = {
        { 0, "DEFAULT", "使用游戏默认设置" },
        { 1, "PRESET A", "预设 A\n新版本已移除！" },
        { 2, "PRESET B", "预设 B\n新版本已移除！" },
        { 3, "PRESET C", "预设 C\n新版本已移除！" },
        { 4, "PRESET D", "默认模型，Transformer" },
        { 5, "PRESET E", "最新 Transformer 模型\n如需景深引导必须使用" },
        { 6, "PRESET F", "最新 Transformer 模型\n如需景深引导必须使用" },
        { NV_PRESET_LATEST, "Latest", "该 dll 支持的最新版" }
    };

    PopulateCombo(name, *value, presets);
}

template <typename TStorage, typename T>
void MenuCommon::PopulateCombo(const std::string& name, TStorage& currentValue,
                               const std::vector<MenuOption<T>>& options)
{
    if (options.empty())
        return;

    // Assumes that different types mean that TStorage is std::optional
    T currentVal;
    if constexpr (std::is_same_v<TStorage, T>)
        currentVal = currentValue;
    else
        currentVal = currentValue.value_or(options[0].value);

    // Find the label for the currently selected item
    std::string preview = "未知";
    for (const auto& opt : options)
    {
        if (opt.value == currentVal)
        {
            preview = opt.label;
            break;
        }
    }

    if (ImGui::BeginCombo(name.c_str(), preview.c_str()))
    {
        for (const auto& opt : options)
        {
            if (opt.hidden)
                continue;

            if (opt.disabled)
                ImGui::BeginDisabled();

            bool isSelected = (currentVal == opt.value);
            if (ImGui::Selectable(opt.label.c_str(), isSelected))
                currentValue = opt.value;

            // Show tooltip for the individual item if it exists
            if (!opt.tooltip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s", opt.tooltip.c_str());

            if (opt.disabled)
                ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
}

static UiTargetMode getUiTargetMode()
{
    const auto& state = State::Instance();

    const bool fallback = !Config::Instance()->OverlayMenu.value_or_default();

    if (fallback)
    {
        // We have no reliable swapchain/output color-space information here.
        // Only classify the upscaled working image.
        if (state.currentFeature && state.currentFeature->IsHdr())
            return UiTargetMode::LinearHDR;

        return UiTargetMode::SDR;
    }

    const auto& output = state.outputColorSpace;

    // If SetColorSpace1 has not provided a known/valid color space,
    // fall back conservatively.
    if (!output.valid)
        return UiTargetMode::SDR;

    switch (output.transfer)
    {
    case ColorTransfer::Linear:
        // scRGB: linear Rec.709 RGB.
        //
        // hdrOutputActive is intentionally NOT required here.
        // A scRGB swapchain is still linear even when the physical output
        // is currently SDR. hdrOutputActive only affects the desired
        // reference-white scaling in toneMapColor().
        if (output.model == ColorModel::RGB && output.primaries == ColorPrimaries::Rec709)
        {
            return UiTargetMode::ScRGB;
        }

        break;

    case ColorTransfer::PQ:
        // Direct PQ UI rendering currently assumes RGB PQ / Rec.2020.
        //
        // Do not treat YCbCr PQ as an RGB render target.
        if (output.model == ColorModel::RGB && output.primaries == ColorPrimaries::Rec2020)
        {
            return UiTargetMode::PQ;
        }

        break;

    case ColorTransfer::HLG:
        // Current HLG UI path assumes an RGB render target.
        //
        // DXGI HLG modes are commonly YCbCr, so reject unsupported
        // combinations rather than applying an RGB HLG transform blindly.
        if (output.model == ColorModel::RGB && output.primaries == ColorPrimaries::Rec2020)
        {
            return UiTargetMode::HLG;
        }

        break;

    case ColorTransfer::SRGB:
        return UiTargetMode::SDR;

    case ColorTransfer::Unknown:
    default:
        break;
    }

    // Unsupported model / primaries / transfer combination.
    return UiTargetMode::SDR;
}

static float srgbToLinear(float x)
{
    x = std::clamp(x, 0.0f, 1.0f);

    if (x <= 0.04045f)
        return x / 12.92f;

    return std::pow((x + 0.055f) / 1.055f, 2.4f);
}

static float linearToPQ(float nits)
{
    // SMPTE ST.2084
    constexpr float m1 = 2610.0f / 16384.0f;
    constexpr float m2 = 2523.0f / 32.0f;
    constexpr float c1 = 3424.0f / 4096.0f;
    constexpr float c2 = 2413.0f / 128.0f;
    constexpr float c3 = 2392.0f / 128.0f;

    const float y = std::clamp(nits / 10000.0f, 0.0f, 1.0f);
    const float ym1 = std::pow(y, m1);

    return std::pow((c1 + c2 * ym1) / (1.0f + c3 * ym1), m2);
}

static float linearToHLG(float x)
{
    // BT.2100 HLG OETF
    constexpr float a = 0.17883277f;
    constexpr float b = 0.28466892f;
    constexpr float c = 0.55991073f;

    x = std::max(x, 0.0f);

    if (x <= (1.0f / 12.0f))
        return std::sqrt(3.0f * x);

    return a * std::log(12.0f * x - b) + c;
}

static ImVec4 linear709To2020(float r, float g, float b)
{
    return ImVec4(0.6274040f * r + 0.3292820f * g + 0.0433136f * b, 0.0690970f * r + 0.9195400f * g + 0.0113612f * b,
                  0.0163916f * r + 0.0880132f * g + 0.8955950f * b, 0.0f);
}

static ImVec4 legacyHdrToneMap(const ImVec4& color)
{
    constexpr float exposure = 1.0f;
    constexpr float strength = 1.0f;

    const float peak = std::max(color.x, std::max(color.y, color.z));

    if (peak <= 0.0f)
        return color;

    const float exposedPeak = peak * exposure;
    const float mappedPeak = exposedPeak / (1.0f + exposedPeak);

    const float reinhardScale = mappedPeak / peak;
    const float scale = 1.0f + (reinhardScale - 1.0f) * strength;

    return ImVec4(color.x * scale, color.y * scale, color.z * scale, color.w);
}

static ImVec4 toneMapColor(const ImVec4& color)
{
    const auto mode = getUiTargetMode();

    switch (mode)
    {
    case UiTargetMode::SDR:
        return color;

    case UiTargetMode::LinearHDR:
        return ImVec4(srgbToLinear(color.x), srgbToLinear(color.y), srgbToLinear(color.z), color.w);

    case UiTargetMode::ScRGB:
    {
        constexpr float scRgbReferenceWhiteNits = 80.0f;
        constexpr float hdrUiWhiteNits = 203.0f;

        const float uiWhiteNits = State::Instance().hdrOutputActive ? hdrUiWhiteNits : scRgbReferenceWhiteNits;

        const float scale = uiWhiteNits / scRgbReferenceWhiteNits;

        return ImVec4(srgbToLinear(color.x) * scale, srgbToLinear(color.y) * scale, srgbToLinear(color.z) * scale,
                      color.w);
    }

    case UiTargetMode::PQ:
        // Direct ImGui rendering into a nonlinear PQ target.
        //
        // Proper PQ encoding of vertex colors produces incorrect results
        // with the standard ImGui alpha blend state because blending then
        // happens in PQ space.
        //
        // Keep the known-good legacy compression until PQ rendering is
        // moved to a linear intermediate/composite pass.
        return legacyHdrToneMap(color);

    case UiTargetMode::HLG:
        // Same fundamental nonlinear-blending problem as PQ.
        // Conservative compatibility behavior for now.
        return legacyHdrToneMap(color);

    default:
        return color;
    }
}

static void MenuHdrCheck(ImGuiIO io)
{
    if (!_hdrTonemapApplied)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        const auto mode = getUiTargetMode();

        LOG_INFO("Output HDR: {}, UI Mode: {}", State::Instance().hdrOutputActive, magic_enum::enum_name(mode));

        CopyMemory(SdrColors, style.Colors, sizeof(style.Colors));

        // Apply tone mapping to the ImGui style
        for (int i = 0; i < ImGuiCol_COUNT; ++i)
        {
            ImVec4 color = style.Colors[i];
            style.Colors[i] = toneMapColor(color);
        }

        _hdrTonemapApplied = true;
    }
}

static float MenuResolutionScale(ImGuiIO io)
{
    if (Config::Instance()->MenuScale.has_value())
        return Config::Instance()->MenuScale.value();

    // Calculate menu scale according to display resolution
    float y = State::Instance().screenHeight;

    if (io.DisplaySize.y != 0)
        y = (float) io.DisplaySize.y;

    // 1000p is minimum for 1.0 menu ratio
    float result = (float) ((int) (y / 108.0f)) / 10.0f;

    result = std::round(result * 10.0f) / 10.0f;

    if (result < 0.5f)
        result = 0.5f;

    if (result > 2.0f)
        result = 2.0f;

    return result;
}

inline static std::string GetSourceString(UINT source)
{
    switch (source)
    {
    case 1:
        return "RTV";
    case 2:
        return "SRV";
    case 4:
        return "UAV";
    case 8:
        return "OM";
    case 16:
        return "Ups";
    case 32:
        return "SCR";
    case 64:
        return "SGR";
    case 128:
        return "OMUAV";
    default:
        return std::format("{}", source);
    }
}

inline static std::string GetDispatchString(UINT source)
{
    switch (source)
    {
    case 0:
        return "-";
    case 512:
        return "DI";
    case 1024:
        return "DII";
    case 256:
        return "Disp";
    default:
        return std::format("{}", source);
    }
}

void MenuCommon::ApplyThemeStyle()
{
    if (ImGui::GetCurrentContext() == nullptr)
        return;

    ImGuiStyle& style = ImGui::GetStyle();

    auto conf = Config::Instance();
    bool lightTheme = conf->LightTheme.value_or_default();

    style.WindowRounding = 8.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 8.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;

    style.WindowPadding = ImVec2(16.0f, 16.0f);
    style.FramePadding = ImVec2(8.0f, 8.0f);
    style.ItemSpacing = ImVec2(8.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.CellPadding = ImVec2(8.0f, 6.0f);
    style.SeparatorTextPadding = ImVec2(8.0f, 6.0f);

    style.WindowBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;

    style.FrameBorderSize = lightTheme ? 1.0f : 0.0f;
    style.TabBorderSize = lightTheme ? 1.0f : 0.0f;

    style.ScrollbarSize = 10.0f;
    style.GrabMinSize = 10.0f;

    auto Clamp01 = [](float v) { return std::max(0.0f, std::min(v, 1.0f)); };

    auto Mix = [](const ImVec4& a, const ImVec4& b, float t, float alpha = 1.0f)
    { return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, alpha); };

    auto Luminance = [](const ImVec4& c) { return c.x * 0.2126f + c.y * 0.7152f + c.z * 0.0722f; };

    auto Saturate = [&](const ImVec4& color, float amount)
    {
        float lum = Luminance(color);

        return ImVec4(Clamp01(lum + (color.x - lum) * amount), Clamp01(lum + (color.y - lum) * amount),
                      Clamp01(lum + (color.z - lum) * amount), color.w);
    };

    ImVec4 accent = ImVec4(conf->MenuAccentColorR.value_or_default(), conf->MenuAccentColorG.value_or_default(),
                           conf->MenuAccentColorB.value_or_default(), 1.0f);

    ImVec4 bgAccent = ImVec4(conf->MenuBGColorR.value_or_default(), conf->MenuBGColorG.value_or_default(),
                             conf->MenuBGColorB.value_or_default(), 1.0f);

    float luminance = Luminance(accent);

    const ImVec4 bgDark = lightTheme ? ImVec4(0.80f, 0.82f, 0.86f, 1.00f) : ImVec4(0.09f, 0.09f, 0.10f, 1.00f);
    const ImVec4 bgMid = lightTheme ? ImVec4(0.89f, 0.91f, 0.95f, 1.00f) : ImVec4(0.11f, 0.11f, 0.12f, 1.00f);
    const ImVec4 bgLight = lightTheme ? ImVec4(0.96f, 0.97f, 0.99f, 1.00f) : ImVec4(0.14f, 0.14f, 0.15f, 1.00f);

    const ImVec4 textPrimary = lightTheme ? ImVec4(0.05f, 0.06f, 0.08f, 1.00f) : ImVec4(0.90f, 0.93f, 0.95f, 1.00f);
    const ImVec4 textDim = lightTheme ? ImVec4(0.22f, 0.25f, 0.31f, 1.00f) : ImVec4(0.54f, 0.58f, 0.62f, 1.00f);

    const ImVec4 borderCol = lightTheme ? ImVec4(0.35f, 0.40f, 0.50f, 1.00f) : ImVec4(0.24f, 0.24f, 0.26f, 1.00f);
    const ImVec4 dimBg = lightTheme ? ImVec4(0.30f, 0.33f, 0.38f, 0.20f) : ImVec4(0.09f, 0.10f, 0.13f, 0.20f);
    const ImVec4 modalDimBg = lightTheme ? ImVec4(0.22f, 0.24f, 0.28f, 0.55f) : ImVec4(0.04f, 0.04f, 0.07f, 0.55f);

    // MenuBGColor: only background/surface tint.
    auto BgTint = [&](const ImVec4& base, float strength = 1.0f, float alpha = 1.0f)
    {
        float t = lightTheme ? (0.180f * strength) : (0.120f * strength);
        return Mix(base, bgAccent, t, alpha);
    };

    // MenuAccentColor: all visible interactive accent colors.
    auto AccentSoft = [&](float alpha = 1.0f)
    { return lightTheme ? Mix(bgLight, accent, 0.14f, alpha) : Mix(bgDark, accent, 0.32f, alpha); };

    auto AccentMed = [&](float alpha = 1.0f)
    { return lightTheme ? Mix(bgLight, accent, 0.42f, alpha) : Mix(bgDark, accent, 0.55f, alpha); };

    auto AccentStrong = [&](float alpha = 1.0f) { return ImVec4(accent.x, accent.y, accent.z, alpha); };

    const ImVec4 bgTitle = AccentSoft();

    auto SurfaceHover = [&](float alpha = 1.0f)
    { return lightTheme ? Mix(bgLight, accent, 0.12f, alpha) : Mix(bgLight, accent, 0.18f, alpha); };

    auto SurfaceActive = [&](float alpha = 1.0f)
    { return lightTheme ? Mix(bgLight, accent, 0.20f, alpha) : Mix(bgLight, accent, 0.28f, alpha); };

    auto TitleActive = [&](float alpha = 1.0f)
    { return lightTheme ? Mix(bgTitle, accent, 0.18f, alpha) : Mix(bgTitle, accent, 0.16f, alpha); };

    auto PlotAccent = [&](float alpha = 1.0f)
    {
        if (lightTheme)
        {
            // Darken slightly for contrast on light bg — no channel floors
            return Mix(accent, ImVec4(0.00f, 0.00f, 0.00f, 1.00f), 0.20f, alpha);
        }

        // Brighten slightly for visibility on dark bg — no channel floors
        return Mix(accent, ImVec4(1.00f, 1.00f, 1.00f, 1.00f), 0.35f, alpha);
    };

    auto PlotAccentHovered = [&](float alpha = 1.0f)
    {
        if (lightTheme)
        {
            return Mix(PlotAccent(alpha), ImVec4(0.00f, 0.00f, 0.00f, 1.00f), 0.15f, alpha);
        }

        return Mix(PlotAccent(alpha), ImVec4(1.00f, 1.00f, 1.00f, 1.00f), 0.25f, alpha);
    };

    auto AccentReadable = [&](float alpha = 1.0f)
    {
        // Apply saturation boost and luminance correction only here,
        // so AccentStrong / AccentMed / AccentSoft stay true to the user's pick.
        ImVec4 a = Saturate(accent, lightTheme ? 1.35f : 1.25f);
        float lum = Luminance(a);

        if (lightTheme && lum > 0.72f)
            a = Mix(a, ImVec4(0.0f, 0.0f, 0.0f, 1.0f), 0.35f, 1.0f);

        if (!lightTheme && lum < 0.25f)
            a = Mix(a, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 0.30f, 1.0f);

        return ImVec4(a.x, a.y, a.z, alpha);
    };

    ImVec4* c = ImGui::GetStyle().Colors;

    float minAlpha = Config::Instance()->MenuBGColorA.value_or_default() >= 0.5f
                         ? Config::Instance()->MenuBGColorA.value_or_default()
                         : 0.5f;

    c[ImGuiCol_Text] = textPrimary;
    c[ImGuiCol_TextDisabled] = textDim;
    c[ImGuiCol_TextLink] = AccentReadable();

    // MenuBGColor only.
    c[ImGuiCol_WindowBg] = BgTint(bgDark, 1.00f, Config::Instance()->MenuBGColorA.value_or_default());
    c[ImGuiCol_ChildBg] = BgTint(bgMid, 1.10f, minAlpha + 0.1f);
    c[ImGuiCol_PopupBg] =
        lightTheme ? BgTint(bgLight, 0.90f) : BgTint(ImVec4(0.09f, 0.10f, 0.13f, 0.97f), 0.90f, 0.97f);
    c[ImGuiCol_MenuBarBg] = BgTint(bgDark, 0.85f);
    c[ImGuiCol_DockingEmptyBg] = BgTint(bgDark, 0.75f);

    c[ImGuiCol_Border] = borderCol;
    c[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Neutral background, not MenuBGColor.
    c[ImGuiCol_FrameBg] = BgTint(bgLight, 0.50f, minAlpha + 0.15f);
    c[ImGuiCol_FrameBgHovered] = SurfaceHover();
    c[ImGuiCol_FrameBgActive] = SurfaceActive();

    c[ImGuiCol_TitleBg] = BgTint(bgTitle, 0.40f);
    c[ImGuiCol_TitleBgActive] = TitleActive();
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(bgTitle.x, bgTitle.y, bgTitle.z, 0.75f);

    c[ImGuiCol_ScrollbarBg] = BgTint(bgDark, 0.60f, minAlpha + 0.2f);
    c[ImGuiCol_ScrollbarGrab] = AccentSoft();
    c[ImGuiCol_ScrollbarGrabHovered] = AccentMed();
    c[ImGuiCol_ScrollbarGrabActive] = AccentStrong();

    c[ImGuiCol_CheckMark] = AccentReadable();
    c[ImGuiCol_SliderGrab] = AccentMed();
    c[ImGuiCol_SliderGrabActive] = AccentReadable();
    c[ImGuiCol_InputTextCursor] = AccentReadable();

    c[ImGuiCol_Button] = AccentSoft();
    c[ImGuiCol_ButtonHovered] = AccentMed();
    c[ImGuiCol_ButtonActive] = AccentStrong();

    c[ImGuiCol_Header] = AccentSoft(0.90f);
    c[ImGuiCol_HeaderHovered] = AccentMed(0.95f);
    c[ImGuiCol_HeaderActive] = AccentStrong();

    c[ImGuiCol_Separator] = borderCol;
    c[ImGuiCol_SeparatorHovered] = AccentMed(0.85f);
    c[ImGuiCol_SeparatorActive] = AccentStrong();

    c[ImGuiCol_ResizeGrip] = AccentSoft(0.30f);
    c[ImGuiCol_ResizeGripHovered] = AccentStrong(0.70f);
    c[ImGuiCol_ResizeGripActive] = AccentStrong(0.95f);

    c[ImGuiCol_Tab] = AccentSoft();
    c[ImGuiCol_TabHovered] = AccentMed();
    c[ImGuiCol_TabSelected] = AccentSoft();
    c[ImGuiCol_TabSelectedOverline] = AccentStrong();
    c[ImGuiCol_TabDimmed] = BgTint(bgDark, 0.60f);
    c[ImGuiCol_TabDimmedSelected] = AccentSoft(0.75f);
    c[ImGuiCol_TabDimmedSelectedOverline] = borderCol;

    c[ImGuiCol_DockingPreview] = AccentStrong(0.70f);

    c[ImGuiCol_PlotLines] = PlotAccent();
    c[ImGuiCol_PlotLinesHovered] = PlotAccentHovered();
    c[ImGuiCol_PlotHistogram] = PlotAccent(0.85f);
    c[ImGuiCol_PlotHistogramHovered] = PlotAccentHovered();

    c[ImGuiCol_TableHeaderBg] = BgTint(bgMid, 0.80f, minAlpha + 0.25f);
    c[ImGuiCol_TableBorderStrong] = borderCol;
    c[ImGuiCol_TableBorderLight] = lightTheme ? ImVec4(0.68f, 0.72f, 0.80f, 1.00f) : AccentSoft();
    c[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.0f);
    c[ImGuiCol_TableRowBgAlt] = lightTheme ? ImVec4(0.00f, 0.00f, 0.00f, 0.045f) : ImVec4(1.00f, 1.00f, 1.00f, 0.03f);

    c[ImGuiCol_TreeLines] = borderCol;
    c[ImGuiCol_TextSelectedBg] = AccentMed(0.38f);
    c[ImGuiCol_DragDropTarget] = AccentStrong(0.90f);
    c[ImGuiCol_NavCursor] = AccentReadable();
    c[ImGuiCol_NavWindowingHighlight] = AccentStrong(0.70f);
    c[ImGuiCol_NavWindowingDimBg] = dimBg;
    c[ImGuiCol_ModalWindowDimBg] = modalDimBg;

    _hdrTonemapApplied = false;
    MenuHdrCheck(ImGui::GetIO());
}

static double lastTime = 0.0;
static double lastFrameTime = 0.0;
static UINT64 uwpTargetFrame = 0;

void MenuCommon::Present()
{
    _frameCount++;

    auto now = Util::MillisecondsNow();

    if (lastTime > 0.0)
        lastFrameTime = now - lastTime;

    lastTime = now;

    if (_handle != nullptr)
        UpdateManualInput(_handle);
}

struct VersionCheckStatus
{
    bool completed = false;
    bool updateAvailable = false;
    std::string latestTag;
    std::string latestUrl;
    std::string error;
};

struct MenuCommon::RenderMenuContext
{
    State& state;
    decltype(Config::Instance()) config;
    ImGuiIO& io;
    IFeature* currentFeature = nullptr;

    double now = 0.0;
    double frameTime = 0.0;
    double frameRate = 0.0;
    float menuResScale = 1.0f;
    float fpsScale = 1.0f;
    float averageFrameTime = 0.0f;
    float averageUpscalerFT = 0.0f;

    bool frameTimesCalculated = false;
    bool newFrame = false;

    VersionCheckStatus versionStatus;
    std::string currentVersionText;

    // Cached when the menu is visible and shared by RenderMainMenuWindow section helpers.
    std::unique_ptr<std::decay_t<decltype(IdentifyGpu::getPrimaryGpu())>> primaryGpu;
};

static std::string splashMessage;

void MenuCommon::UpdateRenderTiming(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& now = ctx.now;
    auto& frameTime = ctx.frameTime;
    auto& frameRate = ctx.frameRate;

    if (config->OverlayMenu.value_or_default())
    {
        _frameCount++;

        // FPS & frame time calculation
        if (lastTime > 0.0)
        {
            frameTime = now - lastTime;
            frameRate = 1000.0 / frameTime;
        }

        lastTime = now;

        if (_handle != nullptr)
            UpdateManualInput(_handle);
    }
    else
    {
        if (state.activeFgInput == FGInput::NoFG || state.activeFgOutput == FGOutput::NoFG)
            MenuCommon::Present();

        frameTime = lastFrameTime;
        frameRate = 1000.0 / frameTime;
    }

    state.frameTimes.pop_front();
    state.frameTimes.push_back(frameTime);
}

void MenuCommon::UpdateMenuInputMode(RenderMenuContext& ctx)
{
    auto& io = ctx.io;

    // Moved here to prevent gamepad key replay
    if (_isVisible)
    {
        if (hasGamepad)
            io.BackendFlags |= ImGuiBackendFlags_HasGamepad;

        io.ConfigFlags = ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    }
    else
    {
        capturingKey = false;
        hasGamepad = (io.BackendFlags & ImGuiBackendFlags_HasGamepad) != 0;
        io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
        io.ConfigFlags = ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoKeyboard;
    }
}

void MenuCommon::HandleMenuShortcuts(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& io = ctx.io;

    // Handle Inputs
    {
        if (inputFG)
        {
            inputFG = false;

            if (state.activeFgInput != FGInput::NoFG && state.activeFgOutput != FGOutput::NoFG &&
                (state.currentFGSwapchain != nullptr || state.activeFgInput == FGInput::NvngxFG))
            {
                config->FGEnabled = !config->FGEnabled.value_or_default();
                LOG_DEBUG("FG toggle key pressed, setting FGEnabled to {}", config->FGEnabled.value_or_default());

                if (config->FGEnabled.value_or_default())
                    state.fgChanged = true;
            }
        }

        if (inputFps)
        {
            inputFps = false;
            config->ShowFps = !config->ShowFps.value_or_default();
        }

        if (inputDlssNr)
        {
            inputDlssNr = false;
            config->DlssNrEnabled = !config->DlssNrEnabled.value_or_default();
            LOG_DEBUG("Neural Rendering toggle key pressed, setting DlssNrEnabled to {}",
                      config->DlssNrEnabled.value_or_default());

            ImGuiToast toast { ImGuiToastType::Info, 2000 };
            toast.setTitle("DLSS 神经渲染");
            toast.setContent(config->DlssNrEnabled.value_or_default() ? "开" : "关");
            ImGui::InsertNotification(toast);
        }

        if (inputFpsCycle && config->ShowFps.value_or_default())
            config->FpsOverlayType = (FpsOverlay) ((config->FpsOverlayType.value_or_default() + 1) % FpsOverlay_COUNT);

        if (inputMenu)
        {
            inputMenu = false;
            _isVisible = !_isVisible;

            LOG_DEBUG("Menu key pressed, {0}", _isVisible ? "opening ImGui" : "closing ImGui");

            if (_isVisible)
            {
                io.ClearEventsQueue();
                io.ClearInputKeys();
                io.ClearInputMouse();

                OptiInput::ResetMenuInputTransientState();

                ApplyThemeStyle();

                refreshRate = Util::GetActiveRefreshRate(_handle);

                auto optiPath = std::filesystem::path(Config::Instance()->MainDllPath.value());
                state.artursFgFileAvailable = enablerExists.Get(optiPath / L"dlss-enabler-headless.dll");
                state.nukemsFgFileAvailable = nukemsExists.Get(optiPath / L"dlssg_to_fsr3_amd_is_better.dll");

                if (State::Instance().currentFeature != nullptr)
                {
                    if (State::Instance().currentFeature->GetUpscalerType() == Upscaler::DLSSD)
                        comboPreset = config->DLSSDRenderPresetForAll.value_or_default();
                    else if (State::Instance().currentFeature->GetUpscalerType() == Upscaler::DLSS)
                        comboPreset = config->RenderPresetForAll.value_or_default();
                }
            }
            else
            {
                ImGui::CloseCurrentPopup();

                _showMipmapCalcWindow = false;
                _showHudlessWindow = false;
            }

            io.MouseDrawCursor = _isVisible;
            io.WantCaptureKeyboard = _isVisible;
            io.WantCaptureMouse = _isVisible;
        }

        inputFpsCycle = false;
    }
}

void MenuCommon::UpdateVersionAndStartupNotifications(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& now = ctx.now;
    auto& versionStatus = ctx.versionStatus;

    constexpr double splashTime = 7000.0;
    constexpr int updateNoticeTime = 10000;

    // Version check state is copied while locked, then consumed by the UI render pass.
    {
        std::scoped_lock lock(state.versionCheckMutex);
        versionStatus.completed = state.versionCheckCompleted;
        versionStatus.updateAvailable = state.updateAvailable;
        versionStatus.latestTag = state.latestVersionTag;
        versionStatus.latestUrl = state.latestVersionUrl;
        versionStatus.error = state.versionCheckError;
    }

    ctx.currentVersionText = VersionCheck::CurrentVersionString();

    if (versionStatus.completed && versionStatus.updateAvailable && !versionStatus.latestTag.empty())
    {
        if (updateNoticeTag != versionStatus.latestTag)
        {
            updateNoticeTag = versionStatus.latestTag;
            updateNoticeUrl = versionStatus.latestUrl;
            const auto notice = [&]()
            {
                ImGuiToast updateNotification { ImGuiToastType::Error, updateNoticeTime };
                updateNotification.setTitle("OptiScaler 有更新可用");
                updateNotification.setContent("按 %s 查看详情",
                                              Keybind::ShortcutLabel(config->ShortcutKey.value_or_default(),
                                                                     config->ShortcutKeyRequireCtrl.value_or_default(),
                                                                     config->ShortcutKeyRequireAlt.value_or_default())
                                                  .c_str());
                ImGui::InsertNotification(updateNotification);
                return true;
            };
            static auto res = notice();
        }
    }

    // One-shot startup warning notifications.
    if (!state.postDone)
    {
        if (state.postCodes & PostCode::SlPluginsAlreadyInMemory)
        {
            auto filename = Util::DllPath().filename().string();
            to_lower_in_place(filename);

            ImGuiToast notification { ImGuiToastType::Warning, 10000 };
            notification.setTitle("检测到 Streamline 延迟挂钩");
            notification.setContent(
                "建议将 OptiScaler 由 %s 改为其他支持的名称。\n否则可能遇到问题。",
                filename.c_str());
            ImGui::InsertNotification(notification);
        }

        if (state.postCodes & PostCode::TryingFsr4Fp8OnUnsupported)
        {
            ImGuiToast notification { ImGuiToastType::Warning, 10000 };
            notification.setTitle("检测到错误配置");
            notification.setContent("FSR 4 FP8 仅支持 AMD");
            ImGui::InsertNotification(notification);
        }

        state.postDone = true;
    }

    // Initialize splash timing and select the splash text once per process.
    if (splashLimit < 1.0f)
    {
        splashStart = now + 100.0;
        splashLimit = splashStart + splashTime;

        std::srand(static_cast<unsigned>(std::time(nullptr)));
        splashMessage = splashText[std::rand() % splashText.size()];
    }
}

void MenuCommon::BeginMenuFrameIfNeeded(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& io = ctx.io;
    auto& now = ctx.now;
    auto& newFrame = ctx.newFrame;

    // New frame check
    if ((!config->DisableSplash.value_or_default() && now > splashStart && now < splashLimit) ||
        config->ShowFps.value_or_default() || _isVisible || ImGui::notifications.size() > 0 ||
        (config->DlssNrCompare.value_or_default() != 0 && config->DlssNrCompareTags.value_or_default()))
    {
        if (!_isUWP)
        {
            ImGui_ImplWin32_NewFrame();
        }
        else
        {
            ImVec2 displaySize { state.screenWidth, state.screenHeight };
            ImGui_ImplUwp_NewFrame(displaySize);
        }

        OptiInput::FeedImGui(_isVisible);

        MenuHdrCheck(io);
        ImGui::NewFrame();

        newFrame = true;
    }
}

void MenuCommon::RenderSplashWindow(RenderMenuContext& ctx)
{
    auto config = ctx.config;
    auto& io = ctx.io;
    auto& now = ctx.now;

    constexpr double fadeTime = 1000.0;

    // Splash screen
    if (!config->DisableSplash.value_or_default())
    {
        if (now > splashStart && now < splashLimit)
        {

            ImGui::SetNextWindowSize({ 0.0f, 0.0f });
            ImGui::SetNextWindowBgAlpha(config->FpsOverlayAlpha.value_or_default());
            ImGui::SetNextWindowPos(splashPosition, ImGuiCond_Always);

            float windowAlpha = 1.0f;
            if (auto diff = now - splashStart; diff < fadeTime)
                windowAlpha = static_cast<float>(diff / fadeTime);
            else if (auto diff = splashLimit - now; diff < fadeTime)
                windowAlpha = static_cast<float>(diff / fadeTime);

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, windowAlpha);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 8));
            ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));

            if (!config->OverlaysUseTheme.value_or_default())
            {
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, toneMapColor(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
            }

            if (ImGui::Begin("Splash", nullptr,
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration |
                                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoNav))
            {
                float splashScale = 1.0f;
                float baseScaleHeight = 720.0f;

                if (io.DisplaySize.y > baseScaleHeight)
                    splashScale = io.DisplaySize.y / baseScaleHeight;

                if (config->UseHQFont.value_or_default())
                    ImGui::PushFontSize(std::round(splashScale * fontSize));
                else
                    ImGui::SetWindowFontScale(splashScale);

                ImGui::Text("OptiScaler - 按 %s 打开菜单",
                            Keybind::ShortcutLabel(config->ShortcutKey.value_or_default(),
                                                   config->ShortcutKeyRequireCtrl.value_or_default(),
                                                   config->ShortcutKeyRequireAlt.value_or_default())
                                .c_str());
                ImGui::TextColored(toneMapColor(ImVec4(1.0f, 1.0f, 1.0f, 0.7f)), splashMessage.c_str());

                splashSize = ImGui::GetWindowSize();

                if (config->UseHQFont.value_or_default())
                    ImGui::PopFontSize();

                ImGui::End();

                splashPosition.x = 0.0f; // io.DisplaySize.x - splashWinSize.x;
                splashPosition.y = io.DisplaySize.y - splashSize.y;
            }

            if (!config->OverlaysUseTheme.value_or_default())
                ImGui::PopStyleColor(4);
            else
                ImGui::PopStyleColor(2);

            ImGui::PopStyleVar(2);
        }
    }
}

void MenuCommon::RenderNotifications(RenderMenuContext& ctx)
{
    auto config = ctx.config;
    auto& io = ctx.io;

    // Notifications
    const UiTargetMode uiTargetMode = getUiTargetMode();
    const bool tonemapRequired = uiTargetMode != UiTargetMode::SDR;

    float screenHeight = State::Instance().screenHeight;
    if (io.DisplaySize.y != 0)
        screenHeight = io.DisplaySize.y;

    // Map resolution height to scale, 0.5 for 480p, 2.0 for 1440p
    constexpr float slope = (2.0f - 0.5f) / (1440.f - 480.f);
    float notificationScale = 0.5f + slope * (screenHeight - 480.f);
    notificationScale = std::clamp(notificationScale, 0.5f, 2.0f);

    if (config->UseHQFont.value_or_default())
        ImGui::PushFontSize(std::round(notificationScale * fontSize));

    // No fallback font, SetWindowFontScale needs to be called after Begin()

    ImGui::RenderNotifications(ImGuiToastPos::TopCenter, notificationScale, tonemapRequired);

    if (config->UseHQFont.value_or_default())
        ImGui::PopFontSize();
}

void MenuCommon::UpdateFrameTimeAverages(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& frameTime = ctx.frameTime;
    auto& frameRate = ctx.frameRate;
    auto& frameTimesCalculated = ctx.frameTimesCalculated;
    auto& menuResScale = ctx.menuResScale;
    auto& fpsScale = ctx.fpsScale;
    auto& averageFrameTime = ctx.averageFrameTime;
    auto& averageUpscalerFT = ctx.averageUpscalerFT;

    // FPS Overlay font
    fpsScale = config->FpsScale.value_or(menuResScale);

    // Update frame time & upscaler time averages
    averageFrameTime = 0.0f;
    averageUpscalerFT = 0.0f;

    if (config->ShowFps.value_or_default() || _isVisible)
    {
        float frameCnt = 0;
        frameTime = 0;
        for (size_t i = 299; i > 199; i--)
        {
            if (state.frameTimes[i] > 0.0)
            {
                frameTime += state.frameTimes[i];
                frameCnt++;
            }
        }

        frameTime /= frameCnt;
        frameRate = 1000.0 / frameTime;
        frameTimesCalculated = true;

        float lastFT = static_cast<float>(state.frameTimes.empty() ? 0.0f : state.frameTimes.back());
        float lastUT = static_cast<float>(state.upscaleTimes.empty() ? 0.0f : state.upscaleTimes.back());
        gFrameTimes.Push(lastFT);
        gUpscalerTimes.Push(lastUT);

        averageFrameTime = gFrameTimes.Average();
        averageUpscalerFT = gUpscalerTimes.Average();
    }
}

// Labels for the comparison views.
//
void MenuCommon::RenderPerformanceOverlay(RenderMenuContext& ctx)
{
    DlssNr::RenderNrCompareTags();

    auto& state = ctx.state;
    auto config = ctx.config;
    auto& io = ctx.io;
    auto& currentFeature = ctx.currentFeature;
    auto& now = ctx.now;
    auto& frameTime = ctx.frameTime;
    auto& frameRate = ctx.frameRate;
    auto& menuResScale = ctx.menuResScale;
    auto& fpsScale = ctx.fpsScale;
    auto& averageFrameTime = ctx.averageFrameTime;
    auto& averageUpscalerFT = ctx.averageUpscalerFT;

    // If Fps overlay is visible
    if (config->ShowFps.value_or_default())
    {
        bool stylePushed = false;

        const static auto defaultStyle = ImGuiStyle();

        // Rescale the fps overlay every frame because it shares style with the main menu
        if (config->FpsScale.has_value() && config->FpsScale.value() != menuResScale)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, defaultStyle.WindowPadding * fpsScale);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, defaultStyle.FramePadding * fpsScale);
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, defaultStyle.CellPadding * fpsScale);
            ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextPadding, defaultStyle.SeparatorTextPadding * fpsScale);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, defaultStyle.ItemSpacing * fpsScale);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, defaultStyle.ItemInnerSpacing * fpsScale);
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, defaultStyle.IndentSpacing * fpsScale);

            stylePushed = true;
        }

        // Set overlay position
        ImGui::SetNextWindowPos(overlayPosition, ImGuiCond_Always);

        // Set overlay window properties
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));  // Transparent border
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0)); // Transparent frame background

        if (!config->OverlaysUseTheme.value_or_default())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, toneMapColor(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        }

        ImGui::SetNextWindowBgAlpha(config->FpsOverlayAlpha.value_or_default()); // Transparent background

        if (!config->OverlaysUseTheme.value_or_default())
        {
            ImVec4 green(0.0f, 1.0f, 0.0f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_PlotLines, toneMapColor(green));
        }

        if (ImGui::Begin("性能 Overlay", nullptr,
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav))
        {
            std::string api;
            if (IdentifyGpu::getPrimaryGpu().usesDxvk && state.api == DX11)
            {
                if (state.swapchainInteropApi == SwapchainInteropApi::None)
                    api = "DXVK";
                else
                    api = "DXVK w/Dx12";
            }
            else if (IdentifyGpu::getPrimaryGpu().usesVkd3dProton && state.api == DX12)
            {
                api = "VKD3D";
            }
            else
            {
                switch (state.swapchainApi)
                {
                case Vulkan:
                    api = "VLK";
                    break;

                case DX11:
                    api = "D3D11";
                    break;

                case DX12:
                    if (state.swapchainInteropApi == SwapchainInteropApi::Dx11wDx12)
                        api = "D3D11 w/DX12";
                    else
                        api = "D3D12";

                    break;

                default:
                    switch (state.api)
                    {
                    case Vulkan:
                        api = "VLK";
                        break;

                    case DX11:
                        api = "D3D11";
                        break;

                    case DX12:
                        api = "D3D12";
                        break;

                    default:
                        api = "???";
                        break;
                    }

                    break;
                }
            }

            if (config->UseHQFont.value_or_default())
                ImGui::PushFontSize(std::round(fpsScale * fontSize));
            else
                ImGui::SetWindowFontScale(fpsScale);

            std::string firstLine = "";
            std::string secondLine = "";
            std::string thirdLine = "";

            auto fg = state.currentFG;
            auto fgText = (fg != nullptr && fg->IsActive() && !fg->IsPaused()) ? (" (" + std::string(fg->Name()) + ")")
                                                                               : std::string();

            const int fakeFramesCount = state.dlssgDetectedInterpolationCount;
            auto formatFg = [&](std::string_view name, int maxFakeFrames)
            {
                if (fakeFramesCount > maxFakeFrames)
                    return std::format(" ({} 不支持超过 {}x)", name, maxFakeFrames);

                else if (fakeFramesCount == 0)
                    return std::format(" ({} 关闭)", name);

                return std::format(" ({} x{})", name, fakeFramesCount + 1);
            };

            const FGNvngxReplacement activeNvngxFg = state.activeFgNvngx;
            if (activeNvngxFg == FGNvngxReplacement::Arturs)
            {
                fgText = formatFg("Enabler", Nvngx_FG::getMaxFakeFramesCount());
            }
            else if (activeNvngxFg == FGNvngxReplacement::Nukems)
            {
                fgText = formatFg("Nukems", Nvngx_FG::getMaxFakeFramesCount());
            }
            else if (activeNvngxFg == FGNvngxReplacement::FFX)
            {
                fgText = formatFg("FFX", Nvngx_FG::getMaxFakeFramesCount());
            }
            else if (activeNvngxFg == FGNvngxReplacement::Combo)
            {
                fgText = formatFg("Combo", Nvngx_FG::getMaxFakeFramesCount());
            }
            else if (state.activeFgOutput == FGOutput::DLSSG && fg)
            {
                fgText = formatFg("DLSSG", fg->GetMaxInterpolationCount());
            }

            const auto overlayType = config->FpsOverlayType.value_or_default();
            const bool hasFeature = currentFeature && !currentFeature->IsFrozen();

            // Prepare Line 1
            std::string featurePart;
            std::string fpsPart;

            if (hasFeature)
            {
                const bool usesDx12CompatLayer = currentFeature->IsWithDx12();

                featurePart = StrFmt(" | %s -> %s %u.%u.%u%s", ApiUpscalerInputName(state.currentInputApiName).c_str(),
                                     currentFeature->ShortName().c_str(), currentFeature->Version().major,
                                     currentFeature->Version().minor, currentFeature->Version().patch,
                                     usesDx12CompatLayer ? " w/Dx12" : "");
            }

            if (fg != nullptr && fg->IsActive() && !fg->IsPaused())
            {
                const double baseFps = frameRate / (double) (fg->GetInterpolatedFrameCount() + 1);

                switch (overlayType)
                {
                case FpsOverlay_JustFPS:
                    fpsPart = StrFmt("%6.1f/%5.1f ", frameRate, baseFps);
                    break;

                case FpsOverlay_Simple:
                    fpsPart = StrFmt("FPS: %6.1f/%5.1f, %7.2f ms", frameRate, baseFps, frameTime);
                    break;

                default:
                    fpsPart = StrFmt("FPS: %6.1f/%5.1f, 平均: %6.1f", frameRate, baseFps, 1000.0f / averageFrameTime);
                    break;
                }
            }
            else
            {
                switch (overlayType)
                {
                case FpsOverlay_JustFPS:
                    fpsPart = StrFmt("%6.1f ", frameRate);
                    break;

                case FpsOverlay_Simple:
                    fpsPart = StrFmt("FPS: %6.1f, %7.2f ms", frameRate, frameTime);
                    break;

                default:
                    fpsPart = StrFmt("FPS: %6.1f, 平均: %6.1f", frameRate, 1000.0f / averageFrameTime);
                    break;
                }
            }

            if (overlayType == FpsOverlay_JustFPS)
                firstLine = StrFmt("%s", fpsPart.c_str());
            else
                firstLine = StrFmt("%s | %s%s%s", api.c_str(), fpsPart.c_str(), fgText.c_str(), featurePart.c_str());

            // Prepare Line 2
            if (config->FpsOverlayType.value_or_default() >= FpsOverlay_Detailed)
            {
                if (config->FpsOverlayHorizontal.value_or_default())
                {
                    ImGui::SameLine(0.0f, 0.0f);
                    ImGui::Text(" | ");
                    ImGui::SameLine(0.0f, 0.0f);
                }
                else
                {
                    ImGui::Spacing();
                }

                secondLine = StrFmt("帧时间: %7.2f ms, 平均: %7.2f ms", state.frameTimes.back(), averageFrameTime);
            }

            // Prepare Line 3
            if (config->FpsOverlayType.value_or_default() >= FpsOverlay_Full)
            {
                thirdLine =
                    StrFmt("超分耗时: %7.2f ms, 平均: %7.2f ms", state.upscaleTimes.back(), averageUpscalerFT);
            }

            ImVec2 plotSize;
            if (config->FpsOverlayHorizontal.value_or_default())
            {
                plotSize = { fpsScale * 150, fpsScale * 16 };
            }
            else
            {
                // Find the widest text width
                auto firstSize = ImGui::CalcTextSize(firstLine.c_str());
                auto secondSize = ImGui::CalcTextSize(secondLine.c_str());
                auto thirdSize = ImGui::CalcTextSize(thirdLine.c_str());
                auto textWidth = 0.0f;

                if (firstSize.x > secondSize.x)
                    textWidth = firstSize.x > thirdSize.x ? firstSize.x : thirdSize.x;
                else
                    textWidth = secondSize.x > thirdSize.x ? secondSize.x : thirdSize.x;

                auto minWidth = fpsScale * 300.0f;
                auto plotWidth = textWidth < minWidth ? minWidth : textWidth;

                plotSize = { plotWidth, fpsScale * 30 };
            }

            // Draw the overlay
            ImGui::Text(firstLine.c_str());

            if (config->FpsOverlayType.value_or_default() >= FpsOverlay_Detailed)
            {
                if (config->FpsOverlayHorizontal.value_or_default())
                {
                    ImGui::SameLine(0.0f, 0.0f);
                    ImGui::Text(" | ");
                    ImGui::SameLine(0.0f, 0.0f);
                }
                else
                {
                    ImGui::Spacing();
                }

                ImGui::Text(secondLine.c_str());
            }

            if (config->FpsOverlayType.value_or_default() >= FpsOverlay_DetailedGraph)
            {
                if (config->FpsOverlayHorizontal.value_or_default())
                    ImGui::SameLine(0.0f, 0.0f);

                // Graph of frame times
                ImGui::PlotLines(
                    "##FrameTimeGraph",
                    [](void* rb, int idx) -> float { return static_cast<RingBuffer<float, plotWidth>*>(rb)->At(idx); },
                    &gFrameTimes, plotWidth, 0, nullptr, 0.0f, 66.6f, plotSize);
            }

            if (config->FpsOverlayType.value_or_default() >= FpsOverlay_Full)
            {
                if (config->FpsOverlayHorizontal.value_or_default())
                {
                    ImGui::SameLine(0.0f, 0.0f);
                    ImGui::Text(" | ");
                    ImGui::SameLine(0.0f, 0.0f);
                }
                else
                {
                    ImGui::Spacing();
                }

                ImGui::Text(thirdLine.c_str());
            }

            if (config->FpsOverlayType.value_or_default() >= FpsOverlay_FullGraph)
            {
                if (config->FpsOverlayHorizontal.value_or_default())
                    ImGui::SameLine(0.0f, 0.0f);

                // Graph of upscaler times
                ImGui::PlotLines(
                    "##UpscalerFrameTimeGraph",
                    [](void* rb, int idx) -> float { return static_cast<RingBuffer<float, plotWidth>*>(rb)->At(idx); },
                    &gUpscalerTimes, plotWidth, 0, nullptr, 0.0f, 20.0f, plotSize);
            }

            if (config->FpsOverlayType.value_or_default() >= FpsOverlay_ReflexTimings)
            {
                constexpr auto delayBetweenPollsMs = 500;
                static auto previousPoll = 0.0;
                static bool gotData = false;

#ifdef LOW_LATENCY_INPUTS
                static TimingData timingData {};

                if (previousPoll <= 0.001 || previousPoll + delayBetweenPollsMs < now)
                {
                    gotData = InputCommon::get_timing_data(timingData);
                    previousPoll = now;
                }

                if (gotData && timingData.timeRange.has_value())
                {
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    constexpr float offsetForText = 155;

                    const auto& rangeInNs = timingData.timeRange.value().length;

                    UINT64 localFrameCount = 0;

                    if (fg != nullptr)
                        localFrameCount = fg->FrameCount();

                    ImGui::Text("FGId: %llu, RfxId: %llu", localFrameCount, state.reflexFrameId);
                    ImGui::Text("低延迟计时,整帧: %.1fms", rangeInNs / 1000.0);

                    const auto maxWidth =
                        config->FpsOverlayHorizontal.value_or_default() ? ImGui::GetWindowWidth() : plotSize.x;

                    const auto drawTiming = [&](const auto& timingOpt, const char* desc, ImVec4 color)
                    {
                        if (!timingOpt.has_value())
                            return;

                        auto toneMappedColor = State::Instance().isHdrActive ? toneMapColor(color) : color;

                        const auto& timing = timingOpt.value();
                        float duration = static_cast<float>(timing.length * rangeInNs / 1000.0);

                        ImGui::TextColored(toneMappedColor, "%-12s %4.1fms", desc, duration);

                        auto leftLimit = ImGui::GetItemRectMin().x + offsetForText * fpsScale;

                        auto start = static_cast<float>(leftLimit + (ImGui::GetItemRectMin().x + maxWidth - leftLimit) *
                                                                        timing.position);

                        auto end = static_cast<float>(start + (ImGui::GetItemRectMin().x + maxWidth - leftLimit) *
                                                                  timing.length);

                        auto pos = ImVec2(start, ImGui::GetItemRectMin().y);
                        auto size = ImVec2(end, ImGui::GetItemRectMax().y);

                        drawList->AddRectFilled(pos, size, ImGui::ColorConvertFloat4ToU32(toneMappedColor));
                    };

                    drawTiming(timingData.simulation, "模拟", ImVec4(0.768f, 0.169f, 0.169f, 1.0f));
                    drawTiming(timingData.renderSubmit, "提交", ImVec4(0.235f, 0.705f, 0.294f, 1.0f));
                    drawTiming(timingData.present, "呈现", ImVec4(1.0f, 0.88f, 0.098f, 1.0f));
                    drawTiming(timingData.driver, "驱动", ImVec4(0.263f, 0.388f, 0.847f, 1.0f));
                    drawTiming(timingData.osRenderQueue, "渲染队列", ImVec4(0.76f, 0.51f, 0.188f, 1.0f));
                    drawTiming(timingData.gpuRender, "GPU渲染", ImVec4(0.569f, 0.117f, 0.705f, 1.0f));
                }
#else
                if (previousPoll <= 0.001 || previousPoll + delayBetweenPollsMs < now)
                {
                    gotData = ReflexHooks::updateTimingData();
                    previousPoll = now;
                }

                auto& timingData = ReflexHooks::timingData;

                if (gotData && timingData[TimingType::TimeRange].has_value())
                {
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    constexpr float offsetForText = 155;

                    const auto& rangeInNs = timingData[TimingType::TimeRange].value().length;

                    UINT64 localFrameCount = 0;

                    if (fg != nullptr)
                        localFrameCount = fg->FrameCount();

                    ImGui::Text("FGId: %llu, RfxId: %llu", localFrameCount, state.reflexFrameId);
                    ImGui::Text("Reflex 计时,整帧: %.1fms", rangeInNs / 1000.0);

                    const auto maxWidth =
                        config->FpsOverlayHorizontal.value_or_default() ? ImGui::GetWindowWidth() : plotSize.x;

                    const auto drawTiming = [&](TimingType type, const char* desc, ImVec4 color)
                    {
                        if (!timingData[type].has_value())
                            return;

                        auto toneMappedColor = toneMapColor(color);

                        auto& timing = timingData[type].value();
                        float duration = static_cast<float>(timing.length * rangeInNs / 1000.0);
                        ImGui::TextColored(toneMappedColor, "%-12s %4.1fms", desc, duration);
                        auto leftLimit = ImGui::GetItemRectMin().x + offsetForText * fpsScale;
                        auto start = static_cast<float>(leftLimit + (ImGui::GetItemRectMin().x + maxWidth - leftLimit) *
                                                                        timing.position);
                        auto end = static_cast<float>(start + (ImGui::GetItemRectMin().x + maxWidth - leftLimit) *
                                                                  timing.length);
                        auto pos = ImVec2(start, ImGui::GetItemRectMin().y);
                        auto size = ImVec2(end, ImGui::GetItemRectMax().y);
                        drawList->AddRectFilled(pos, size, ImGui::ColorConvertFloat4ToU32(toneMappedColor));
                    };

                    drawTiming(TimingType::Simulation, "模拟", ImVec4(0.768f, 0.169f, 0.169f, 1.0f));
                    drawTiming(TimingType::RenderSubmit, "提交", ImVec4(0.235f, 0.705f, 0.294f, 1.0f));
                    drawTiming(TimingType::Present, "呈现", ImVec4(1.0f, 0.88f, 0.098f, 1.0f));
                    drawTiming(TimingType::Driver, "驱动", ImVec4(0.263f, 0.388f, 0.847f, 1.0f));
                    drawTiming(TimingType::OsRenderQueue, "渲染队列", ImVec4(0.76f, 0.51f, 0.188f, 1.0f));
                    drawTiming(TimingType::GpuRender, "GPU渲染", ImVec4(0.569f, 0.117f, 0.705f, 1.0f));
                }
#endif
            }
        }

        // Restore the style
        if (!config->OverlaysUseTheme.value_or_default())
            ImGui::PopStyleColor(5);
        else
            ImGui::PopStyleColor(2);

        // Get size for postioning
        overlaySize = ImGui::GetWindowSize();

        if (config->UseHQFont.value_or_default())
            ImGui::PopFontSize();

        ImGui::End();

        if (stylePushed)
            ImGui::PopStyleVar(7);

        // Left / Right
        if (config->FpsOverlayPosition.value_or_default() == FpsOverlayPos_TopLeft ||
            config->FpsOverlayPosition.value_or_default() == FpsOverlayPos_BottomLeft)
        {
            overlayPosition.x = 0;
        }
        else
        {
            overlayPosition.x = io.DisplaySize.x - overlaySize.x;
        }

        // Top / Bottom
        if (config->FpsOverlayPosition.value_or_default() == FpsOverlayPos_TopLeft ||
            config->FpsOverlayPosition.value_or_default() == FpsOverlayPos_TopRight)
        {
            overlayPosition.y = 0;
        }
        else
        {
            // Prevent overlapping with splash message
            if (!config->DisableSplash.value_or_default() && now > splashStart && now < splashLimit)
                overlayPosition.y = io.DisplaySize.y - overlaySize.y - splashSize.y;
            else
                overlayPosition.y = io.DisplaySize.y - overlaySize.y;
        }
    }
}

void MenuCommon::RenderMainMenuHeaderMessages(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;
    auto& versionStatus = ctx.versionStatus;
    auto& currentVersionText = ctx.currentVersionText;
    auto& primaryGpu = *ctx.primaryGpu;

    if (!_showMipmapCalcWindow && !_showHudlessWindow && !ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow))
        ImGui::SetWindowFocus();

    if (config->MenuScale.has_value())
    {
        _selectedScale = ((int) (menuResScale * 10.0f)) - 4;
    }
    else
    {
        _selectedScale = 0;
    }

    if (versionStatus.completed)
    {
        if (versionStatus.updateAvailable && !versionStatus.latestTag.empty())
        {
            ImGui::Spacing();
            ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.8f, 0.f, 1.f)), "有更新可用: %s (当前 %s)",
                               versionStatus.latestTag.c_str(), currentVersionText.c_str());

            if (!versionStatus.latestUrl.empty())
            {
                ImGui::SameLine();
                ImGui::TextLinkOpenURL("打开发布页面", versionStatus.latestUrl.c_str());
            }

            ImGui::Spacing();
        }
        else if (!versionStatus.error.empty())
        {
            LOG_ERROR("Version check failed: {0}", versionStatus.error);
            versionStatus.error.clear();
        }
        // Disabled error message
        // else if (!versionStatus.error.empty())
        //{
        //    ImGui::Spacing();
        //    ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.4f, 0.f, 1.f)), "%s", versionStatus.error.c_str());
        //    ImGui::Spacing();
        //}
    }

    // No active upscaler message
    if (currentFeature == nullptr || !currentFeature->IsInited())
    {
        ImGui::Spacing();

        if (config->UseHQFont.value_or_default())
            ImGui::PushFontSize(std::round(fontSize * menuResScale * 2.5f));
        else
            ImGui::SetWindowFontScale(menuResScale * 2.5f);

        if (state.nvngxExists || state.nvngxReplacement.has_value() ||
            (state.libxessExists || XeSSProxy::Module() != nullptr))
        {
            ImGui::Spacing();

            std::vector<std::string> upscalers;

            if (state.fsrHooks)
                upscalers.push_back("FSR");

            if (state.nvngxExists || state.nvngxReplacement.has_value() || primaryGpu.dlssCapable)
                upscalers.push_back("DLSS");

            if (state.libxessExists || XeSSProxy::Module() != nullptr)
                upscalers.push_back("XeSS");

            auto joined = upscalers | std::views::join_with(std::string { " or " });

            std::string joinedUpscalers(joined.begin(), joined.end());

            ImGui::Text("请在游戏选项中选择%s作为超分器并载入存档"
                        "以启用Opti设置。\n超分器在菜单中不一定生效。",
                        joinedUpscalers.c_str());

            if (config->UseHQFont.value_or_default())
                ImGui::PopFontSize();
            else
                ImGui::SetWindowFontScale(menuResScale);

            ImGui::Spacing();

            if (primaryGpu.dlssCapable)
            {
                ImGui::Text("nvngx_dlss : %s", state.NVNGX_DLSS_Path.has_value() ? "存在" : "不存在");
                ImGui::SameLine(0.0f, 16.0f);
                ImGui::Text("nvngx_dlssd : %s", state.NVNGX_DLSSD_Path.has_value() ? "存在" : "不存在");
            }
            else
            {
                ImGui::Text("nvngx.dll: %s", state.nvngxExists ? "存在" : "不存在");
                ImGui::SameLine(0.0f, 16.0f);
                ImGui::Text("nvngx replacement: %s", state.nvngxReplacement.has_value() ? "存在" : "不存在");
            }

            ImGui::Text("libxess: %s",
                        (state.libxessExists || XeSSProxy::Module() != nullptr) ? "存在" : "不存在");

            ImGui::Text("FSR Hooks: %s", state.fsrHooks ? "存在" : "不存在");
            ImGui::SameLine(0.0f, 16.0f);
            ImGui::Text("FSR 3.1: %s", FfxApiProxy::Dx12Module() != nullptr ? "存在" : "不存在");
            ImGui::SameLine(0.0f, 16.0f);
            ImGui::Text("FSR 3.1 SR: %s", FfxApiProxy::Dx12Module_SR() != nullptr ? "存在" : "不存在");
            ImGui::SameLine(0.0f, 16.0f);
            ImGui::Text("FSR 3.1 FG: %s", FfxApiProxy::Dx12Module_FG() != nullptr ? "存在" : "不存在");

            ImGui::Spacing();
        }
        else
        {
            ImGui::Spacing();
            ImGui::Text("找不到 nvngx.dll、libxess.dll 和 FSR 输入\n超分支持将不可用。");
            ImGui::Spacing();

            if (config->UseHQFont.value_or_default())
                ImGui::PopFont();
            else
                ImGui::SetWindowFontScale(menuResScale);
        }
    }
    else if (currentFeature->IsFrozen())
    {
        ImGui::Spacing();

        if (config->UseHQFont.value_or_default())
            ImGui::PushFontSize(std::round(fontSize * menuResScale * 3.0f));
        else
            ImGui::SetWindowFontScale(menuResScale * 3.0f);

        ImGui::Text("%s 已激活，但游戏当前未使用\n请进入游戏",
                    currentFeature->Name().c_str());

        if (config->UseHQFont.value_or_default())
            ImGui::PopFont();
        else
            ImGui::SetWindowFontScale(menuResScale);
    }
}

void MenuCommon::RenderActiveUpscalerSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;
    auto& primaryGpu = *ctx.primaryGpu;

    if (currentFeature != nullptr && !currentFeature->IsFrozen())
    {
        // UPSCALERS -----------------------------
        ImGui::SeparatorText("升频器");
        ShowTooltip("你选哪种升频方案?");

        GetCurrentBackendInfo(state.api, currentBackend, &currentBackendName);

        std::string spoofingText;

        ImGui::PushItemWidth(180.0f * menuResScale);

        const bool usesDlssd = currentFeature->GetUpscalerType() == Upscaler::DLSSD;
        const bool usesDx12CompatLayer = currentFeature->IsWithDx12();

        switch (state.api)
        {
        case DX11:
            ImGui::Text(primaryGpu.name.c_str());

            ImGui::Text("D3D11 %s| %s %d.%d.%d%s", primaryGpu.usesDxvk ? "(DXVK) " : "",
                        currentFeature->ShortName().c_str(), currentFeature->Version().major,
                        currentFeature->Version().minor, currentFeature->Version().patch,
                        usesDx12CompatLayer ? " w/Dx12" : "");
            ImGui::SameLine(0.0f, 6.0f);
            ImGui::Text("| 输入: %s", ApiUpscalerInputName(state.currentInputApiName).c_str());

            ImGui::SameLine(0.0f, 6.0f);
            spoofingText = config->DxgiSpoofing.value_or_default() ? "开" : "关";
            ImGui::Text("| 欺骗: %s", spoofingText.c_str());

            if (!usesDlssd)
                AddDx11Backends(currentBackend);

            break;

        case DX12:
            ImGui::Text(primaryGpu.name.c_str());

            ImGui::Text("D3D12 %s| %s %d.%d.%d", primaryGpu.usesDxvk ? "(DXVK) " : "",
                        currentFeature->ShortName().c_str(), currentFeature->Version().major,
                        currentFeature->Version().minor, currentFeature->Version().patch);
            ImGui::SameLine(0.0f, 6.0f);
            ImGui::Text("| 输入: %s", ApiUpscalerInputName(state.currentInputApiName).c_str());

            ImGui::SameLine(0.0f, 6.0f);
            spoofingText = config->DxgiSpoofing.value_or_default() ? "开" : "关";
            ImGui::Text("| 欺骗: %s", spoofingText.c_str());

            if (!usesDlssd)
                AddDx12Backends(currentBackend);

            break;

        default:
            ImGui::Text(primaryGpu.name.c_str());

            ImGui::Text("Vulkan %s| %s %d.%d.%d%s", primaryGpu.usesDxvk ? "(DXVK) " : "",
                        currentFeature->ShortName().c_str(), currentFeature->Version().major,
                        currentFeature->Version().minor, currentFeature->Version().patch,
                        usesDx12CompatLayer ? " w/Dx12" : "");
            ImGui::SameLine(0.0f, 6.0f);
            ImGui::Text("| 输入: %s", ApiUpscalerInputName(state.currentInputApiName).c_str());

            auto vlkSpoof = config->VulkanSpoofing.value_or_default();
            auto vlkExtSpoof = config->VulkanExtensionSpoofing.value_or_default();

            if (vlkSpoof && vlkExtSpoof)
                spoofingText = "开 + 扩展";
            else if (vlkSpoof)
                spoofingText = "开";
            else if (vlkExtSpoof)
                spoofingText = "仅扩展";
            else
                spoofingText = "关";

            ImGui::SameLine(0.0f, 6.0f);
            ImGui::Text("| 欺骗: %s", spoofingText.c_str());

            if (!usesDlssd)
                AddVulkanBackends(currentBackend);
        }

        ImGui::PopItemWidth();

        if (!usesDlssd)
        {
            ImGui::SameLine(0.0f, 6.0f);

            if (ImGui::Button("切换升频器##2") && state.newBackend != Upscaler::Reset &&
                state.newBackend != currentBackend)
            {
                if (state.newBackend == Upscaler::XeSS)
                {
                    // Reseting them for xess
                    config->DisableReactiveMask.reset();
                    config->DlssReactiveMaskBias.reset();
                }

                MARK_ALL_BACKENDS_CHANGED();
            }
        }

        if (currentFeature->AccessToReactiveMask())
        {
            ImGui::BeginDisabled(config->DisableReactiveMask.value_or(false));

            auto useAsTransparency = config->FsrUseMaskForTransparency.value_or_default();
            if (ImGui::Checkbox("将 Reactive Mask 用作 Transparency Mask", &useAsTransparency))
                config->FsrUseMaskForTransparency = useAsTransparency;

            ImGui::EndDisabled();
        }

        if (primaryGpu.dlssCapable && !state.NVNGX_DLSS_Path.has_value())
        {
            ImGui::Spacing();
            ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.8f, 0.f, 1.f)), "未找到 nvngx_dlss.dll,已禁用 DLSS!");
        }
    }

    if (currentFeature != nullptr && !currentFeature->IsFrozen())
    {
        const bool usesDlssd = currentFeature->GetUpscalerType() == Upscaler::DLSSD;

        // Dx11 with Dx12
        if (state.api == DX11 && currentFeature->IsWithDx12())
        {
            ImGui::Spacing();
            if (auto ch = ScopedCollapsingHeader("Dx11 与 Dx12 设置"); ch.IsHeaderOpen())
            {
                ScopedIndent indent {};
                ImGui::Spacing();

                if (bool dontUseNTShared = config->DontUseNTShared.value_or_default();
                    ImGui::Checkbox("不使用 NTShared", &dontUseNTShared))
                    config->DontUseNTShared = dontUseNTShared;

                ImGui::Spacing();
                ImGui::Spacing();
            }
        }

        if (state.api == Vulkan && currentFeature->IsWithDx12())
        {
            ImGui::Spacing();
            if (auto ch = ScopedCollapsingHeader("Vulkan 与 Dx12 设置"); ch.IsHeaderOpen())
            {
                ScopedIndent indent {};
                ImGui::Spacing();

                if (bool inputsUseCopy = config->VulkanUseCopyForInputs.value_or_default();
                    ImGui::Checkbox("输入使用 CopyResource", &inputsUseCopy))
                    config->VulkanUseCopyForInputs = inputsUseCopy;

                if (bool outputUseCopy = config->VulkanUseCopyForOutput.value_or_default();
                    ImGui::Checkbox("输出使用 CopyResource", &outputUseCopy))
                    config->VulkanUseCopyForOutput = outputUseCopy;

                ImGui::Spacing();
                ImGui::Spacing();
            }
        }

        // UPSCALER SPECIFIC -----------------------------

        // XeSS -----------------------------
        if (currentBackend == Upscaler::XeSS && !usesDlssd)
        {
            ImGui::Spacing();
            if (auto ch = ScopedCollapsingHeader("XeSS 设置"); ch.IsHeaderOpen())
            {
                ScopedIndent indent {};
                ImGui::Spacing();

                const char* models[] = { "KPSS", "SPLAT", "MODEL_3", "MODEL_4", "MODEL_5", "MODEL_6" };
                auto configModes = config->NetworkModel.value_or_default();

                if (configModes < 0 || configModes > 5)
                    configModes = 0;

                const char* selectedModel = models[configModes];

                if (ImGui::BeginCombo("网络模型", selectedModel))
                {
                    for (int n = 0; n < 6; n++)
                    {
                        if (ImGui::Selectable(models[n], (config->NetworkModel.value_or_default() == n)))
                        {
                            config->NetworkModel = n;
                            state.newBackend = currentBackend;
                            MARK_ALL_BACKENDS_CHANGED();
                        }
                    }

                    ImGui::EndCombo();
                }
                ShowHelpMarker("可能作用不大");

                if (bool dbg = state.xessDebug; ImGui::Checkbox("转储 (Shift+Del)", &dbg))
                    state.xessDebug = dbg;

                ImGui::SameLine(0.0f, 6.0f);
                int dbgCount = state.xessDebugFrames;

                ImGui::PushItemWidth(95.0f * menuResScale);
                if (ImGui::InputInt("帧数", &dbgCount))
                {
                    if (dbgCount < 4)
                        dbgCount = 4;
                    else if (dbgCount > 999)
                        dbgCount = 999;

                    state.xessDebugFrames = dbgCount;
                }

                ImGui::PopItemWidth();

                ImGui::Spacing();
                ImGui::Spacing();
            }
        }

        // FFX -----------------
        if (!usesDlssd && (currentBackend == Upscaler::FFX || currentBackend == Upscaler::FFX_on12))
        {
            ImGui::SeparatorText("FFX 设置");

            if (_ffxUpscalerIndex < 0)
                _ffxUpscalerIndex = config->FfxUpscalerIndex.value_or_default();

            if (currentBackend == Upscaler::FFX ||
                currentBackend == Upscaler::FFX_on12 && state.ffxUpscalerVersionNames.size() > 0)
            {
                ImGui::PushItemWidth(135.0f * menuResScale);

                auto currentName = StrFmt("FSR %s", state.ffxUpscalerVersionNames[_ffxUpscalerIndex]);
                if (ImGui::BeginCombo("FFX 升频器", currentName.c_str()))
                {
                    for (int n = 0; n < state.ffxUpscalerVersionIds.size(); n++)
                    {
                        auto name = StrFmt("FSR %s##%d", state.ffxUpscalerVersionNames[n], n);
                        if (ImGui::Selectable(name.c_str(), config->FfxUpscalerIndex.value_or_default() == n))
                            _ffxUpscalerIndex = n;
                    }

                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();

                ShowHelpMarker("FFX SDK 上报的升频器列表");

                ImGui::SameLine(0.0f, 6.0f);

                if (ImGui::Button("切换升频器") &&
                    _ffxUpscalerIndex != config->FfxUpscalerIndex.value_or_default())
                {
                    config->FfxUpscalerIndex = _ffxUpscalerIndex;
                    state.newBackend = currentBackend;
                    MARK_ALL_BACKENDS_CHANGED();
                }

                auto majorFsrVersion = currentFeature->Version().major;

                if (majorFsrVersion >= 4)
                {
                    ImGui::Spacing();

                    // Colorspaces
                    const char* colorSpaces[] = { "线性 (默认)", "非线性", "非线性 sRGB",
                                                  "非线性 PQ" };
                    int currentColorSpace = 0;
                    if (config->FsrNonLinearPQ.value_or_default())
                        currentColorSpace = 3;
                    else if (config->FsrNonLinearSRGB.value_or_default())
                        currentColorSpace = 2;
                    else if (config->FsrNonLinearColorSpace.value_or_default())
                        currentColorSpace = 1;

                    ImGui::SetNextItemWidth(150.0f * menuResScale);
                    if (ImGui::Combo("输入色彩空间", &currentColorSpace, colorSpaces, IM_ARRAYSIZE(colorSpaces)))
                    {
                        bool isSrgb = (currentColorSpace == 2);
                        bool isPq = (currentColorSpace == 3);

                        config->FsrNonLinearSRGB = isSrgb;
                        config->FsrNonLinearPQ = isPq;

                        if (isSrgb || isPq)
                        {
                            config->FsrNonLinearColorSpace.set_volatile_value(true);
                        }
                        else if (currentColorSpace == 1) // Just non-Linear
                        {
                            config->FsrNonLinearColorSpace = true;
                        }
                        else // Linear
                        {
                            config->FsrNonLinearColorSpace = false;
                        }

                        state.newBackend = currentBackend;
                        MARK_ALL_BACKENDS_CHANGED();
                    }
                    ShowHelpMarker("选择游戏使用的输入色彩空间。\n"
                                   "非线性 / sRGB: 可能提升 FSR4 升频质量, 可能增加重影。\n"
                                   "PQ: 最少见, 可能增加重影并破坏灯光。");

                    // FSR 4 Presets
                    const char* presets[] = { "默认",  "预设 0", "预设 1", "预设 2",
                                              "预设 3", "预设 4", "预设 5" };
                    int currentPresetIdx = config->Fsr4Preset.has_value() ? config->Fsr4Preset.value() + 1 : 0;

                    if (currentPresetIdx < 0 || currentPresetIdx >= IM_ARRAYSIZE(presets))
                        currentPresetIdx = 0;

                    ImGui::SetNextItemWidth(150.0f * menuResScale);
                    if (ImGui::Combo("FSR4 预设", &currentPresetIdx, presets, IM_ARRAYSIZE(presets)))
                    {
                        if (currentPresetIdx == 0)
                            config->Fsr4Preset.reset();
                        else
                            config->Fsr4Preset = currentPresetIdx - 1;

                        state.newBackend = currentBackend;
                        MARK_ALL_BACKENDS_CHANGED();
                    }
                    ShowHelpMarker("每个内置 FSR4 预设针对特定分辨率调优。\n"
                                   "选择 FSR4 预设不会改变游戏内\n升频器预设!!!\n\n"
                                   "预设 0 用于 FSR 原生 AA\n"
                                   "预设 1 用于 质量/超级质量\n"
                                   "预设 2 用于 均衡\n"
                                   "预设 3 用于 性能\n"
                                   "预设 4 用于 DRS\n"
                                   "预设 5 用于 超级性能");

                    // Display the active preset right next to the combo box instead of using a table
                    ImGui::SameLine();
                    if (state.currentFsr4Preset.has_value())
                        ImGui::TextDisabled("(当前: %d)", state.currentFsr4Preset.value());
                    else if (FSR4ModelSelection::IsInt8FsrHooked())
                        ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.8f, 0.f, 1.f)), "(可能是 FSR3 回退)");
                    else
                        ImGui::TextDisabled("(挂钩失败)");
                }

                if (majorFsrVersion >= 3)
                {
                    ImGui::Spacing();

                    bool debugView = config->FsrDebugView.value_or_default();
                    if (ImGui::Checkbox("升频器调试视图", &debugView))
                    {
                        config->FsrDebugView = debugView;

                        // FSR 4's debug view requires backend reinit
                        if (majorFsrVersion > 3)
                        {
                            state.newBackend = currentBackend;
                            MARK_ALL_BACKENDS_CHANGED();
                        }
                    }

                    if (majorFsrVersion > 3)
                    {
                        ShowHelpMarker("左上: 扩张运动矢量\n"
                                       "右上: 预测混合因子");
                    }
                    else
                    {
                        ShowHelpMarker("左上: 扩张运动矢量\n"
                                       "中上: 保护区域\n"
                                       "右上: 扩张深度\n"
                                       "中间: 升频后帧\n"
                                       "左下: 去遮挡掩膜\n"
                                       "中下: 响应性\n"
                                       "右下: 细节保护衰减");
                    }

                    if (majorFsrVersion > 3)
                    {
                        ImGui::SameLine(0.0f, 20.0f * menuResScale);
                        bool fsr4wm = config->Fsr4EnableWatermark.value_or_default();
                        if (ImGui::Checkbox("水印", &fsr4wm))
                        {
                            LOG_DEBUG("FSR4 Watermark set to {}", fsr4wm);
                            config->Fsr4EnableWatermark = fsr4wm;
                        }

                        ShowHelpMarker("修改此选项后, 请保存设置。\n"
                                       "将在下次启动时生效。");
                    }
                }

                if (currentFeature->Version() >= feature_version { 3, 1, 1 } &&
                    currentFeature->Version() < feature_version { 4, 0, 0 })
                {
                    ImGui::Spacing();

                    if (currentFeature != nullptr)
                    {
                        ImGui::Text("FSR 3.1 预设:");

                        ImGui::SameLine(0.0f, 6.0f);

                        // This will be applied by default
                        if (ImGui::Button("稳定性"))
                        {
                            auto const scaleRatioX =
                                (float) currentFeature->TargetWidth() / (float) currentFeature->RenderWidth();
                            auto const scaleRatioY =
                                (float) currentFeature->TargetHeight() / (float) currentFeature->RenderHeight();
                            auto const scaleRatio = std::max(scaleRatioX, scaleRatioY);

                            config->FsrVelocity = 0.5f;
                            config->FsrReactiveScale = 0.25f;

                            config->FsrShadingScale.reset();
                            config->FsrAccAddPerFrame.reset();
                            config->FsrMinDisOccAcc.reset();
                            config->FsrShadingScale.set_volatile_value(0.5f / scaleRatio);
                            config->FsrAccAddPerFrame.set_volatile_value(scaleRatio / 10.0f);
                            config->FsrMinDisOccAcc.set_volatile_value(scaleRatio / 20.0f);
                        }

                        ImGui::SameLine(0.0f, 6.0f);

                        if (ImGui::Button("运动"))
                        {
                            auto const scaleRatioX =
                                (float) currentFeature->TargetWidth() / (float) currentFeature->RenderWidth();
                            auto const scaleRatioY =
                                (float) currentFeature->TargetHeight() / (float) currentFeature->RenderHeight();
                            auto const scaleRatio = std::max(scaleRatioX, scaleRatioY);

                            config->FsrVelocity = 1.0f;
                            config->FsrReactiveScale = 0.5f;

                            config->FsrShadingScale.reset();
                            config->FsrAccAddPerFrame.reset();
                            config->FsrMinDisOccAcc.reset();
                            config->FsrShadingScale.set_volatile_value(1.0f / scaleRatio);
                            config->FsrAccAddPerFrame.set_volatile_value(scaleRatio / 10.0f);
                            config->FsrMinDisOccAcc.set_volatile_value(scaleRatio / 20.0f);
                        }

                        ImGui::SameLine(0.0f, 6.0f);

                        if (ImGui::Button("默认"))
                        {
                            config->FsrVelocity = 1.0f;
                            config->FsrReactiveScale = 1.0f;
                            config->FsrShadingScale = 1.0f;
                            config->FsrAccAddPerFrame = 0.333f;
                            config->FsrMinDisOccAcc = -0.333f;
                        }
                    }

                    ImGui::Spacing();

                    if (auto ch = ScopedCollapsingHeader("FSR 3 升频器手动调优"); ch.IsHeaderOpen())
                    {
                        ScopedIndent indent {};
                        ImGui::Spacing();
                        ImGui::Spacing();

                        ImGui::PushItemWidth(220.0f * menuResScale);

                        float velocity = config->FsrVelocity.value_or_default();
                        if (ImGui::SliderFloat("速度因子", &velocity, 0.00f, 1.0f, "%.2f"))
                            config->FsrVelocity = velocity;

                        ShowHelpMarker("0.0 可改善亮像素时间稳定性\n"
                                       "值越低越稳定但重影越多\n"
                                       "值越高像素感越强但重影越少");

                        if (currentFeature->Version() >= feature_version { 3, 1, 4 })
                        {
                            // Reactive Scale
                            float reactiveScale = config->FsrReactiveScale.value_or_default();
                            if (ImGui::SliderFloat("响应缩放", &reactiveScale, 0.0f, 1.0f, "%.3f"))
                                config->FsrReactiveScale = reactiveScale;

                            ShowHelpMarker("用于开发测试, 向响应掩膜写入更大值\n"
                                           "是否减少重影。");

                            // Shading Scale
                            float shadingScale = config->FsrShadingScale.value_or_default();
                            if (ImGui::SliderFloat("着色缩放", &shadingScale, 0.0f, 1.0f, "%.3f"))
                                config->FsrShadingScale = shadingScale;

                            ShowHelpMarker("增大此值会缩放 FSR3.1 计算的着色\n"
                                           "变化值, 读取时获得更高响应性。");

                            // Accumulation Added Per Frame
                            float accAddPerFrame = config->FsrAccAddPerFrame.value_or_default();
                            if (ImGui::SliderFloat("每帧累积增量", &accAddPerFrame, 0.0f, 1.0f, "%.3f"))
                                config->FsrAccAddPerFrame = accAddPerFrame;

                            ShowHelpMarker("对应去遮挡发生处或响应掩膜值 > 0.0 时\n"
                                           "每帧累积量。减小此值并将重影物体\n"
                                           "(如无运动矢量) 以接近 1.0 写入响应掩膜\n"
                                           "可减少时间重影。\n"
                                           "减小此值可能导致细小特征像素闪烁增多。");

                            // Min Disocclusion Accumulation
                            float minDisOccAcc = config->FsrMinDisOccAcc.value_or_default();
                            if (ImGui::SliderFloat("最小去遮挡累积", &minDisOccAcc, -1.0f, 1.0f, "%.3f"))
                                config->FsrMinDisOccAcc = minDisOccAcc;

                            ShowHelpMarker("增大此值可减少相互遮挡的摆动细小物体\n"
                                           "周围白色像素时间闪烁。值过高可能增加重影。");
                        }

                        ImGui::PopItemWidth();

                        ImGui::Spacing();
                        ImGui::Spacing();
                    }
                }
            }
        }

        // DLSS -----------------
        if ((config->DLSSEnabled.value_or_default() && currentBackend == Upscaler::DLSS &&
             currentFeature->Version().major > 2) ||
            usesDlssd)
        {

            if (usesDlssd)
                ImGui::SeparatorText("DLSSD 设置");
            else
                ImGui::SeparatorText("DLSS 设置");

            auto overridden =
                usesDlssd ? state.dlssdPresetsOverriddenExternally : state.dlssPresetsOverriddenExternally;

            if (overridden)
            {
                ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.8f, 0.f, 1.f)), "预设被外部覆盖");
                ShowHelpMarker("通常因使用如下工具导致\n"
                               "如 Nvidia App 或 Nvidia Inspector");
                // ImGui::Text("Selecting setting below will disable that external override\n"
                //             "but you need to Save Settings and restart the game");

                ImGui::Spacing();
            }

            if (usesDlssd)
            {
                if (bool pOverride = config->DLSSDRenderPresetOverride.value_or_default();
                    ImGui::Checkbox("渲染预设覆盖", &pOverride))
                    config->DLSSDRenderPresetOverride = pOverride;

                ShowHelpMarker("每个渲染预设各有优缺点\n"
                               "覆盖可能提升画质\n"
                               "启用/禁用后请按应用");
                /*
                auto currentPresetIndex = GetPresetIndex(currentFeature, true);

                if (currentPresetIndex == 0)
                    ImGui::Text("当前预设: 默认");
                else
                    ImGui::Text("当前预设: %c", 64 + currentPresetIndex);
                */

                ImGui::BeginDisabled(!config->DLSSDRenderPresetOverride.value_or_default() /*|| overridden*/);
                ImGui::PushItemWidth(135.0f * menuResScale);

                AddDLSSDRenderPreset("覆盖预设", &comboPreset);

                ImGui::PopItemWidth();
                ImGui::EndDisabled();
            }
            else
            {
                if (bool pOverride = config->RenderPresetOverride.value_or_default();
                    ImGui::Checkbox("渲染预设覆盖", &pOverride))
                    config->RenderPresetOverride = pOverride;

                ShowHelpMarker("每个渲染预设各有优缺点\n"
                               "覆盖可能提升画质\n"
                               "启用/禁用后请按应用");
                /*
                auto currentPresetIndex = GetPresetIndex(currentFeature, false);

                if (currentPresetIndex == 0)
                    ImGui::Text("当前预设: 默认");
                else
                    ImGui::Text("当前预设: %c", 64 + currentPresetIndex);
                */

                ImGui::BeginDisabled(!config->RenderPresetOverride.value_or_default() /*|| overridden*/);

                ImGui::PushItemWidth(135.0f * menuResScale);

                AddDLSSRenderPreset("覆盖预设", &comboPreset);

                ImGui::PopItemWidth();
                ImGui::EndDisabled();
            }

            ImGui::SameLine(0.0f, 6.0f);

            if (ImGui::Button("应用更改"))
            {
                LOG_DEBUG("Applying DLSS/DLSSD preset override changes, preset index: {}",
                          comboPreset.value_or_default());

                if (usesDlssd)
                {
                    config->DLSSDRenderPresetForAll = comboPreset.value_or_default();
                    state.newBackend = Upscaler::DLSSD;
                }
                else
                {
                    config->RenderPresetForAll = comboPreset.value_or_default();
                    state.newBackend = currentBackend;
                }

                MARK_ALL_BACKENDS_CHANGED();
            }

            ImGui::Spacing();

            if (auto ch = ScopedCollapsingHeader(usesDlssd ? "高级 DLSSD 设置" : "高级 DLSS 设置");
                ch.IsHeaderOpen())
            {
                ScopedIndent indent {};
                ImGui::Spacing();

                bool appIdOverride = config->UseGenericAppIdWithDlss.value_or_default();
                if (ImGui::Checkbox("DLSS 使用通用 App Id", &appIdOverride))
                    config->UseGenericAppIdWithDlss = appIdOverride;

                ShowHelpMarker("对 NGX 使用通用 AppId\n"
                               "修复某些游戏中 OptiScaler 预设覆盖不生效\n"
                               "需要重启游戏");

                ImGui::BeginDisabled(!config->RenderPresetOverride.value_or_default() || overridden);
                ImGui::Spacing();
                ImGui::PushItemWidth(135.0f * menuResScale);

                if (usesDlssd)
                {
                    AddDLSSDRenderPreset("DLAA 预设", &config->DLSSDRenderPresetDLAA);
                    AddDLSSDRenderPreset("超高质量预设", &config->DLSSDRenderPresetUltraQuality);
                    AddDLSSDRenderPreset("质量预设", &config->DLSSDRenderPresetQuality);
                    AddDLSSDRenderPreset("均衡预设", &config->DLSSDRenderPresetBalanced);
                    AddDLSSDRenderPreset("性能预设", &config->DLSSDRenderPresetPerformance);
                    AddDLSSDRenderPreset("超级性能预设", &config->DLSSDRenderPresetUltraPerformance);
                }
                else
                {
                    AddDLSSRenderPreset("DLAA 预设", &config->RenderPresetDLAA);
                    AddDLSSRenderPreset("超高质量预设", &config->RenderPresetUltraQuality);
                    AddDLSSRenderPreset("质量预设", &config->RenderPresetQuality);
                    AddDLSSRenderPreset("均衡预设", &config->RenderPresetBalanced);
                    AddDLSSRenderPreset("性能预设", &config->RenderPresetPerformance);
                    AddDLSSRenderPreset("超级性能预设", &config->RenderPresetUltraPerformance);
                }
                ImGui::PopItemWidth();
                ImGui::EndDisabled();

                ImGui::Spacing();
                ImGui::Spacing();
            }
        }
    }
}

#if defined(OPTISCALER_RTX40_MFG)
// Options for the built-in RTX 40 unlock, shown only while it is on and not overridden by another
// unlocker. Startup settings: they apply when DLSSG loads, so a change needs a restart, and the result of
// each is shown directly under it.
static void RenderAdaUnlockOptions(Config* config, const MfgUnlock::Status& status, void (*showHelp)(const char*))
{
    if (!ImGui::CollapsingHeader("RTX 40 (Ada) MFG 解锁选项"))
        return;

    ImGui::Indent();

    // Frame timing fix. Auto is resolved inline, like the RTX 20/30 "Kernel Image" combo below.
    const auto resolved = MfgUnlock::ConfiguredTemporalMethod();
    const char* options[] = { "自动 (复用 Blackwell 内核)", "复用 Blackwell 内核", "重写混合权重 (PTX)" };

    const std::string chosen = config->FGDLSSGAdaTemporalFix.value_or("Auto");
    int index = chosen == "Retarget" ? 1 : chosen == "Ptx" ? 2 : 0;

    if (ImGui::Combo("帧时序修复##ada", &index, options, 3))
    {
        const char* stored[] = { "Auto", "Retarget", "Ptx" };
        config->FGDLSSGAdaTemporalFix = std::string(stored[index]);
    }
    showHelp("2X 以上时, 每帧生成帧都可能落在两帧真实帧中点, 3X/4X\n"
             "帧数更多但运动并不会更顺滑。此选项让每帧拥有各自时间。\n"
             "自动 / 复用 Blackwell 内核: 使用 DLSSG 模块\n"
             "自带的 Blackwell 插值内核。默认选项。\n"
             "重写混合权重: 改写 Ada 内核 PTX。仅对其识别的 DLSSG 版本\n"
             "有效。仅当 3X/4X 运动未变顺滑时尝试。\n"
             "ini: [DLSSG] AdaTemporalFix。保存设置并重启以应用。");

    // The result, directly under the control (which method applied, or the specific reason it did not).
    if (!status.ModuleFound)
    {
        ImGui::TextDisabled("尚未应用: DLSSG 未加载。");
    }
    else if (status.KernelsRewritten > 0)
    {
        const bool ptx = status.TemporalAttempted == MfgUnlock::TemporalMethod::Ptx;
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "已应用: %s, %u %s",
                           ptx ? "rewrite blend weight" : "reuse Blackwell kernel", status.KernelsRewritten,
                           ptx ? "descriptor(s)" : "kernel group(s)");
    }
    else
    {
        ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.20f, 1.0f), "未应用: %s。",
                           status.TemporalDetail.empty() ? "无记录原因" : status.TemporalDetail.c_str());
    }

    if (status.ModuleFound && status.TemporalAttempted != resolved)
        ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.20f, 1.0f), "保存设置并重启以应用。");

    ImGui::Spacing();

    // Software frame pacing. Second because it is the rarer need: a freeze above 2X, not a setting every
    // unlock user wants.
    bool softwarePacing = config->FGDLSSGAdaFlipMeteringPatch.value_or_default();

    if (ImGui::Checkbox("软件帧调度 (仅 3X+ 卡死时)##ada", &softwarePacing))
        config->FGDLSSGAdaFlipMeteringPatch = softwarePacing;
    showHelp("请求超过一帧生成帧且硬件翻转计量开启时可能画面冻结。\n"
             "此选项在 Streamline DLSS-G 插件加载时在内存中修改, 改用\n"
             "软件调度。仅对其识别的代码形态生效。\n"
             "仅当 3X 或以上冻结时使用。[NvApi] DisableFlipMetering=true (仅 ini) 更温和, 请先试\n"
             "该选项。\n"
             "ini: [DLSSG] AdaFlipMeteringPatch。保存设置并重启以应用。");

    const std::string_view pacing = status.FlipMetering;

    if (pacing.empty())
    {
        if (softwarePacing)
            ImGui::TextDisabled("尚未应用: Streamline DLSS-G 插件未加载。");
    }
    else if (pacing == "patched")
    {
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "已在 %u 处应用。", status.FlipSites);
    }
    else if (pacing != "off")
    {
        ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.20f, 1.0f), "未应用: %s。", status.FlipMetering);
    }

    if (!pacing.empty() && softwarePacing != status.FlipRequested)
        ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.20f, 1.0f), "保存设置并重启以应用。");

    ImGui::Unindent();
}

// One line under "Override DLSSG Ratio". The combo says what was requested; this says whether it
// happened: what the game asked for, what was sent on after any override, and what Streamline reports it
// presented at the last check. Nothing is drawn while DLSS-G is off.
static void RenderDlssgTelemetry()
{
    const auto& telemetry = MfgUnlock::GetTelemetry();

    if (!telemetry.optionsSeen.load(std::memory_order_acquire) || !telemetry.active.load(std::memory_order_relaxed))
        return;

    if (!telemetry.stateSeen.load(std::memory_order_acquire))
    {
        ImGui::TextDisabled("DLSSG: 等待 Streamline 状态...");
        return;
    }

    const unsigned int requestedX = telemetry.requested.load(std::memory_order_relaxed) + 1;
    const unsigned int sentX = telemetry.sent.load(std::memory_order_relaxed) + 1;
    const unsigned int presented = telemetry.presented.load(std::memory_order_relaxed);
    const unsigned int result = telemetry.result.load(std::memory_order_relaxed);

    const bool agrees = result == 0 && presented == sentX;
    const ImVec4 green(0.4f, 0.9f, 0.5f, 1.0f);
    const ImVec4 amber(0.95f, 0.70f, 0.20f, 1.0f);

    ImGui::TextColored(agrees ? green : amber, "游戏请求 %uX, 已发送 %uX, Streamline 已呈现 %u (最大 %u)",
                       requestedX, sentX, presented, telemetry.maxPresented.load(std::memory_order_relaxed));

    if (result != 0)
        ImGui::TextColored(amber, "该请求的 slDLSSGSetOptions 返回 sl::Result %u。", result);

    // Only when the symptom is there: 3X or more sent, fewer presented, and nothing pacing in software.
    if (result == 0 && sentX > 2 && presented < sentX && MfgUnlock::UnlockedMax() > 0 && !MfgUnlock::SoftwarePacing())
        ImGui::TextColored(amber,
                           "若画面冻结: 请在 RTX 40 (Ada) MFG 解锁选项下尝试软件帧调度。");
}
#endif

void MenuCommon::RenderFrameGenerationSelection(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& menuResScale = ctx.menuResScale;
    auto& primaryGpu = *ctx.primaryGpu;

#if defined(OPTISCALER_RTX40_MFG)
    const bool adaEnabledForSession = MfgUnlock::EnabledForSession();
    bool adaUnlock = config->FGDLSSGAdaMfgUnlock.value_or_default();
    const bool isAda = primaryGpu.vendorId == VendorId::Nvidia &&
                       primaryGpu.nvidiaArchInfo.architecture_id == NV_GPU_ARCHITECTURE_AD100;
    ImGui::BeginDisabled(!isAda);
    if (ImGui::Checkbox("RTX 40 MFG 解锁 (需重启)", &adaUnlock))
        config->FGDLSSGAdaMfgUnlock = adaUnlock;
    ImGui::EndDisabled();
    ShowHelpMarker("实验性。保存设置并重启。需要支持的 DLSSG 运行时。"
                   "\n请勿与其他 MFG 解锁器混用。");
    if (isAda && (adaUnlock || adaEnabledForSession))
    {
        const auto status = MfgUnlock::LastStatus();
        if (adaUnlock != adaEnabledForSession)
            ImGui::TextWrapped("保存设置并重启以应用此更改。");
        else if (!status.ModuleFound)
            ImGui::TextWrapped("等待 DLSSG 加载。");
        else if (status.AdvertiseMatched && status.ValidateMatched && status.KernelsRewritten)
            ImGui::TextWrapped("DLSSG %s: RTX 40 MFG 解锁已应用。", status.SnippetVersion.c_str());
        else
            ImGui::TextWrapped("DLSSG %s: 当前运行时不支持解锁。", status.SnippetVersion.c_str());

        if (status.ModuleFound && status.PluginCeiling[0] != '\0')
            ImGui::TextWrapped("Streamline 插件上限: %s。", status.PluginCeiling);

        RenderAdaUnlockOptions(config, status, [](const char* tip) { ShowHelpMarker(tip); });
    }
#endif

    /// FG INPUTS
    static std::vector<MenuOption<FGInput>> inputOptions;
    inputOptions.clear();

    // clang-format off

    inputOptions = {
        { FGInput::NoFG, "无" },
        { FGInput::Upscaler, "OptiFG (升频器)",
            "升频器必须启用\n\n可与任何 FG 输出搭配, 但某些可能不完美\n为防止 UI 闪烁, 需要 HUD 修复" },
        { FGInput::DLSSG, "经 Streamline 的 DLSSG",
            "可与任何 FG 输出搭配\n\n需要在游戏设置中启用 DLSS-FG\n开箱支持无 HUD 输出\n\n仅限使用 Streamline 的游戏" },
        { FGInput::NvngxFG, "经 Nvngx 的 DLSSG",
            "仅限 FSR FG 变体\n\n需要在游戏设置中启用 DLSS-FG\n开箱支持无 HUD 输出\n使用 Streamline 交换链调度" },
        { FGInput::FSRFG, "FSR 3.1 FG",
            "可与任何 FG 输出搭配\n\n需要在游戏设置中启用 FSR-FG\n开箱支持无 HUD 输出" },
        { FGInput::FSRFG30, "FSR 3.0 FG",
            "可与任何 FG 输出搭配\n\n需要在游戏设置中启用 FSR-FG\n开箱支持无 HUD 输出" },
        { FGInput::XeFG, "XeFG" }
    };

    // clang-format on

    auto constexpr nvngxInputIndex = (uint32_t) FGInput::NvngxFG;

    // XeFG input requirements
    auto constexpr xefgInputIndex = (uint32_t) FGInput::XeFG;
    inputOptions[xefgInputIndex].set_disabled(true, "支持未实现, 本意指 FG 输出");

    // OptiFG requirements
    auto constexpr optiFgIndex = (uint32_t) FGInput::Upscaler;
    inputOptions[optiFgIndex].set_disabled(state.swapchainApi == API::Vulkan, "不支持的 API");

    if (!inputOptions[optiFgIndex].disabled && state.activeFgOutput == FGOutput::FSRFG && !FfxApiProxy::IsFGReady() &&
        !ffxInitTried)
    {
        ffxInitTried = true;
        FfxApiProxy::InitFfxDx12();
        inputOptions[optiFgIndex].set_disabled(!FfxApiProxy::IsFGReady(), "缺失 amd_fidelityfx_dx12.dll");
    }
    else if (!inputOptions[optiFgIndex].disabled && state.activeFgOutput == FGOutput::XeFG && !xefgInitTried &&
             XeFGProxy::Module() == nullptr)
    {
        xefgInitTried = true;
        XeFGProxy::InitXeFG();
        inputOptions[optiFgIndex].set_disabled(XeFGProxy::Module() == nullptr, "缺失 libxess_fg.dll");
    }

    // DLSSG inputs requirements
    auto constexpr dlssgInputIndex = (uint32_t) FGInput::DLSSG;
    // inputOptions[dlssgInputIndex].set_disabled(state.streamlineVersion.major == 0, "Game doesn't use streamline");
    inputOptions[dlssgInputIndex].set_disabled(state.swapchainApi == API::DX11, "不支持的 API");

    // FSRFG inputs requirements
    auto constexpr fsrfgInputIndex = (uint32_t) FGInput::FSRFG;
    inputOptions[fsrfgInputIndex].set_disabled(state.swapchainApi != API::DX12, "不支持的 API");

    // FSRFG30 inputs requirements
    auto constexpr fsrfg30InputIndex = (uint32_t) FGInput::FSRFG30;
    inputOptions[fsrfg30InputIndex].set_disabled(state.swapchainApi != API::DX12, "不支持的 API");

    if (!config->FGInput.has_value())
        config->FGInput = config->FGInput.value_or_default(); // need to have a value before combo

    /// FG OUTPUTS

    static std::vector<MenuOption<FGOutput>> outputOptions;
    outputOptions.clear();

    // clang-format off

    outputOptions = {
        { FGOutput::NoFG, "无" },
        { FGOutput::FSRFG, "FSR FG", "FSR3/4-FG, RDNA4 自动升级到 FSR4-FG\n\nFSR4-FG 有时优于/劣于 XeFG" },
        { FGOutput::DLSSG, "DLSSG", "DLSSG 输出\n可与 Nukem 等方案联用" },
        { FGOutput::XeFG, "XeFG", "XeFG - 最重, 但最通用\n\nXeFG 3 总体处理 HUD 最好\n\n若 HUD 重影请启用 UI 合成" },
    };

    // clang-format on

    // DLSSG output requirements
    auto constexpr dlssgOutputIndex = (uint32_t) FGOutput::DLSSG;
    const bool maySupportDlssg = primaryGpu.vendorId == VendorId::Nvidia;
    const bool hasDlssgReplacement =
        state.nukemsFgFileAvailable || state.artursFgFileAvailable || FfxApiProxy::IsFGReady(false);

    if (!maySupportDlssg && hasDlssgReplacement)
    {
        outputOptions[dlssgOutputIndex].tooltip =
            "无原生 DLSSG, 硬件不支持\n仅可用 Nvngx FG 替换方案";
    }

    outputOptions[dlssgOutputIndex].set_disabled(state.swapchainApi == API::Vulkan, "不支持的 API");
    outputOptions[dlssgOutputIndex].set_disabled(!maySupportDlssg && !hasDlssgReplacement,
                                                 "硬件不支持且无替换方案");

    // For that one case of DX11 DLSSG
    const auto streamlineVersion = state.streamlineVersion;
    const bool nukemsUnsupportedApi =
        state.swapchainApi == API::DX11 &&
        (streamlineVersion == feature_version { 0, 0, 0 } || streamlineVersion > feature_version { 2, 0, 1 });
    inputOptions[nvngxInputIndex].set_disabled(nukemsUnsupportedApi, "不支持的 API");

    // FSR FG output requirements
    auto constexpr fsrfgOutputIndex = (uint32_t) FGOutput::FSRFG;
    outputOptions[fsrfgOutputIndex].set_disabled(state.swapchainApi == API::Vulkan, "不支持的 API");

    // XeFG output requirements
    auto constexpr xefgOutputIndex = (uint32_t) FGOutput::XeFG;
    outputOptions[xefgOutputIndex].set_disabled(state.swapchainApi == API::Vulkan, "不支持的 API");
    // Unsupported FG input selected
    const auto currentInputIndex = (uint32_t) state.activeFgInput;
    if (config->FGInput != FGInput::NoFG && inputOptions.size() > currentInputIndex &&
        inputOptions[currentInputIndex].disabled && state.activeFgInput == config->FGInput)
    {
        LOG_WARN("Resetting FGInput to NoFG: {}", inputOptions[currentInputIndex].label);
        config->FGInput = FGInput::NoFG;

        // Changing active can be dangerous but we are talking about an unsupported mode
        // which shouldn't even actually have taken affect
        state.activeFgInput = FGInput::NoFG;
    }

    // Unsupported FG output selected
    const auto currentOutputIndex = (uint32_t) state.activeFgOutput;
    if (config->FGOutput != FGOutput::NoFG && outputOptions.size() > currentOutputIndex &&
        outputOptions[currentOutputIndex].disabled && state.activeFgOutput == config->FGOutput)
    {
        LOG_WARN("Resetting FGOutput to NoFG: {}", outputOptions[currentOutputIndex].label);
        config->FGOutput = FGOutput::NoFG;
        state.activeFgOutput = FGOutput::NoFG;
    }

    if (!config->FGOutput.has_value())
        config->FGOutput = config->FGOutput.value_or_default(); // need to have a value before combo

    /// FG NVNGX REPLACEMENT

    static std::vector<MenuOption<FGNvngxReplacement>> nvngxOptions;
    nvngxOptions.clear();

    // clang-format off

    nvngxOptions = {
        { FGNvngxReplacement::None, "无 (原生 DLSSG)", "原生 DLSSG, 用于 RTX 40xx 及以上"},
        { FGNvngxReplacement::Nukems, "Nukem", "FSR 3 FG" },
        { FGNvngxReplacement::Arturs, "Enabler", "FSR 3 MFG 模组" },
        { FGNvngxReplacement::FFX, "FSR 3/4 FG", "使用 FFX 升级的 FSR 3/4 FG\n\n"
                                                 "部分基于 Nukem, 使用 SL 交换链\n"
                                                 "相比 FSR-FG 输出可能性能与帧调度更好"},
        { FGNvngxReplacement::Combo, "FFX + Enabler", "若支持 FSR4-FG 则用此, 否则用 Enabler\n\n"
                                                      "中间假帧用 FFX, 其余用 Enabler\n\n"
                                                      "2x - FFX\n3x - Enabler\n4x - FFX + Enabler\n5x - Enabler\n6x - FFX + Enabler\n\n"
                                                      "受调度限制, 仅奇数假帧数可用 FFX"},
    };

    // clang-format on

    bool replaceFgOutputWithNvngx = false;
    bool showNvngxFgDowndown = false;

    if (config->FGInput == FGInput::NvngxFG)
    {
        config->FGOutput = FGOutput::NoFG;
        replaceFgOutputWithNvngx = true;
    }
    else if (config->FGOutput == FGOutput::DLSSG)
    {
        showNvngxFgDowndown = true;
    }

    auto constexpr fgNvngxNoneIndex = (uint32_t) FGNvngxReplacement::None;
    nvngxOptions[fgNvngxNoneIndex].set_disabled(!maySupportDlssg, "硬件不支持");

    if (replaceFgOutputWithNvngx)
    {
        nvngxOptions[fgNvngxNoneIndex].label = "无";
        nvngxOptions[fgNvngxNoneIndex].set_hidden(true);
    }

    auto constexpr fgNvngxNukemsIndex = (uint32_t) FGNvngxReplacement::Nukems;
    nvngxOptions[fgNvngxNukemsIndex].set_disabled(!state.nukemsFgFileAvailable,
                                                  "缺失 dlssg_to_fsr3_amd_is_better.dll");

    auto constexpr fgNvngxArtursIndex = (uint32_t) FGNvngxReplacement::Arturs;
    nvngxOptions[fgNvngxArtursIndex].set_disabled(!state.artursFgFileAvailable, "缺失 dlss-enabler-headless.dll");

    auto constexpr fgNvngxFfxIndex = (uint32_t) FGNvngxReplacement::FFX;
    nvngxOptions[fgNvngxFfxIndex].set_disabled(!FfxApiProxy::IsFGReady(false),
                                               "缺失 amd_fidelityfx_framegeneration_dx12.dll");

    auto constexpr fgNvngxComboIndex = (uint32_t) FGNvngxReplacement::Combo;
    nvngxOptions[fgNvngxComboIndex].set_disabled(
        !FfxApiProxy::IsFGReady(false) || !state.artursFgFileAvailable,
        "缺失 amd_fidelityfx_framegeneration_dx12.dll\n或缺失 dlss-enabler-headless.dll");

    // TODO: Automatically switch to any other option

    if (!config->FGNvngxReplacement.has_value())
        config->FGNvngxReplacement = config->FGNvngxReplacement.value_or_default(); // need to have a value before combo

    if (state.activeFgInput != FGInput::ForceXeLL)
    {
        ImGui::SeparatorText("帧生成");

        if (ImGui::BeginTable("fgSelection", 2, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();

            PopulateCombo("FG 输入", config->FGInput, inputOptions);
            ShowTooltip("用于 FG 的数据源\n"
                        "游戏原生支持的 FG");

            ImGui::TableNextColumn();

            if (replaceFgOutputWithNvngx)
            {
                // Disable None?
                PopulateCombo("FG Nvngx", config->FGNvngxReplacement, nvngxOptions);
                ShowTooltip("替代原生 DLSSG 的后端");
            }
            else
            {
                PopulateCombo("FG 输出", config->FGOutput, outputOptions);
                ShowTooltip("实际使用的 FG");
            }

            ImGui::EndTable();
        }

        // Should be on a new line
        if (showNvngxFgDowndown)
        {
            PopulateCombo("FG Nvngx 替换", config->FGNvngxReplacement, nvngxOptions);
            ShowTooltip("替代原生 DLSSG 的后端");
        }

        // Try to avoid having None selected when the gpu doesn't support DLSSG + some fallbacks
        if (!maySupportDlssg && (replaceFgOutputWithNvngx || showNvngxFgDowndown) &&
            config->FGNvngxReplacement.value_or_default() == FGNvngxReplacement::None)
        {
            if (state.nukemsFgFileAvailable)
                config->FGNvngxReplacement.set_volatile_value(FGNvngxReplacement::Nukems);

            else if (state.artursFgFileAvailable)
                config->FGNvngxReplacement.set_volatile_value(FGNvngxReplacement::Arturs);

            else if (FfxApiProxy::IsFGReady(false))
                config->FGNvngxReplacement.set_volatile_value(FGNvngxReplacement::FFX);
        }

        const bool nvngxFgChanged = (replaceFgOutputWithNvngx || showNvngxFgDowndown) &&
                                    state.activeFgNvngx != config->FGNvngxReplacement.value_or_default();
        state.fgSettingsChanged = state.activeFgOutput != config->FGOutput.value_or_default() ||
                                  state.activeFgInput != config->FGInput.value_or_default() || nvngxFgChanged;

        if (state.fgSettingsChanged)
        {
            ImGui::Spacing();
            ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.f, 0.0f, 1.f)),
                               "保存设置并重启以应用更改");
            ImGui::Spacing();
        }

        const bool dlssgInputOrOutput =
            state.activeFgOutput == FGOutput::DLSSG || state.activeFgInput == FGInput::DLSSG;

        ImGui::BeginDisabled(state.dlssgGameDMFGSupported && config->FGDLSSGOverrideForceDMFG.value_or_default());
        if (state.dlssgMfgMax.has_value() && state.dlssgMfgMax.value() >= 1 && !dlssgInputOrOutput)
        {
            auto maxInterpolationCount = state.dlssgMfgMax.value();

            if (maxInterpolationCount >= 1)
            {
                // Map config value to UI index
                int currentSet = 0;
                if (config->FGDLSSGOverrideInterpolationCount.has_value())
                {
                    currentSet = config->FGDLSSGOverrideInterpolationCount.value() + 1;
                }

                std::string currentIntCountStr;
                if (currentSet == 0)
                    currentIntCountStr = "默认";
                else if (currentSet == 1)
                    currentIntCountStr = "关";
                else
                    currentIntCountStr = std::to_string(currentSet) + "X";

                ImGui::PushItemWidth(95.0f * menuResScale);

                if (ImGui::BeginCombo("覆盖 DLSSG 倍率", currentIntCountStr.c_str()))
                {
                    for (int i = 0; i <= maxInterpolationCount + 1; i++)
                    {
                        std::string modeStr;
                        if (i == 0)
                            modeStr = "默认";
                        else if (i == 1)
                            modeStr = "关";
                        else
                            modeStr = std::to_string(i) + "X";

                        if (ImGui::Selectable(modeStr.c_str(), (currentSet == i)))
                        {
                            if (i == 0)
                            {
                                // Default, no override
                                config->FGDLSSGOverrideInterpolationCount.reset();
                            }
                            else
                            {
                                // UI index, store value
                                int framesToGenerate = i - 1;

                                LOG_DEBUG("DLSSG Interpolation Count set to: {}", framesToGenerate);
                                config->FGDLSSGOverrideInterpolationCount = framesToGenerate;
                            }

                            StreamlineHooks::updateDlssgOptions();
                        }
                    }

                    ImGui::EndCombo();
                }

                ImGui::PopItemWidth();
            }
        }

        ImGui::EndDisabled();

#if defined(OPTISCALER_RTX40_MFG)
        if (state.dlssgMfgMax.has_value() && state.dlssgMfgMax.value() >= 1 && !dlssgInputOrOutput)
            RenderDlssgTelemetry();
#endif

        if (state.dlssgGameDMFGSupported && !dlssgInputOrOutput)
        {
            ImGui::SameLine(0.0f, 16.0f);

            if (bool dynamicMFG = config->FGDLSSGOverrideForceDMFG.value_or_default();
                ImGui::Checkbox("强制动态 MFG", &dynamicMFG))
            {
                config->FGDLSSGOverrideForceDMFG = dynamicMFG;
                StreamlineHooks::updateDlssgOptions();
            }

            ImGui::BeginDisabled(state.dlssgLastSetMode != sl::DLSSGMode::eDynamic);
            static float fpsTarget = config->FGDLSSGFramerateTargetDMFG.value_or_default();
            ImGui::SliderFloat("DMFG 目标帧率", &fpsTarget, 0, 200, "%.0f");

            ShowHelpMarker("0 表示自动检测显示器刷新率");

            if (ImGui::Button("应用目标"))
            {
                config->FGDLSSGFramerateTargetDMFG = fpsTarget;
                StreamlineHooks::updateDlssgOptions();
            }

            ImGui::SameLine(0.0f, 16.0f);

            if (ImGui::Button("重置目标"))
            {
                fpsTarget = 0.0f;
                config->FGDLSSGFramerateTargetDMFG.reset();
            }

            ImGui::EndDisabled();
        }

        auto fgOutput = reinterpret_cast<IFGFeature_Dx12*>(state.currentFG);
        if (((state.activeFgOutput == FGOutput::FSRFG || state.activeFgOutput == FGOutput::XeFG ||
              state.activeFgOutput == FGOutput::DLSSG) &&
             state.activeFgInput != FGInput::NoFG && state.activeFgInput != FGInput::NvngxFG) &&
            fgOutput)
        {
            ImGui::Checkbox("显示检测到的 UI", &state.fgHudlessCompare);
            ShowHelpMarker("需要 HUDless 纹理以与最终画面对比。\n"
                           "UI 元素且仅 UI 元素应带粉色！");

            const auto isUsingUIAny = fgOutput->IsUsingUIAny();

            ImGui::BeginDisabled(!isUsingUIAny);

            if (bool drawUIOverFG = config->FGDrawUIOverFG.value_or_default();
                ImGui::Checkbox("在上层绘制 UI", &drawUIOverFG))
            {
                config->FGDrawUIOverFG = drawUIOverFG;
            }
            ShowHelpMarker("在最终画面上层绘制 UI 资源\n"
                           "若看不到 UI, 请启用此项！");

            ImGui::EndDisabled();

            ImGui::SameLine(0.0f, 16.0f);

            ImGui::BeginDisabled(!isUsingUIAny || !config->FGDrawUIOverFG.value_or_default());

            if (bool uiPremultipliedAlpha = config->FGUIPremultipliedAlpha.value_or_default();
                ImGui::Checkbox("UI 预乘 alpha", &uiPremultipliedAlpha))
            {
                config->FGUIPremultipliedAlpha = uiPremultipliedAlpha;
            }
            ShowHelpMarker("若 UI 太淡, 请关闭此选项");

            ImGui::EndDisabled();
        }

        const bool showOutputSpecificFGSettings = state.activeFgInput == FGInput::DLSSG ||
                                                  state.activeFgInput == FGInput::FSRFG ||
                                                  state.activeFgInput == FGInput::FSRFG30;

        const bool showHudCutoff = state.activeFgInput == FGInput::NvngxFG || state.activeFgOutput == FGOutput::FSRFG;

        if (showOutputSpecificFGSettings || showHudCutoff)
        {
            ImGui::Spacing();

            if (auto ch = ScopedCollapsingHeader("高级 FG 设置"); ch.IsHeaderOpen())
            {
                ScopedIndent indent {};
                ImGui::Spacing();

                if (showOutputSpecificFGSettings)
                {
                    auto fgOutput = reinterpret_cast<IFGFeature_Dx12*>(state.currentFG);
                    if (fgOutput)
                    {
                        ImGui::BeginDisabled(!fgOutput->IsActive());

                        const auto isUsingUIAny = fgOutput->IsUsingUIAny();
                        const auto isUsingHudlessAny = fgOutput->IsUsingHudlessAny();

                        bool disableUI = config->FGDisableUI.value_or_default();
                        ImGui::BeginDisabled(!isUsingUIAny && !disableUI);

                        if (ImGui::Checkbox("禁用 UI 纹理", &disableUI))
                        {
                            config->FGDisableUI = disableUI;
                            fgOutput->UpdateTarget();
                        }

                        ShowHelpMarker("当游戏发送 UI 纹理, 但你想禁用时使用");

                        ImGui::EndDisabled();

                        ImGui::SameLine(0.0f, 16.0f);

                        bool disableHudless = config->FGDisableHudless.value_or_default();
                        ImGui::BeginDisabled(!isUsingHudlessAny && !disableHudless);

                        if (ImGui::Checkbox("禁用 HUDless", &disableHudless))
                        {
                            config->FGDisableHudless = disableHudless;
                        }

                        ShowHelpMarker("当游戏发送 HUDless, 但你想禁用时使用");

                        ImGui::EndDisabled();

                        bool depthValidNow = config->FGDepthValidNow.value_or_default();
                        if (ImGui::Checkbox("深度视为 ValidNow", &depthValidNow))
                            config->FGDepthValidNow = depthValidNow;

                        ShowHelpMarker("将占用更多显存, 但 Uniscaler 需要此项\n"
                                       "可能其他游戏也需要");

                        ImGui::SameLine(0.0f, 16.0f);

                        bool velocityValidNow = config->FGVelocityValidNow.value_or_default();
                        if (ImGui::Checkbox("速度视为 ValidNow", &velocityValidNow))
                            config->FGVelocityValidNow = velocityValidNow;

                        ShowHelpMarker("将占用更多显存, 但 Uniscaler 需要此项\n"
                                       "可能其他游戏也需要");

                        bool hudlessValidNow = config->FGHudlessValidNow.value_or_default();
                        if (ImGui::Checkbox("HUDless 视为 ValidNow", &hudlessValidNow))
                            config->FGHudlessValidNow = hudlessValidNow;

                        ShowHelpMarker("将占用更多显存, 但某些游戏可能需要此项");

                        ImGui::SameLine(0.0f, 16.0f);

                        bool firstHudless = config->FGOnlyAcceptFirstHudless.value_or_default();
                        if (ImGui::Checkbox("接受首个 HUDless", &firstHudless))
                            config->FGOnlyAcceptFirstHudless = firstHudless;

                        ShowHelpMarker("若来源标记多个 HUDless, 仅用第一个");

                        if (bool skipReset = config->FGSkipReset.value_or_default();
                            ImGui::Checkbox("跳过重置", &skipReset))
                        {
                            config->FGSkipReset = skipReset;
                        }

                        ShowHelpMarker("不使用来自 FG 输入的重置信号");

                        ImGui::EndDisabled();

                        ImGui::PushItemWidth(80.0f * menuResScale);

                        auto frameAhead = config->FGAllowedFrameAhead.value_or_default();
                        if (ImGui::InputInt("预取帧数", &frameAhead, 1, 1) && frameAhead > 0 && frameAhead < 4)
                        {
                            config->FGAllowedFrameAhead = frameAhead;
                        }

                        ShowHelpMarker("FG 允许领先游戏的帧数\n"
                                       "可能防止 FG 开关切换, 但也可能引发问题");

                        ImGui::PopItemWidth();

                        ImGui::SameLine(0.0f, 16.0f);

                        const char* ftSources[] = { "输入", "Opti", "零" };
                        const char* ftSourceInfos[] = { "使用\nDLSSG 或 FSR-FG 提供的帧时间 ",
                                                        "使用 Opti 计算的帧时间",
                                                        "让 XeFG 处理帧时间" };

                        auto currentSet = (int) config->FTInput.value_or_default();
                        auto currentSourceCount = state.activeFgOutput == FGOutput::XeFG ? 3 : 2;

                        ImGui::PushItemWidth(95.0f * menuResScale);

                        if (ImGui::BeginCombo("帧时间输入", ftSources[currentSet]))
                        {
                            for (size_t i = 0; i < currentSourceCount; i++)
                            {

                                if (ImGui::Selectable(ftSources[i], currentSet == i))
                                {
                                    LOG_DEBUG("FTInput has changed {} -> {}", ftSources[currentSet], ftSources[i]);
                                    config->FTInput = (FrameTimeSource) i;
                                }

                                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                                    ImGui::SetTooltip(ftSourceInfos[i]);
                            }

                            ImGui::EndCombo();
                        }

                        ImGui::PopItemWidth();

                        ShowHelpMarker("选择帧时间来源\n"
                                       "可能改善帧调度与卡顿问题");
                    }
                }

                if (showHudCutoff)
                {
                    float fgHudCutoff = config->FGHudCutoff.value_or_default();
                    if (ImGui::SliderFloat("HUD 截断", &fgHudCutoff, 0.00f, 1.0f, "%.2f"))
                        config->FGHudCutoff = fgHudCutoff;

                    ShowHelpMarker("从 UI 截断透明以辅助插值\n"
                                   "可用显示检测到的 UI 查看差异\n0.0 为自动");
                }
            }
        }
    }
}

void MenuCommon::RenderFrameGenerationRuntimeSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;
    auto& primaryGpu = *ctx.primaryGpu;
    auto fgOutput = state.currentFG;

    const UiTargetMode uiTargetMode = getUiTargetMode();
    const bool outputIsHdr = uiTargetMode != UiTargetMode::SDR;

    // FSR FG controls
    if (state.activeFgOutput == FGOutput::FSRFG && state.activeFgInput != FGInput::NoFG &&
        state.currentFGSwapchain != nullptr)
    {
        if (state.activeFgInput != FGInput::Upscaler ||
            (currentFeature != nullptr && !currentFeature->IsFrozen()) && FfxApiProxy::IsFGReady())
        {
            ImGui::SeparatorText("帧生成 (FSR FG)");

            if (_ffxFGIndex < 0)
                _ffxFGIndex = config->FfxFGIndex.value_or_default();

            if (state.ffxFGVersionNames.size() > 0)
            {
                ImGui::PushItemWidth(135.0f * menuResScale);

                auto currentName = StrFmt("FSR %s", state.ffxFGVersionNames[_ffxFGIndex]);
                if (ImGui::BeginCombo("FFX FG", currentName.c_str()))
                {
                    for (int n = 0; n < state.ffxFGVersionIds.size(); n++)
                    {
                        auto name = StrFmt("FSR %s", state.ffxFGVersionNames[n]);
                        if (ImGui::Selectable(name.c_str(), config->FfxFGIndex.value_or_default() == n))
                            _ffxFGIndex = n;
                    }

                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();

                ShowHelpMarker("FFX SDK 上报的 FG 列表");

                ImGui::SameLine(0.0f, 6.0f);

                if (ImGui::Button("切换 FG") && _ffxFGIndex != config->FfxFGIndex.value_or_default())
                {
                    config->FfxFGIndex = _ffxFGIndex;
                    state.fgChanged = true;
                    state.scChanged = true;
                }
            }

            bool fgActive = config->FGEnabled.value_or_default();
            if (ImGui::Checkbox("启用##2", &fgActive))
            {
                config->FGEnabled = fgActive;
                LOG_DEBUG("FGEnabled set FGEnabled: {}", fgActive);

                if (config->FGEnabled.value_or_default())
                    state.fgChanged = true;
            }
            ShowHelpMarker("启用帧生成");

            bool fgAsync = config->FGAsync.value_or_default();
            if (ImGui::Checkbox("允许异步", &fgAsync))
            {
                config->FGAsync = fgAsync;

                if (config->FGEnabled.value_or_default())
                {
                    state.fgChanged = true;
                    state.scChanged = true;
                    LOG_DEBUG("Async set FGChanged");
                }
            }
            ShowHelpMarker("异步提升帧生成性能\n但可能崩溃，尤其启用HUD修复时。");

            ImGui::SameLine(0.0f, 16.0f);

            bool fgDV = config->FGDebugView.value_or_default();
            if (ImGui::Checkbox("调试视图##2", &fgDV))
            {
                config->FGDebugView = fgDV;

                if (config->FGEnabled.value_or_default())
                {
                    state.fgChanged = true;
                    LOG_DEBUG("DebugView set FGChanged");
                }
            }
            ShowHelpMarker("启用FSR3.1 FG调试视图\n\n"
                           "左上：游戏运动矢量\n"
                           "中上：GMV深度\n"
                           "右上：光流运动矢量\n"
                           "中部：仅内插帧\n"
                           "左下：去遮挡掩码\n"
                           "中下：内插源（无UI）\n"
                           "右下：HUDless资源");

            ImGui::SameLine(0.0f, 16.0f);

            if (state.currentFG && state.currentFG->Version().major > 3)
            {
                if (bool fgwm = config->FSRFGEnableWatermark.value_or_default();
                    ImGui::Checkbox("启用水印", &fgwm))
                {
                    LOG_DEBUG("FSRFGEnableWatermark set FGWatermark: {}", fgwm);
                    config->FSRFGEnableWatermark = fgwm;
                }

                ShowHelpMarker("修改此选项后, 请保存设置\n"
                               "将在下次启动时生效。");
            }

            ImGui::Spacing();

            if (auto ch = ScopedCollapsingHeader("扩展 FSR FG 设置"); ch.IsHeaderOpen())
            {
                ScopedIndent indent {};
                ImGui::Spacing();

                ImGui::Checkbox("仅显示生成帧", &state.fgOnlyGenerated);
                ShowHelpMarker("仅显示 FSR 3.1 生成帧");

                ImGui::SameLine(0.0f, 16.0f);
                auto debugResetLines = config->FGDebugResetLines.value_or_default();
                if (ImGui::Checkbox("调试重置线", &debugResetLines))
                {
                    config->FGDebugResetLines = debugResetLines;
                    LOG_DEBUG("Enabled set FGDebugLines: {}", debugResetLines);
                }
                ShowHelpMarker("启用插值跳过线绘制");

                auto debugTearLines = config->FGDebugTearLines.value_or_default();
                if (ImGui::Checkbox("调试撕裂线", &debugTearLines))
                {
                    config->FGDebugTearLines = debugTearLines;
                    LOG_DEBUG("Enabled set FGDebugLines: {}", debugTearLines);
                }
                ShowHelpMarker("启用撕裂与插值跳过线绘制");

                ImGui::SameLine(0.0f, 16.0f);
                auto debugPacingLines = config->FGDebugPacingLines.value_or_default();
                if (ImGui::Checkbox("调试调度线", &debugPacingLines))
                {
                    config->FGDebugPacingLines = debugPacingLines;
                    LOG_DEBUG("Enabled set FGDebugLines: {}", debugPacingLines);
                }
                ShowHelpMarker("启用调度线绘制");

                ImGui::Spacing();
                if (ImGui::TreeNode("FG 矩形设置"))
                {
                    ImGui::PushItemWidth(95.0f * menuResScale);
                    int rectLeft = config->FGRectLeft.value_or(0);
                    if (ImGui::InputInt("矩形左", &rectLeft))
                        config->FGRectLeft = rectLeft;

                    ImGui::SameLine(0.0f, 16.0f);
                    int rectTop = config->FGRectTop.value_or(0);
                    if (ImGui::InputInt("矩形上", &rectTop))
                        config->FGRectTop = rectTop;

                    int rectWidth = config->FGRectWidth.value_or(0);
                    if (ImGui::InputInt("矩形宽", &rectWidth))
                        config->FGRectWidth = rectWidth;

                    ImGui::SameLine(0.0f, 16.0f);
                    int rectHeight = config->FGRectHeight.value_or(0);
                    if (ImGui::InputInt("矩形高", &rectHeight))
                        config->FGRectHeight = rectHeight;

                    ImGui::PopItemWidth();
                    ShowHelpMarker("帧生成矩形,针对黑边内容调整");

                    ImGui::BeginDisabled(!config->FGRectLeft.has_value() && !config->FGRectTop.has_value() &&
                                         !config->FGRectWidth.has_value() && !config->FGRectHeight.has_value());

                    if (ImGui::Button("重置 FG 矩形"))
                    {
                        config->FGRectLeft.reset();
                        config->FGRectTop.reset();
                        config->FGRectWidth.reset();
                        config->FGRectHeight.reset();
                    }

                    ShowHelpMarker("重置帧生成矩形");

                    ImGui::EndDisabled();
                    ImGui::TreePop();
                }

                auto fg = state.currentFG;
                if (fg != nullptr && strcmp(fg->Name(), "FSR-FG") == 0 &&
                    FfxApiProxy::VersionDx12_FG() >= feature_version { 3, 1, 3 })
                {
                    ImGui::Spacing();

                    if (ImGui::TreeNode("帧调度调优"))
                    {
                        auto fptEnabled = config->FGFramePacingTuning.value_or_default();
                        if (ImGui::Checkbox("启用调优", &fptEnabled))
                        {
                            config->FGFramePacingTuning = fptEnabled;
                            state.fsrfgFramePaceTuningChanged = true;
                        }

                        ImGui::BeginDisabled(!config->FGFramePacingTuning.value_or_default());

                        ImGui::PushItemWidth(115.0f * menuResScale);
                        auto fptSafetyMargin = config->FGFPTSafetyMarginInMs.value_or_default();
                        if (ImGui::InputFloat("安全余量 (毫秒)", &fptSafetyMargin, 0.01f, 0.1f, "%.2f"))
                            config->FGFPTSafetyMarginInMs = fptSafetyMargin;
                        ShowHelpMarker("安全余量, 单位毫秒\n"
                                       "FSR 默认值: 0.1ms\n"
                                       "Opti 默认值: 0.01ms");

                        auto fptVarianceFactor = config->FGFPTVarianceFactor.value_or_default();
                        if (ImGui::SliderFloat("方差因子", &fptVarianceFactor, 0.0f, 1.0f, "%.2f"))
                            config->FGFPTVarianceFactor = fptVarianceFactor;
                        ShowHelpMarker("方差因子\n"
                                       "FSR 默认值: 0.1\n"
                                       "Opti 默认值: 0.3");
                        ImGui::PopItemWidth();

                        auto fpHybridSpin = config->FGFPTAllowHybridSpin.value_or_default();
                        if (ImGui::Checkbox("启用混合自旋", &fpHybridSpin))
                            config->FGFPTAllowHybridSpin = fpHybridSpin;
                        ShowHelpMarker("允许调度自旋锁休眠, 应降低 CPU 占用\n"
                                       "可能导致 FPS 缓慢爬升");

                        ImGui::PushItemWidth(115.0f * menuResScale);
                        auto fptHybridSpinTime = config->FGFPTHybridSpinTime.value_or_default();
                        if (ImGui::SliderInt("混合自旋时间", &fptHybridSpinTime, 0, 100))
                            config->FGFPTHybridSpinTime = fptHybridSpinTime;
                        ShowHelpMarker("若 FPTHybridSpin 为真时的自旋时长。以计时器 "
                                       "分辨率单位计量。\n"
                                       "不建议低于 2。会导致频繁超调");
                        ImGui::PopItemWidth();

                        auto fpWaitForSingleObjectOnFence =
                            config->FGFPTAllowWaitForSingleObjectOnFence.value_or_default();
                        if (ImGui::Checkbox("启用 WaitForSingleObjectOnFence", &fpWaitForSingleObjectOnFence))
                        {
                            config->FGFPTAllowWaitForSingleObjectOnFence = fpWaitForSingleObjectOnFence;
                        }
                        ShowHelpMarker("允许用 WaitForSingleObject 等待围栏值而非自旋");

                        if (ImGui::Button("应用计时更改"))
                            state.fsrfgFramePaceTuningChanged = true;

                        ImGui::EndDisabled();
                        ImGui::TreePop();
                    }
                }

                ImGui::Spacing();
                ImGui::Spacing();
            }
        }
    }

    // XeFG controls
    if (state.activeFgOutput == FGOutput::XeFG && state.activeFgInput != FGInput::NoFG &&
        state.activeFgInput != FGInput::ForceXeLL && state.currentFGSwapchain != nullptr && XeFGProxy::InitXeFG() &&
        fgOutput)
    {
        ImGui::SeparatorText("帧生成 (XeFG)");

        bool ignoreChecks = config->FGXeFGIgnoreInitChecks.value_or_default();

        bool nativeAA = false;
        if (state.activeFgInput == FGInput::Upscaler && currentFeature != nullptr)
            nativeAA = currentFeature->RenderWidth() == currentFeature->DisplayWidth();

        const bool correctMVs = fgOutput->IsLowResMV() || nativeAA ||
                                (State::Instance().gameQuirks & GameQuirk::ForceFGRenderSizeMVs) || ignoreChecks;

        if (!correctMVs || state.realExclusiveFullscreen)
        {
            config->FGEnabled.reset();
            config->FGXeFGDebugView.reset();
        }

        const bool restartNeeded = config->FGXeFGDepthInverted.value_or_default() != fgOutput->IsInvertedDepth() ||
                                   config->FGXeFGJitteredMV.value_or_default() != fgOutput->IsJitteredMVs() ||
                                   config->FGXeFGHighResMV.value_or_default() == fgOutput->IsLowResMV();

        bool cantActivate = false;
        if (restartNeeded)
        {
            ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.8f, 0.f, 1.f)),
                               "重启游戏以应用正确的 XeFG 设置！");
        }
        else
        {
            if (!correctMVs)
                ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.f, 0.f, 1.f)),
                                   "需要禁用扩张运动矢量");

            if (!ignoreChecks && state.realExclusiveFullscreen)
            {
                cantActivate = true;
                ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.f, 0.f, 1.f)), "需要无边框显示模式！");
            }

            if (!ignoreChecks && outputIsHdr)
            {
                if (state.currentSwapchainDesc.BufferDesc.Format >= DXGI_FORMAT_R32G32B32A32_TYPELESS &&
                    state.currentSwapchainDesc.BufferDesc.Format <= DXGI_FORMAT_R16G16B16A16_SINT)
                {
                    cantActivate = true;
                    ImGui::TextColored(toneMapColor(ImVec4(1.0f, 0.0f, 0.0f, 1.f)), "XeFG 仅支持 HDR10");
                }
            }
        }

        if (!correctMVs || cantActivate || ignoreChecks)
        {
            if (ImGui::Checkbox("忽略初始化检查", &ignoreChecks))
                config->FGXeFGIgnoreInitChecks = ignoreChecks;

            ShowHelpMarker("忽略 XeFG 全部预检\n"
                           "勿用此选项跳过 UE 游戏的 MV 尺寸警告！\n"
                           "可能导致崩溃与画质变差！");
        }

        ImGui::BeginDisabled(!correctMVs || cantActivate);

        bool fgActive = config->FGEnabled.value_or_default();
        if (ImGui::Checkbox("启用##3", &fgActive))
        {
            config->FGEnabled = fgActive;
            LOG_DEBUG("Enabled set FGEnabled: {}", fgActive);

            if (config->FGEnabled.value_or_default())
                state.fgChanged = true;
        }

        ShowHelpMarker("启用帧生成");

        auto maxInterpolationCount = fgOutput->GetMaxInterpolationCount();

        if (maxInterpolationCount > 1)
        {
            ImGui::SameLine(0.0f, 16.0f);

            auto currentSet = fgOutput->GetInterpolatedFrameCount() - 1;

            std::string currentIntCountStr = std::to_string(currentSet + 2) + "X";

            ImGui::PushItemWidth(95.0f * menuResScale);

            if (ImGui::BeginCombo("MFG", currentIntCountStr.c_str()))
            {
                for (int i = 0; i < maxInterpolationCount; i++)
                {
                    std::string modeStr = std::to_string(i + 2) + "X";

                    if (ImGui::Selectable(modeStr.c_str(), (currentSet == i)))
                    {
                        LOG_DEBUG("XeFG Interpolation Count set to: {}", i + 1);
                        state.fgChanged = true;
                        config->FGXeFGInterpolationCount = i + 1;
                    }
                }

                ImGui::EndCombo();
            }

            ImGui::PopItemWidth();

            ShowHelpMarker("设置 XeFG 插值数");
        }

        ImGui::SameLine(0.0f, 16.0f);
        ImGui::BeginDisabled(!fgOutput->IsUsingHudlessAny() || XeFGProxy::SetUiCompositionState() == nullptr);
        bool fgCompositeUI = config->FGXeFGUIComposition.value_or_default();
        if (ImGui::Checkbox("UI 合成", &fgCompositeUI))
            config->FGXeFGUIComposition = fgCompositeUI;

        ShowHelpMarker("禁用 HUD/UI 插值\n"
                       "回退到旧版 XeFG 2 行为\n\n"
                       "修复透明 HUD/UI 伪影");
        ImGui::EndDisabled();

        bool fgDV = config->FGXeFGDebugView.value_or_default();
        if (ImGui::Checkbox("调试视图##2", &fgDV))
        {
            config->FGXeFGDebugView = fgDV;

            if (config->FGXeFGDebugView.value_or_default())
            {
                state.fgChanged = true;
                LOG_DEBUG("DebugView set FGChanged");
            }
        }
        ShowHelpMarker("启用 XeFG 调试视图");

        ImGui::EndDisabled();

        ImGui::SameLine(0.0f, 16.0f);
        bool fgBorderless = config->FGXeFGForceBorderless.value_or_default();
        if (ImGui::Checkbox("强制无边框", &fgBorderless))
            config->FGXeFGForceBorderless = fgBorderless;

        ShowHelpMarker("强制无边框显示模式\n\n"
                       "为获得最佳效果, 将全屏\n"
                       "分辨率设为显示器分辨率\n"
                       "可能导致一些不稳定。\n\n"
                       "需重启游戏才生效！");

        // Disable this for now
        // ImGui::SameLine(0.0f, 16.0f);
        // ImGui::Checkbox("Only Generated##2", &state.fgOnlyGenerated);
        // ShowHelpMarker("Display only XeFG generated frames");

        ImGui::Spacing();
        if (auto ch = ScopedCollapsingHeader("扩展 XeFG 设置"); ch.IsHeaderOpen())
        {
            ImGui::Spacing();
            if (ImGui::TreeNode("矩形设置"))
            {
                ImGui::PushItemWidth(95.0f * menuResScale);
                int rectLeft = config->FGRectLeft.value_or(0);
                if (ImGui::InputInt("矩形左##2", &rectLeft))
                    config->FGRectLeft = rectLeft;

                ImGui::SameLine(0.0f, 16.0f);
                int rectTop = config->FGRectTop.value_or(0);
                if (ImGui::InputInt("矩形上##2", &rectTop))
                    config->FGRectTop = rectTop;

                int rectWidth = config->FGRectWidth.value_or(0);
                if (ImGui::InputInt("矩形宽##2", &rectWidth))
                    config->FGRectWidth = rectWidth;

                ImGui::SameLine(0.0f, 16.0f);
                int rectHeight = config->FGRectHeight.value_or(0);
                if (ImGui::InputInt("矩形高##2", &rectHeight))
                    config->FGRectHeight = rectHeight;

                ImGui::PopItemWidth();
                ShowHelpMarker("帧生成矩形,针对黑边内容调整##2");

                ImGui::BeginDisabled(!config->FGRectLeft.has_value() && !config->FGRectTop.has_value() &&
                                     !config->FGRectWidth.has_value() && !config->FGRectHeight.has_value());

                if (ImGui::Button("重置 FG 矩形##2"))
                {
                    config->FGRectLeft.reset();
                    config->FGRectTop.reset();
                    config->FGRectWidth.reset();
                    config->FGRectHeight.reset();
                }

                ShowHelpMarker("重置帧生成矩形##2");

                ImGui::EndDisabled();
                ImGui::TreePop();
            }

            ImGui::Spacing();
            ImGui::Spacing();
        }
    }

    // DLSSG controls
    if (state.activeFgOutput == FGOutput::DLSSG && state.activeFgInput != FGInput::NoFG &&
        state.currentFGSwapchain != nullptr && StreamlineProxy::LoadStreamline() && fgOutput)
    {
        ImGui::SeparatorText("帧生成 (DLSSG)");

        if (state.activeFgNvngx == FGNvngxReplacement::None && (state.hdrOutputActive && outputIsHdr))
        {
            if (state.currentSwapchainDesc.BufferDesc.Format >= DXGI_FORMAT_R32G32B32A32_TYPELESS &&
                state.currentSwapchainDesc.BufferDesc.Format <= DXGI_FORMAT_R16G16B16A16_SINT)
            {
                ImGui::TextColored(toneMapColor(ImVec4(1.0f, 0.0f, 0.0f, 1.f)), "DLSSG 仅支持 HDR10");
            }
        }

        ImGui::Text("当前 DLSSG 状态:");
        ImGui::SameLine();
        if (auto count = state.dlssgDetectedInterpolationCount; count > 0)
        {
            ImGui::TextColored(toneMapColor(ImVec4(0.f, 1.f, 0.25f, 1.f)), std::format("ON {}x", count + 1).c_str());
        }
        else
        {
            ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.f, 0.f, 1.f)), "关");
        }

        bool fgActive = config->FGEnabled.value_or_default();
        if (ImGui::Checkbox("启用##4", &fgActive))
        {
            config->FGEnabled = fgActive;
            LOG_DEBUG("Enabled set FGEnabled: {}", fgActive);

            if (config->FGEnabled.value_or_default())
                state.fgChanged = true;
        }

        ShowHelpMarker("启用帧生成");

        auto maxInterpolationCount = fgOutput->GetMaxInterpolationCount();

        if (maxInterpolationCount > 1)
        {
            ImGui::SameLine(0.0f, 16.0f);

            ImGui::BeginDisabled(config->FGDLSSGForceDMFG.value_or_default());

            auto currentSet = fgOutput->GetInterpolatedFrameCount() - 1;

            std::string currentIntCountStr = std::to_string(currentSet + 2) + "X";

            ImGui::PushItemWidth(95.0f * menuResScale);

            if (ImGui::BeginCombo("MFG", currentIntCountStr.c_str()))
            {
                for (int i = 0; i < maxInterpolationCount; i++)
                {
                    std::string modeStr = std::to_string(i + 2) + "X";

                    if (ImGui::Selectable(modeStr.c_str(), (currentSet == i)))
                    {
                        LOG_DEBUG("DLSSG Interpolation Count set to: {}", i + 1);
                        config->FGDLSSGInterpolationCount = i + 1;
                    }
                }

                ImGui::EndCombo();
            }

            ImGui::PopItemWidth();

            ShowHelpMarker("设置 DLSSG 插值数");

            ImGui::EndDisabled();

            if (fgOutput->GetDMFGSupport())
            {
                ImGui::SameLine(0.0f, 16.0f);

                if (bool dynamicMFG = config->FGDLSSGForceDMFG.value_or_default();
                    ImGui::Checkbox("强制动态 MFG", &dynamicMFG))
                {
                    config->FGDLSSGForceDMFG = dynamicMFG;
                }

                ImGui::BeginDisabled(!config->FGDLSSGForceDMFG.value_or_default());
                static float fpsTarget = config->FGDLSSGFramerateTargetDMFG.value_or_default();
                ImGui::SliderFloat("DMFG 目标帧率", &fpsTarget, 0, 200, "%.0f");

                ShowHelpMarker("0 表示自动检测显示器刷新率");

                if (ImGui::Button("应用目标"))
                {
                    config->FGDLSSGFramerateTargetDMFG = fpsTarget;
                }

                ImGui::SameLine(0.0f, 16.0f);

                if (ImGui::Button("重置目标"))
                {
                    fpsTarget = 0.0f;
                    config->FGDLSSGFramerateTargetDMFG.reset();
                }

                ImGui::EndDisabled();
            }
        }

        bool useGamesMarkers = config->FGDLSSGUseGamesReflexMarkers.value_or_default();
        ImGui::BeginDisabled(!ReflexHooks::gameIsSendingMarkers());
        if (ImGui::Checkbox("使用游戏 Reflex 标记", &useGamesMarkers))
        {
            config->FGDLSSGUseGamesReflexMarkers = useGamesMarkers;
            LOG_DEBUG("Changed set FGDLSSGUseGamesReflexMarkers: {}", useGamesMarkers);
        }
        ImGui::EndDisabled();
    }

    // OptiFG
    if (state.api != API::Vulkan && state.currentFGSwapchain != nullptr && state.activeFgInput == FGInput::Upscaler)
    {
        SeparatorWithHelpMarker("帧生成 (OptiFG)", "使用升频器数据做 FG");

        if (currentFeature != nullptr && !currentFeature->IsFrozen() &&
            ((state.activeFgOutput == FGOutput::FSRFG && FfxApiProxy::IsFGReady()) ||
             (state.activeFgOutput == FGOutput::XeFG && XeFGProxy::Module() != nullptr) ||
             (state.activeFgOutput == FGOutput::DLSSG && StreamlineProxy::Module() != nullptr)))
        {
            const bool dx11HudfixTracking = state.swapchainInteropApi == SwapchainInteropApi::Dx11wDx12;
            const bool hudfixTrackingSupported =
                !Config::Instance()->FGDisableHUDFix.value_or_default() &&
                (state.swapchainInteropApi == SwapchainInteropApi::None || dx11HudfixTracking);

            if (hudfixTrackingSupported)
            {
                bool fgHudfix = config->FGHUDFix.value_or_default();

                if (ImGui::Checkbox("HUD 修复", &fgHudfix))
                {
                    config->FGHUDFix = fgHudfix;
                    LOG_DEBUG("Enabled set FGHUDFix: {}", fgHudfix);
                    state.clearCapturedHudlesses = true;
                    state.fgChanged = true;
                }

                ShowHelpMarker("启用 HUD 稳定性修复,可能导致崩溃!");

                ImGui::BeginDisabled(!config->FGHUDFix.value_or_default());

                ImGui::SameLine(0.0f, 16.0f);
                ImGui::PushItemWidth(95.0f * menuResScale);
                int hudFixLimit = config->FGHUDLimit.value_or_default();
                if (ImGui::InputInt("限制", &hudFixLimit))
                {
                    if (hudFixLimit < 1)
                        hudFixLimit = 1;
                    else if (hudFixLimit > 999)
                        hudFixLimit = 999;

                    config->FGHUDLimit = hudFixLimit;
                    LOG_DEBUG("Enabled set FGHUDLimit: {}", hudFixLimit);
                }
                ShowHelpMarker("延迟 HUDless 捕获,值过高可能崩溃!");

                ImGui::SameLine(0.0f, 16.0f);
                if (ImGui::Button("重置##2"))
                    _showHudlessWindow = !_showHudlessWindow;

                ImGui::EndDisabled();

                auto hudExtended = config->FGHUDFixExtended.value_or_default();
                if (ImGui::Checkbox("扩展", &hudExtended))
                {
                    LOG_DEBUG("Enabled set FGHUDFixExtended: {}", hudExtended);
                    config->FGHUDFixExtended = hudExtended;
                }
                ShowHelpMarker("扩展格式检查以查找可能的 HUDless\n可能导致崩溃和卡顿！");
                ImGui::SameLine(0.0f, 16.0f);

                ImGui::BeginDisabled(!config->FGHUDFix.value_or_default());

                auto immediate = config->FGImmediateCapture.value_or_default();
                if (ImGui::Checkbox("立即捕获", &immediate))
                {
                    LOG_DEBUG("Enabled set FGImmediateCapture: {}", immediate);
                    config->FGImmediateCapture = immediate;
                }
                ShowHelpMarker("在着色器执行前捕获资源。\n增加 HUDless "
                               "捕获几率, 但可能捕获多余资源。");

                ImGui::PopItemWidth();

                ImGui::EndDisabled();
            }

            bool depthScale = config->FGEnableDepthScale.value_or_default();
            if (ImGui::Checkbox("缩放深度以修复 DLSS RR", &depthScale))
                config->FGEnableDepthScale = depthScale;
            ShowHelpMarker("修复 DLSS-D 错误深度输入");

            bool resourceFlip = config->FGResourceFlip.value_or_default();
            if (ImGui::Checkbox("翻转 (Unity)", &resourceFlip))
                config->FGResourceFlip = resourceFlip;
            ShowHelpMarker("翻转 Unity 游戏的速度与深度资源");

            ImGui::SameLine(0.0f, 16.0f);

            bool resourceFlipOffset = config->FGResourceFlipOffset.value_or_default();
            if (ImGui::Checkbox("翻转使用偏移", &resourceFlipOffset))
                config->FGResourceFlipOffset = resourceFlipOffset;
            ShowHelpMarker("使用高度差作为偏移");

            ImGui::Spacing();

            if (auto ch = ScopedCollapsingHeader("高级 OptiFG 设置"); ch.IsHeaderOpen())
            {
                ScopedIndent indent {};

                if (hudfixTrackingSupported)
                {
                    ImGui::Spacing();

                    auto rb = config->FGResourceBlocking.value_or_default();
                    if (ImGui::Checkbox("资源屏蔽", &rb))
                    {
                        config->FGResourceBlocking = rb;
                        LOG_DEBUG("Enabled set FGResourceBlocking: {}", rb);
                    }
                    ShowHelpMarker("屏蔽少用资源用作 HUDless \n"
                                   "以防止闪烁等问题\n\n"
                                   "HUD 修复开关将重置屏蔽列表！");

                    ImGui::SameLine(0.0f, 16.0f);

                    auto rrc = config->FGRelaxedResolutionCheck.value_or_default();
                    if (ImGui::Checkbox("宽松资源检查", &rrc))
                    {
                        config->FGRelaxedResolutionCheck = rrc;
                        LOG_DEBUG("Enabled set FGRelaxedResolutionCheck: {}", rrc);
                    }
                    ShowHelpMarker("对 HUDless 放宽 32 像素分辨率检查 \n"
                                   "帮助部分分辨率用黑边的游戏 \n"
                                   "及屏幕比例 (如 Witcher 3)");

                    ImGui::BeginDisabled(state.fgResetCapturedResources);
                    ImGui::PushItemWidth(95.0f * menuResScale);
                    if (ImGui::Checkbox("FG 创建列表", &state.fgCaptureResources))
                    {
                        if (!state.fgCaptureResources)
                            config->FGHUDLimit = 1;
                        else
                            state.fgOnlyUseCapturedResources = false;
                    }

                    ImGui::SameLine(0.0f, 16.0f);
                    if (ImGui::Checkbox("FG 使用列表", &state.fgOnlyUseCapturedResources))
                    {
                        if (state.fgCaptureResources)
                        {
                            state.fgCaptureResources = false;
                            config->FGHUDLimit = 1;
                        }
                    }

                    ImGui::SameLine(0.0f, 8.0f);
                    ImGui::Text("(%d)", state.fgCapturedResourceCount);

                    ImGui::PopItemWidth();

                    ImGui::SameLine(0.0f, 16.0f);

                    if (ImGui::Button("重置列表"))
                    {
                        LOG_DEBUG("Resetting captured resource list");

                        state.fgResetCapturedResources = true;
                        state.fgOnlyUseCapturedResources = false;
                    }

                    ImGui::EndDisabled();

                    ImGui::Spacing();
                    ImGui::Spacing();
                    if (ImGui::TreeNode("追踪设置"))
                    {
                        ImGui::BeginDisabled(dx11HudfixTracking);

                        auto ath = config->FGAlwaysTrackHeaps.value_or_default();
                        if (ImGui::Checkbox("始终追踪堆", &ath))
                        {
                            config->FGAlwaysTrackHeaps = ath;
                            LOG_DEBUG("Enabled set FGAlwaysTrackHeaps: {}", ath);
                        }
                        ImGui::EndDisabled();

                        ShowHelpMarker(dx11HudfixTracking
                                           ? "仅 D3D12; 不适用于 DX11。"
                                           : "始终追踪资源, 可能影响性能\n, 但也可能 "
                                             "修复 HUD 修复相关崩溃！");

                        auto disableRTV = config->FGHudfixDisableRTV.value_or_default();
                        if (ImGui::Checkbox("禁用 RTV 追踪", &disableRTV))
                            config->FGHudfixDisableRTV = disableRTV;
                        ShowHelpMarker("禁用 CreateRenderTargetView 追踪\n"
                                       "有助于过滤错误的 HUDless 资源");

                        ImGui::SameLine(0.0f, 16.0f);

                        auto disableSRV = config->FGHudfixDisableSRV.value_or_default();
                        if (ImGui::Checkbox("禁用 SRV 追踪", &disableSRV))
                            config->FGHudfixDisableSRV = disableSRV;
                        ShowHelpMarker("禁用 CreateShaderResourceView 追踪\n"
                                       "有助于过滤错误的 HUDless 资源");

                        auto disableUAV = config->FGHudfixDisableUAV.value_or_default();
                        if (ImGui::Checkbox("禁用 UAV 追踪", &disableUAV))
                            config->FGHudfixDisableUAV = disableUAV;
                        ShowHelpMarker("禁用 CreateUnorderedAccessView 追踪\n"
                                       "有助于过滤错误的 HUDless 资源");

                        ImGui::SameLine(0.0f, 16.0f);

                        auto disableOM = config->FGHudfixDisableOM.value_or_default();
                        if (ImGui::Checkbox("禁用 OM 追踪", &disableOM))
                            config->FGHudfixDisableOM = disableOM;
                        ShowHelpMarker("禁用 OMSetRenderTargets 追踪\n"
                                       "有助于过滤错误的 HUDless 资源");

                        auto disableSCR = config->FGHudfixDisableSCR.value_or_default();
                        if (ImGui::Checkbox("禁用 SCR 追踪", &disableSCR))
                            config->FGHudfixDisableSCR = disableSCR;
                        ShowHelpMarker("禁用 SetComputeRootDescriptorTable 追踪\n"
                                       "有助于过滤错误的 HUDless 资源");

                        ImGui::SameLine(0.0f, 16.0f);

                        auto disableSGR = config->FGHudfixDisableSGR.value_or_default();
                        if (ImGui::Checkbox("禁用 SGR 追踪", &disableSGR))
                            config->FGHudfixDisableSGR = disableSGR;
                        ShowHelpMarker("禁用 SetGraphicsRootDescriptorTable 追踪\n"
                                       "有助于过滤错误的 HUDless 资源");

                        ImGui::Spacing();

                        auto disableDI = config->FGHudfixDisableDI.value_or_default();
                        if (ImGui::Checkbox("禁用 DI 追踪", &disableDI))
                            config->FGHudfixDisableDI = disableDI;
                        ShowHelpMarker("禁用 DrawInstanced 追踪\n"
                                       "有助于过滤错误的 HUDless 资源");

                        ImGui::SameLine(0.0f, 16.0f);

                        auto disableDII = config->FGHudfixDisableDII.value_or_default();
                        if (ImGui::Checkbox("禁用 DII 追踪", &disableDII))
                            config->FGHudfixDisableDII = disableDII;
                        ShowHelpMarker("禁用 DrawIndexedInstanced 追踪\n"
                                       "有助于过滤错误的 HUDless 资源");

                        auto disableDispatch = config->FGHudfixDisableDispatch.value_or_default();
                        if (ImGui::Checkbox("禁用 Dispatch 追踪", &disableDispatch))
                            config->FGHudfixDisableDispatch = disableDispatch;
                        ShowHelpMarker("禁用 Dispatch 追踪\n"
                                       "有助于过滤错误的 HUDless 资源");

                        ImGui::TreePop();
                    }
                }

                ImGui::Spacing();
                if (ImGui::TreeNode("资源设置"))
                {
                    bool makeMVCopies = config->FGMakeMVCopy.value_or_default();
                    if (ImGui::Checkbox("FG 复制运动矢量", &makeMVCopies))
                        config->FGMakeMVCopy = makeMVCopies;
                    ShowHelpMarker("复制运动矢量以供 OptiFG 使用\n"
                                   "可防止可能出现的损坏");

                    bool makeDepthCopies = config->FGMakeDepthCopy.value_or_default();
                    if (ImGui::Checkbox("FG 复制深度", &makeDepthCopies))
                        config->FGMakeDepthCopy = makeDepthCopies;
                    ShowHelpMarker("复制深度以供 OptiFG 使用\n"
                                   "可防止可能出现的损坏");

                    ImGui::PushItemWidth(115.0f * menuResScale);
                    float depthScaleMax = config->FGDepthScaleMax.value_or_default();
                    if (ImGui::InputFloat("FG 深度缩放上限", &depthScaleMax, 10.0f, 100.0f, "%.1f"))
                        config->FGDepthScaleMax = depthScaleMax;
                    ShowHelpMarker("深度值将被除以该值");
                    ImGui::PopItemWidth();

                    ImGui::TreePop();
                }

                ImGui::Spacing();
                if (ImGui::TreeNode("同步设置"))
                {
                    bool useMutexForPresent = config->FGUseMutexForSwapchain.value_or_default();
                    if (ImGui::Checkbox("FG Present 使用互斥", &useMutexForPresent))
                        config->FGUseMutexForSwapchain = useMutexForPresent;
                    ShowHelpMarker("使用互斥防止 FG 不同步与崩溃\n"
                                   "关闭可能提升性能但降低稳定性");

                    ImGui::TreePop();
                }

                ImGui::Spacing();
                ImGui::Spacing();
            }
        }
        else if (currentFeature == nullptr || currentFeature->IsFrozen())
        {
            ImGui::Text("升频器未激活"); // Probably never will be visible
        }
        else if (state.activeFgOutput == FGOutput::FSRFG && !FfxApiProxy::IsFGReady())
        {
            ImGui::TextColored(toneMapColor({ 1.0f, 0.0f, 0.0f, 1.0f }),
                               "amd_fidelityfx_dx12.dll is missing!"); // Probably never will be visible
        }
        else if (state.activeFgOutput == FGOutput::XeFG && XeFGProxy::Module() == nullptr)
        {
            ImGui::TextColored(toneMapColor({ 1.0f, 0.0f, 0.0f, 1.0f }),
                               "libxess_fg.dll is missing!"); // Probably never will be visible
        }
    }

    const FGNvngxReplacement activeNvngxFg = state.activeFgNvngx;
    if (activeNvngxFg != FGNvngxReplacement::None)
    {
        if (activeNvngxFg == FGNvngxReplacement::Nukems)
        {
            SeparatorWithHelpMarker("帧生成 (FSR3-FG via Nukem's DLSSG)",
                                    "需要 Nukem 的 dlssg_to_fsr3 dll");

            if (!state.nukemsFgFileAvailable)
            {
                ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.f, 0.f, 1.f)),
                                   "请将 dlssg_to_fsr3_amd_is_better.dll 放入 OptiScaler 文件夹");
            }
        }
        else if (activeNvngxFg == FGNvngxReplacement::Arturs)
        {
            SeparatorWithHelpMarker("帧生成 (FSR3-MFG via DLSS Enabler)",
                                    "DLSS Enabler 对应 dlss-enabler-headless.dll");

            if (!state.artursFgFileAvailable)
            {
                ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.f, 0.f, 1.f)),
                                   "请将 dlss-enabler-headless.dll 放入 OptiScaler 文件夹");
            }

            ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.8f, 0.f, 1.f)),
                               "正在使用 DLSS Enabler 的部分功能");
        }
        else if (activeNvngxFg == FGNvngxReplacement::FFX)
        {
            SeparatorWithHelpMarker("帧生成 (FSRFG via FFX)", "FFX 使用 DLSSG 交换链");
        }
        else if (activeNvngxFg == FGNvngxReplacement::Combo)
        {
            SeparatorWithHelpMarker("帧生成 (Enabler + FFX)",
                                    "中间假帧用 FFX，其余用 Enabler\n\n2x - FFX\n"
                                    "3x - Enabler\n4x - FFX + Enabler\n5x - Enabler\n6x - FFX + Enabler");
        }

        if (state.activeFgInput == FGInput::NvngxFG)
        {

            bool dmfgActive = state.dlssgGameDMFGSupported && config->FGDLSSGOverrideForceDMFG.value_or_default();

            if (!ReflexHooks::isReflexHooked())
            {
                ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.f, 0.f, 1.f)), "Reflex 未挂钩");
                ImGui::Text("若使用 AMD/Intel 显卡，请确保已安装 Fakenvapi");
            }
            else if (ReflexHooks::dlssgFrameCountToGenerate() == 0 && !dmfgActive)
            {
                ImGui::Text("请在游戏选项中选择DLSS帧生成\n"
                            "可能需要先选择DLSS");
            }

            if (state.swapchainApi == DX12)
            {
                ImGui::Text("当前 DLSSG 状态:");
                ImGui::SameLine();
                if (auto count = state.dlssgDetectedInterpolationCount; count > 0)
                {
                    ImGui::TextColored(toneMapColor(ImVec4(0.f, 1.f, 0.25f, 1.f)),
                                       std::format("ON {}x", count + 1).c_str());
                }
                else
                {
                    ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.f, 0.f, 1.f)), "关");
                }

                // Issue mostly shows up on AMD on Windows on pre-RDNA3 in some non-UE games
                // Hide to reduce confusion, config is still read
                const bool isUnrealEngine = State::Instance().NVNGX_Engine == NVSDK_NGX_ENGINE_TYPE_UNREAL ||
                                            State::Instance().gameQuirks & GameQuirk::ForceUnrealEngine;
                const bool isDllProxyNvngxType =
                    activeNvngxFg == FGNvngxReplacement::Nukems || activeNvngxFg == FGNvngxReplacement::Arturs;
                if (isDllProxyNvngxType && !primaryGpu.dlssCapable && primaryGpu.fsr4Support == FSR4Support::None &&
                    !primaryGpu.usesVkd3dProton && !isUnrealEngine)
                {
                    if (bool makeDepthCopy = config->NvngxFGMakeDepthCopy.value_or_default();
                        ImGui::Checkbox("修复异常画面", &makeDepthCopy))
                    {
                        config->NvngxFGMakeDepthCopy = makeDepthCopy;
                    }
                    ShowHelpMarker("复制深度缓冲\n可修复 Windows 下 AMD 显卡部分游戏的异常画面\n"
                                   "可能导致卡顿，建议仅在必要时使用");
                }
            }
            else if (state.swapchainApi == Vulkan)
            {
                ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.8f, 0.f, 1.f)),
                                   "此菜单可见时 DLSSG 会被有意禁用");
                ImGui::Spacing();
            }
        }

        bool isLoaded = false;
        if (state.swapchainApi == Vulkan)
            isLoaded = Nvngx_FG::isVulkanAvailable();
        if (state.swapchainApi == DX12)
            isLoaded = Nvngx_FG::isDx12Available();

        if (isLoaded)
        {
            if (activeNvngxFg == FGNvngxReplacement::Arturs || activeNvngxFg == FGNvngxReplacement::Combo)
            {
                auto featureVer = Nvngx_FG::version();
                auto antighostingVer = Nvngx_FG::extraVersion();
                ImGui::Text("DE Ver: %d.%d.%d.%d   GB Ver: %d.%d", featureVer.major, featureVer.minor, featureVer.patch,
                            featureVer.reserved, antighostingVer.major, antighostingVer.minor);

                static std::vector<FlagDefinition> common_flags = {
                    { "抗鬼影 (GB)", 0x00100000, "启用抗鬼影校正" },
                    { "时域 HUD 固定", 0x04000000, "启用时域 HUD 固定（后缓冲稳定性）" }
                };

                static std::vector<FlagDefinition> uncommon_flags = {
                    //{ "Hudless UI mask", 0x02000000, "Use HUD-less as UI mask (DL2 inverted semantics)" },
                    { "HUD 插值", 0x08000000, "HUD 光流插值（0=旧版固定呈现，1=光流扭曲）" },
                    { "忽略 UI 纹理", 0x10000000, "忽略专用 DLSSG.UI 纹理（强制旧版 HUD 路径）" },
                    //{ "Dp4a active", 0x20000000, "OF pipeline using dp4a-accelerated SSD (SM 6.4+)" },
                    { "固定后缓冲", 0x40000000, "在 MFG 帧间将 DLSSG.Backbuffer 固定为子帧-1 快照" }
                };

                static std::vector<FlagDefinition> debug_flags = {
                    { "抗鬼影红色染色", 0x00200000, "调试：在校正像素上染红色" },
                    { "抗鬼影分屏对比", 0x00400000, "调试：分屏对比" },
                    { "帧序号线", 0x00010000, "" },
                    { "HUD 检测", 0x00020000, "" },
                    { "遮挡染色", 0x00040000, "" },
                    { "伪影检测", 0x00080000, "" },
                    { "相机运动矢量调试", 0x00800000, "调试：使用相机运动矢量兜底处染蓝色" },
                    { "通用可视化", 0x01000000, "调试：梯形区域可视化" }
                };

                uint32_t temp_flags = config->NvngxFGDispatchFlags.value_or_default();
                bool changed = false;

                ImGui::Text("原始 DispatchFlags:");
                changed |= ImGui::InputScalar("##RawFlags", ImGuiDataType_U32, &temp_flags, NULL, NULL, "%08X",
                                              ImGuiInputTextFlags_CharsHexadecimal);

                ImGui::SameLine(0.0f, 20.0f * menuResScale);
                if (bool showDebug = config->NvngxFGShowDebug.value_or_default();
                    ImGui::Checkbox("显示调试", &showDebug))
                {
                    config->NvngxFGShowDebug = showDebug;
                }
                ShowHelpMarker("调试标志正常工作所需");

                ImGui::Spacing();

                if (auto ch = ScopedCollapsingHeader("当前 DispatchFlags"); ch.IsHeaderOpen())
                {
                    ScopedIndent indent {};

                    auto render_flags = [&](const std::vector<FlagDefinition>& flags)
                    {
                        for (const auto& flag : flags)
                        {
                            changed |= ImGui::CheckboxFlags(flag.name.c_str(), &temp_flags, flag.mask);

                            if (ImGui::IsItemHovered() && !flag.description.empty())
                            {
                                ImGui::SetTooltip("%s", flag.description.c_str());
                            }
                        }
                    };

                    ImGui::TextDisabled("常用");
                    render_flags(common_flags);

                    ImGui::Spacing();
                    ImGui::TextDisabled("不常用");
                    render_flags(uncommon_flags);

                    if (config->NvngxFGShowDebug.value_or_default())
                    {
                        ImGui::Spacing();
                        ImGui::TextDisabled("调试");
                        render_flags(debug_flags);
                    }
                }

                if (changed)
                {
                    config->NvngxFGDispatchFlags = temp_flags;
                }
            }

            if (activeNvngxFg == FGNvngxReplacement::Nukems)
            {
                if (ImGui::Checkbox("启用调试视图", &state.dlssgDebugView))
                {
                    Nvngx_FG::setDebugView(state.dlssgDebugView);
                }
                if (ImGui::Checkbox("仅内插帧", &state.dlssgInterpolatedOnly))
                {
                    Nvngx_FG::setInterpolatedOnly(state.dlssgInterpolatedOnly);
                }
            }

            if (activeNvngxFg == FGNvngxReplacement::FFX || activeNvngxFg == FGNvngxReplacement::Combo)
            {
                if (_ffxFGIndex < 0)
                    _ffxFGIndex = config->FfxFGIndex.value_or_default();

                if (state.ffxFGVersionNames.size() > 0)
                {
                    ImGui::PushItemWidth(135.0f * menuResScale);

                    auto currentName = StrFmt("FSR %s", state.ffxFGVersionNames[_ffxFGIndex]);
                    if (ImGui::BeginCombo("FFX FG", currentName.c_str()))
                    {
                        for (int n = 0; n < state.ffxFGVersionIds.size(); n++)
                        {
                            auto name = StrFmt("FSR %s", state.ffxFGVersionNames[n]);
                            if (ImGui::Selectable(name.c_str(), config->FfxFGIndex.value_or_default() == n))
                                _ffxFGIndex = n;
                        }

                        ImGui::EndCombo();
                    }
                    ImGui::PopItemWidth();

                    ShowHelpMarker("FFX SDK 上报的 FG 列表");

                    ImGui::SameLine(0.0f, 6.0f);

                    if (ImGui::Button("切换 FG") && _ffxFGIndex != config->FfxFGIndex.value_or_default())
                    {
                        config->FfxFGIndex = _ffxFGIndex;
                        state.fgChanged = true;
                    }
                }

                bool fgAsync = config->FGAsync.value_or_default();
                if (ImGui::Checkbox("允许异步##2", &fgAsync))
                {
                    config->FGAsync = fgAsync;

                    if (config->FGEnabled.value_or_default())
                    {
                        state.fgChanged = true;
                        LOG_DEBUG("Async set FGChanged");
                    }
                }
                ShowHelpMarker("启用异步以提升 FG 性能\n可能导致崩溃，尤其配合 HUD 修复时！");

                ImGui::SameLine(0.0f, 20.0f * menuResScale);
                bool fgDV = config->FGDebugView.value_or_default();
                if (ImGui::Checkbox("调试视图##3", &fgDV))
                {
                    config->FGDebugView = fgDV;

                    if (config->FGEnabled.value_or_default())
                    {
                        state.fgChanged = true;
                        LOG_DEBUG("DebugView set FGChanged");
                    }
                }
                ShowHelpMarker("启用 FSR3.1-FG 调试视图\n\n"
                               "左上：游戏运动矢量\n"
                               "上中：运动矢量深度\n"
                               "右上：光流运动矢量\n"
                               "中间：仅内插帧\n"
                               "左下：遮挡掩膜\n"
                               "下中：插值源（无 UI）\n"
                               "右下：HUDless 资源");

                if (Nvngx_FG::version().major > 3)
                {
                    ImGui::SameLine(0.0f, 20.0f * menuResScale);
                    if (bool fgwm = config->FSRFGEnableWatermark.value_or_default();
                        ImGui::Checkbox("启用水印", &fgwm))
                    {
                        LOG_DEBUG("FSRFGEnableWatermark set FGWatermark: {}", fgwm);
                        config->FSRFGEnableWatermark = fgwm;
                    }

                    ShowHelpMarker("更改此选项后，请保存设置\n"
                                   "将在下次启动时生效。");
                }
            }

            if (bool disableHudless = config->NvngxFGDisableHudless.value_or_default();
                ImGui::Checkbox("禁用 HUDless", &disableHudless))
            {
                config->NvngxFGDisableHudless = disableHudless;
            }
            ShowHelpMarker("部分 DispatchFlags 组合可能需要此项");
        }
    }

    // FSR-FG Inputs
    if (state.currentFGSwapchain != nullptr &&
        (state.activeFgInput == FGInput::FSRFG || state.activeFgInput == FGInput::FSRFG30))
    {
        SeparatorWithHelpMarker("帧生成 (FSR-FG 输入)", "在游戏中选择 FSR-FG");

        auto fgOutput = reinterpret_cast<IFGFeature_Dx12*>(state.currentFG);
        if (fgOutput != nullptr)
        {
            ImGui::Text("当前 FSR-FG 状态:");
            ImGui::SameLine();
            if (state.fsrfgInputActive)
            {
                if (fgOutput->IsActive())
                    ImGui::TextColored(toneMapColor(ImVec4(0.f, 1.f, 0.25f, 1.f)), "开");
                else
                    ImGui::TextColored(toneMapColor(ImVec4(1.0f, 0.647f, 0.0f, 1.f)), "激活 FG");
            }
            else
            {
                ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.f, 0.f, 1.f)), "关");
                ImGui::Text("请在游戏选项中选择FSR帧生成\n"
                            "可能需要先选择FSR");
            }
        }

        bool skipConfig = config->FSRFGSkipConfigForHudless.value_or_default();
        if (ImGui::Checkbox("HUDless 跳过 Config", &skipConfig))
            config->FSRFGSkipConfigForHudless = skipConfig;

        ShowHelpMarker("在 ffxConfig 处不使用 HUDless 集合");

        ImGui::SameLine(0.0f, 6.0f);

        bool skipDispatch = config->FSRFGSkipDispatchForHudless.value_or_default();
        if (ImGui::Checkbox("HUDless 跳过 Dispatch", &skipDispatch))
            config->FSRFGSkipDispatchForHudless = skipDispatch;

        ShowHelpMarker("在 ffxDispatch 处不使用 HUDless 集合");
    }

    // Streamline FG Inputs
    if (state.currentFGSwapchain != nullptr && state.activeFgInput == FGInput::DLSSG)
    {
        SeparatorWithHelpMarker("帧生成 (Streamline FG 输入)", "在游戏中选择 DLSS-FG");

        auto fgOutput = reinterpret_cast<IFGFeature_Dx12*>(state.currentFG);

        if (!ReflexHooks::isReflexHooked())
        {
            ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.f, 0.f, 1.f)), "Reflex 未挂钩");
            ImGui::Text("若使用 AMD/Intel 显卡，请确保已安装 fakenvapi");
        }
        else if (fgOutput != nullptr)
        {
            ImGui::Text("当前 Streamline FG 状态:");
            ImGui::SameLine();
            if ((state.fgLastFrame - state.dlssgLastFrame) < 3)
            {
                if (fgOutput->IsActive())
                    ImGui::TextColored(toneMapColor(ImVec4(0.f, 1.f, 0.25f, 1.f)), "开");
                else
                    ImGui::TextColored(toneMapColor(ImVec4(1.0f, 0.647f, 0.0f, 1.f)), "激活 FG");
            }
            else
            {
                ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.f, 0.f, 1.f)), "关");
                ImGui::Text("请在游戏选项中选择DLSS帧生成\n"
                            "可能需要先选择DLSS");
            }
        }
    }
}

void MenuCommon::RenderFsrCommonSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;

    if (currentFeature != nullptr && !currentFeature->IsFrozen())
    {
        // FSR Common -----------------
        if (currentFeature != nullptr && !currentFeature->IsFrozen() &&
            (state.activeFgOutput == FGOutput::FSRFG || IsFsr(currentBackend)))
        {
            SeparatorWithHelpMarker("FSR 通用设置", "同时影响 FSR-FG 与升频器");

            bool useFsrVales = config->FsrUseFsrInputValues.value_or_default();
            if (ImGui::Checkbox("使用 FSR 输入值", &useFsrVales))
                config->FsrUseFsrInputValues = useFsrVales;

            ImGui::Spacing();
            if (auto ch = ScopedCollapsingHeader("视场与相机数值"); ch.IsHeaderOpen())
            {
                ScopedIndent indent {};
                ImGui::Spacing();

                bool useVFov = config->FsrVerticalFov.has_value() || !config->FsrHorizontalFov.has_value();

                float vfov = config->FsrVerticalFov.value_or_default();
                float hfov = config->FsrHorizontalFov.value_or(90.0f);

                if (useVFov && !config->FsrVerticalFov.has_value())
                    config->FsrVerticalFov = vfov;
                else if (!useVFov && !config->FsrHorizontalFov.has_value())
                    config->FsrHorizontalFov = hfov;

                if (ImGui::RadioButton("使用垂直视场", useVFov))
                {
                    config->FsrHorizontalFov.reset();
                    config->FsrVerticalFov = vfov;
                    useVFov = true;
                }

                ImGui::SameLine(0.0f, 6.0f);

                if (ImGui::RadioButton("使用水平视场", !useVFov))
                {
                    config->FsrVerticalFov.reset();
                    config->FsrHorizontalFov = hfov;
                    useVFov = false;
                }

                if (useVFov)
                {
                    if (ImGui::SliderFloat("垂直视场", &vfov, 0.0f, 180.0f, "%.1f"))
                        config->FsrVerticalFov = vfov;

                    ShowHelpMarker("可能改善画质");
                }
                else
                {
                    if (ImGui::SliderFloat("水平视场", &hfov, 0.0f, 180.0f, "%.1f"))
                        config->FsrHorizontalFov = hfov;

                    ShowHelpMarker("可能改善画质");
                }

                float cameraNear;
                float cameraFar;

                cameraNear = config->FsrCameraNear.value_or_default();
                cameraFar = config->FsrCameraFar.value_or_default();

                if (ImGui::SliderFloat("相机近裁", &cameraNear, 0.1f, 500000.0f, "%.1f"))
                    config->FsrCameraNear = cameraNear;
                ShowHelpMarker("可能改善画质\n"
                               "并可能减少鬼影");

                if (ImGui::SliderFloat("相机远裁", &cameraFar, 0.1f, 500000.0f, "%.1f"))
                    config->FsrCameraFar = cameraFar;
                ShowHelpMarker("可能改善画质\n"
                               "并可能减少鬼影");

                if (ImGui::Button("重置相机数值"))
                {
                    config->FsrVerticalFov.reset();
                    config->FsrHorizontalFov.reset();
                    config->FsrCameraNear.reset();
                    config->FsrCameraFar.reset();
                }

                ImGui::SameLine(0.0f, 6.0f);
                ImGui::Text("Near: %.1f Far: %.1f",
                            state.lastFsrCameraNear < 500000.0f ? state.lastFsrCameraNear : 500000.0f,
                            state.lastFsrCameraFar < 500000.0f ? state.lastFsrCameraFar : 500000.0f);

                ImGui::Spacing();
                ImGui::Spacing();
            }
        }
    }
}

void MenuCommon::RenderFramerateSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& menuResScale = ctx.menuResScale;

    // Framerate ---------------------
    if (state.reflexLimitsFps || config->OverlayMenu.value_or_default())
    {
        SeparatorWithHelpMarker(
            "帧率", "尽可能使用 Reflex\n在 AMD/Intel 显卡上可用 Fakenvapi 代替 Reflex");

        static std::string currentMethod {};
        LowLatencyMode fakenvapiMode = {};
        if (state.reflexLimitsFps)
        {
            fakenvapiMode = fakenvapi::getCurrentMode();

            if (fakenvapiMode == LowLatencyMode::AntiLag2)
                currentMethod = "FSR Anti-Lag 2.0";
            else if (fakenvapiMode == LowLatencyMode::LatencyFlex)
                currentMethod = "LatencyFlex";
            else if (fakenvapiMode == LowLatencyMode::XeLL)
                currentMethod = "XeLL";
            else if (fakenvapiMode == LowLatencyMode::AntiLagVk)
                currentMethod = "Vulkan AntiLag";
            else if (fakenvapiMode == LowLatencyMode::None)
            {
                if (fakenvapi::isUsingAsMainNvapi())
                    currentMethod = "无";
                else
                    currentMethod = "Reflex";
            }

            if (state.rtssReflexInjection && fakenvapiMode == LowLatencyMode::AntiLag2 &&
                config->FGOutput.value_or_default() == FGOutput::FSRFG)
                ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.8f, 0.f, 1.f)),
                                   "RTSS Reflex 注入与 FSR Anti-Lag 2.0 和 FSR FG 同用 "
                                   "可能导致问题");
        }
        else
        {
            if (XellHooks::canLimit())
                currentMethod = "游戏 XeLL";
            else
                currentMethod = "回退";
        }

        if (state.rtssReflexInjection)
            currentMethod.append(" (RTSS)");

        const bool fakenvapiInactive = (fakenvapi::isUsingAsMainNvapi() || fakenvapiMode == LowLatencyMode::XeLL) &&
                                       !fakenvapi::isLowLatencyActive() && state.reflexLimitsFps;

        if (fakenvapiInactive)
            currentMethod.append("（未激活）");

        ImGui::Text("当前方式：%s", currentMethod.c_str());

        if (fakenvapiMode == LowLatencyMode::AntiLag2)
            ShowHelpMarker("FSR Anti-Lag 2.0 是 AntiLag 2 的新名称\n别问我为什么");

        if (state.reflexShowWarning)
        {
            ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.f, 0.f, 1.f)),
                               "FSR FG 下使用 Reflex 限帧有性能开销");

            ImGui::Spacing();
        }

        // set initial value
        if (std::isinf(_limitFps))
            _limitFps = config->FramerateLimit.value_or_default();

        ImGui::SliderFloat("帧率限制", &_limitFps, 0, 200, "%.0f");

        if (ImGui::Button("应用限制"))
        {
            config->FramerateLimit = _limitFps;
        }

        ImGui::SameLine(0.0f, 16.0f);

        if (ImGui::Button("重置限制"))
        {
            _limitFps = 0.0f;
            config->FramerateLimit = _limitFps;
        }

        ImGui::Spacing();
        if (auto ch = ScopedCollapsingHeader("VRR 帧上限计算器"); ch.IsHeaderOpen())
        {
            ScopedIndent indent {};
            ImGui::Spacing();

            ImGui::PushItemWidth(105.0f * menuResScale);
            ImGui::InputInt("刷新率", &refreshRate, 1, 1, ImGuiInputTextFlags_None);
            ImGui::PopItemWidth();

            float refreshRateF = static_cast<float>(refreshRate);
            // it's fine to use with real reflex, we only care about antilag
            auto fpsLimitTech = fakenvapi::getCurrentMode();
            constexpr float margin = 0.3f; // in ms
            float frameCap = std::round(10000.f / (1000.f / refreshRateF + margin)) / 10.f;

            if (fpsLimitTech == LowLatencyMode::AntiLag2 || fpsLimitTech == LowLatencyMode::AntiLagVk)
                frameCap = std::round(frameCap);

            ImGui::Text("计算上限: %.1f", frameCap);

            ImGui::SameLine(0.0f, 16.0f);

            if (ImGui::Button("设为帧率限制"))
            {
                _limitFps = frameCap;
                config->FramerateLimit = _limitFps;
            }
        }
    }
}

void MenuCommon::RenderFakenvapiSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;

    // FAKENVAPI ---------------------------
    ImGui::SeparatorText("fakenvapi");

    // Using state.reflexLimitsFps as a detection for Reflex being used on Nvidia
    bool showLatencyFlex =
        fakenvapi::isUsingAsMainNvapi() || (state.activeFgOutput == FGOutput::XeFG && state.reflexLimitsFps);

    if (showLatencyFlex)
    {
        ImGui::BeginDisabled(state.activeFgOutput == FGOutput::XeFG || state.activeFgInput == FGInput::ForceXeLL);
        if (bool forceLFX = config->FN_ForceLatencyFlex.value_or_default();
            ImGui::Checkbox("强制 LatencyFlex", &forceLFX))
        {
            config->FN_ForceLatencyFlex = forceLFX;
        }
        ShowHelpMarker("默认在可用时使用 FSR Anti-Lag 2.0/XeLL。\n"
                       "此设置可强制改用 LatencyFlex");
        ImGui::EndDisabled();

        // Keep Force XeLL on the same line if LatencyFlex is visible
        ImGui::SameLine(0.0f, 16.0f);
    }

    // Force XeLL is always visible
    bool forceXell = config->ForceXeLL.value_or_default();
    static bool activeForceXeLL = forceXell;

    if (ImGui::Checkbox("强制 XeLL", &forceXell))
    {
        config->ForceXeLL = forceXell;
    }
    ShowHelpMarker("允许在非 Intel 显卡上无 FG 使用 XeLL。\n\n禁用 FG "
                   "选项\n\n需要重启");

    if (activeForceXeLL != forceXell)
    {
        ImGui::Spacing();
        ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.f, 0.0f, 1.f)), "保存 INI 并重启以应用更改");
        ImGui::Spacing();
    }

    if (showLatencyFlex)
    {
        // clang-format off
        static const std::vector<MenuOption<LFXMode>> lfx_modes = {
            { LFXMode::Conservative, "保守",
                "最安全，但降低延迟效果可能不佳" },
            { LFXMode::Aggressive, "激进",
                "降低延迟更好，但某些情况掉帧超预期" },
            { LFXMode::ReflexIDs, "Reflex ID",
                "可用时效果最好，部分游戏不兼容（如赛博朋克）\n"
                "将回退到激进模式" }
        };

        bool usingLFX = fakenvapi::getCurrentMode() == LowLatencyMode::LatencyFlex;

        ImGui::BeginDisabled(!usingLFX);
        PopulateCombo("LatencyFlex 模式", config->FN_LatencyFlexMode, lfx_modes);
        ImGui::EndDisabled();

        static std::vector<MenuOption<ForceReflex>> reflex_modes = { { ForceReflex::InGame, "跟随游戏内" },
                                                                { ForceReflex::ForceDisable, "强制禁用" },
                                                                { ForceReflex::ForceEnable, "强制启用" } };

        PopulateCombo("强制 Reflex", config->FN_ForceReflex, reflex_modes);
        // clang-format on
    }
}

template <typename T> std::string GetMenuOptionLabel(const std::vector<MenuOption<T>>& options, T targetValue)
{
    auto it = std::find_if(options.begin(), options.end(),
                           [targetValue](const MenuOption<T>& option) { return option.value == targetValue; });

    if (it != options.end())
    {
        return it->label;
    }

    return "未知";
}

void MenuCommon::RenderLowLatencySettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;

    // Low Latency ---------------------------
    ImGui::SeparatorText("低延迟");

    static std::vector<MenuOption<LowLatencyInput>> lowLatencyInput = {
        { LowLatencyInput::None, "无（关闭）" },    { LowLatencyInput::Auto, "自动" },
        { LowLatencyInput::AntiLag2, "AntiLag 2" }, { LowLatencyInput::Reflex, "Reflex" },
        { LowLatencyInput::XeLL, "XeLL" },          { LowLatencyInput::UeLowLatency, "UeLowLatency" },
    };

    static std::vector<MenuOption<LowLatencyMode>> lowLatencyOutput = {
        { LowLatencyMode::None, "无（关闭）" },
        { LowLatencyMode::Auto, "自动" },
        { LowLatencyMode::LatencyFlex, "LatencyFlex" },
        { LowLatencyMode::AntiLag2, "AntiLag 2" },
        { LowLatencyMode::XeLL, "XeLL" },
        { LowLatencyMode::AntiLagVk, "AntiLag Vk" },
        { LowLatencyMode::Reflex, "Reflex" },
    };

    LowLatencyInput activeInput {};
    LowLatencyMode activeOutput {};

    if (ImGui::BeginTable("lowLatencyActive", 2, ImGuiTableFlags_SizingStretchSame))
    {
        InputCommon::get_currently_active(activeInput, activeOutput);

        ImGui::TableNextColumn();

        ImGui::Text("当前输入: %s", GetMenuOptionLabel(lowLatencyInput, activeInput).c_str());

        ImGui::TableNextColumn();

        ImGui::Text("当前输出: %s", GetMenuOptionLabel(lowLatencyOutput, activeOutput).c_str());

        ImGui::EndTable();
    }

    if (ImGui::BeginTable("lowLatencySelection", 2, ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableNextColumn();

        auto avalibleInputs = InputCommon::get_avaliable_inputs();

        lowLatencyInput[(uint32_t) LowLatencyInput::AntiLag2].set_disabled(!avalibleInputs[LowLatencyInput::AntiLag2]);
        lowLatencyInput[(uint32_t) LowLatencyInput::Reflex].set_disabled(!avalibleInputs[LowLatencyInput::Reflex]);
        lowLatencyInput[(uint32_t) LowLatencyInput::XeLL].set_disabled(!avalibleInputs[LowLatencyInput::XeLL]);
        lowLatencyInput[(uint32_t) LowLatencyInput::UeLowLatency].set_disabled(
            !avalibleInputs[LowLatencyInput::UeLowLatency]);

        // need to have a value before combo
        if (!config->LowLatencyInput.has_value())
            config->LowLatencyInput = config->LowLatencyInput.value_or_default();

        PopulateCombo("输入", config->LowLatencyInput, lowLatencyInput);

        ImGui::TableNextColumn();

        lowLatencyOutput[(uint32_t) LowLatencyMode::AntiLagVk].set_disabled(true, "不支持");
        lowLatencyOutput[(uint32_t) LowLatencyMode::Reflex].set_disabled(true, "不支持");

        // need to have a value before combo
        if (!config->LowLatencyOutput.has_value())
            config->LowLatencyOutput = config->LowLatencyOutput.value_or_default();

        PopulateCombo("输出", config->LowLatencyOutput, lowLatencyOutput);

        ImGui::EndTable();
    }

    if (activeOutput == LowLatencyMode::LatencyFlex)
    {
        static const std::vector<MenuOption<LFXMode>> lfx_modes = {
            { LFXMode::Conservative, "保守", "最安全，但降低延迟效果可能不佳" },
            { LFXMode::Aggressive, "激进",
              "降低延迟更好，但某些情况掉帧超预期" },
            { LFXMode::ReflexIDs, "Reflex ID",
              "可用时效果最好，部分游戏不兼容（如赛博朋克）\n"
              "将回退到激进模式" }
        };

        PopulateCombo("LatencyFlex 模式", config->FN_LatencyFlexMode, lfx_modes);
    }

    static std::vector<MenuOption<ForceReflex>> lowlatency_states = { { ForceReflex::InGame, "跟随游戏内" },
                                                                      { ForceReflex::ForceDisable, "强制禁用" },
                                                                      { ForceReflex::ForceEnable, "强制启用" } };

    ImGui::SetNextItemWidth(150.0f * ctx.menuResScale);
    PopulateCombo("强制状态", config->FN_ForceReflex, lowlatency_states);
}

void MenuCommon::RenderActiveImageSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;

    bool rcasEnabled = false;

    if (currentFeature != nullptr && !currentFeature->IsFrozen())
    {
        // SHARPNESS -----------------------------
        ImGui::SeparatorText("锐化");

        if (bool overrideSharpness = config->OverrideSharpness.value_or_default();
            ImGui::Checkbox("覆盖", &overrideSharpness))
        {
            config->OverrideSharpness = overrideSharpness;

            if (currentBackend == Upscaler::DLSS && currentFeature->Version().major < 3)
            {
                state.newBackend = currentBackend;
                MARK_ALL_BACKENDS_CHANGED();
            }
        }
        ShowHelpMarker("忽略游戏发送的值\n"
                       "改用下方设置的值");

        ImGui::SameLine(0.0f, 16.0f * menuResScale);

        float featuresCurrentSharpness = currentFeature->Sharpness();
        if (featuresCurrentSharpness > 0.0f)
            ImGui::TextDisabled("(当前锐化: %.3f)", featuresCurrentSharpness);
        else
            ImGui::TextDisabled("(当前锐化: 已禁用)");

        ImGui::BeginDisabled(!config->OverrideSharpness.value_or_default());

        float sharpness = config->Sharpness.value_or_default();

        if (ImGui::SliderFloat("锐化", &sharpness, 0.0f, 1.0f))
            config->Sharpness = sharpness;

        ImGui::EndDisabled();

        // RCAS
        // if (state.api == DX12 || state.api == DX11)
        {
            // xess or dlss version >= 2.5.1
            constexpr feature_version requiredDlssVersion = { 2, 5, 1 };
            rcasEnabled = (currentBackend == Upscaler::XeSS ||
                           (currentBackend == Upscaler::DLSS && currentFeature->Version() >= requiredDlssVersion));

            ImGui::Spacing();
            ImGui::Spacing();

            if (bool rcas = config->RcasEnabled.value_or(rcasEnabled); ImGui::Checkbox("启用 RCAS/DA", &rcas))
                config->RcasEnabled = rcas;

            ShowHelpMarker("启用 OptiScaler 锐化滤镜\n"
                           "默认使用游戏提供的锐化值\n"
                           "在“锐化”下选择“覆盖”并调节滑块\n"
                           "即可修改\n\n"
                           "部分升频器自带锐化滤镜，因此\n"
                           "此选项并非总是需要");

            ImGui::BeginDisabled(!config->RcasEnabled.value_or(rcasEnabled));

            auto sharpnessShader = (int32_t) Config::Instance()->SharpnessShader.value_or_default();

            if (ImGui::RadioButton("RCAS", &sharpnessShader, (int32_t) SharpenShader::RCAS))
            {
                Config::Instance()->SharpnessShader = SharpenShader::RCAS;
            }

            ShowHelpMarker("使用 AMD RCAS\n"
                           "已修改，增加对比度参数\n"
                           "及 MAS 支持");

            ImGui::SameLine(0.0f, 6.0f);

            if (ImGui::RadioButton("深度感知 (RCAS)", &sharpnessShader, (int32_t) SharpenShader::DepthAware))
            {
                Config::Instance()->SharpnessShader = SharpenShader::DepthAware;
            }

            ShowHelpMarker("使用深度感知锐化（RCAS）\n"
                           "更智能、伪影更少，\n"
                           "但开销更大\n\n"
                           "目标越远，\n"
                           "应用的锐化越强");

            ImGui::SameLine(0.0f, 6.0f);

            if (ImGui::RadioButton("深度感知 (DAS)", &sharpnessShader,
                                   (int32_t) SharpenShader::LocalContrastDepthAware))
            {
                Config::Instance()->SharpnessShader = SharpenShader::LocalContrastDepthAware;
            }

            ShowHelpMarker("使用深度感知锐化（DAS）\n"
                           "深度感知方向自适应亮度锐化\n"
                           "更智能、伪影更少，\n"
                           "但开销更大\n\n"
                           "目标越远，\n"
                           "应用的锐化越强");

            ImGui::Spacing();

            if (bool overrideMotionSharpness = config->MotionSharpnessEnabled.value_or_default();
                ImGui::Checkbox("启用运动自适应锐化", &overrideMotionSharpness))
                config->MotionSharpnessEnabled = overrideMotionSharpness;
            ShowHelpMarker("按运动量调节锐化");

            if (Config::Instance()->SharpnessShader.value_or_default() != SharpenShader::RCAS)
            {
                if (bool overrideMSDebug = config->MotionSharpnessDebug.value_or_default();
                    ImGui::Checkbox("DA + MAS 调试", &overrideMSDebug))
                    config->MotionSharpnessDebug = overrideMSDebug;

                ShowHelpMarker("启用 DA + MAS 调试视图\n"
                               "DA 检测到的边缘染蓝色\n\n"
                               "越红的区域应用的锐化越多\n"
                               "绿色区域会降低锐化");

                if (auto ch = ScopedCollapsingHeader("高级 DA 参数"); ch.IsHeaderOpen())
                {
                    ScopedIndent indent {};
                    ImGui::Spacing();

                    if (bool clamp = config->DAClampOutput.value_or(false); ImGui::Checkbox("钳制输出", &clamp))
                    {
                        if (clamp)
                            config->DAClampOutput = true;
                        else
                            config->DAClampOutput.reset();
                    }

                    ShowHelpMarker("将最终图像钳制到 [0, 1] 范围。\n\n"
                                   "防止过冲伪影，如亮光晕或负色。\n"
                                   "LDR 管线建议开启；HDR 是否开启取决于色调映射。\n\n"
                                   "未设置时 OptiScaler 按升频器 HDR 标志控制");

                    if (currentFeature->DepthLinear())
                    {
                        float depthBias = config->DADepthBias.value_or(0.0015f);
                        if (ImGui::SliderFloat("深度偏移", &depthBias, 0.005f, 0.03f, "%.4f"))
                            config->DADepthBias = depthBias;

                        ShowHelpMarker("边缘检测前忽略微小深度差异。\n\n"
                                       "值越大越减少细微深度变化的闪烁与噪声，但可能 "
                                       "模糊真实几何边缘。\n"
                                       "值越小越保留细节，但边缘检测可能不稳定或有噪声。");

                        float depthScale = config->DADepthScale.value_or(250.0f);
                        if (ImGui::SliderFloat("深度缩放", &depthScale, 100.0f, 600.0f, "%.1f"))
                            config->DADepthScale = depthScale;

                        ShowHelpMarker("控制跨深度边缘降低锐化的强度。\n\n"
                                       "值越大越积极防止跨物体边界锐化 "
                                       "（减少光晕）。\n"
                                       "值越小越允许锐化跨过边缘（更锐但 "
                                       "风险更高）。");
                    }
                    else
                    {
                        float depthBias = config->DADepthBias.value_or(0.001f);
                        if (ImGui::SliderFloat("深度偏移", &depthBias, 0.0001f, 0.003f, "%.4f"))
                            config->DADepthBias = depthBias;

                        ShowHelpMarker("边缘检测前忽略微小深度差异。\n\n"
                                       "值越大越减少细微深度变化的闪烁与噪声，但可能 "
                                       "模糊真实几何边缘。\n"
                                       "值越小越保留细节，但边缘检测可能不稳定或有噪声。");

                        float depthScale = config->DADepthScale.value_or(35.0f);
                        if (ImGui::SliderFloat("深度缩放", &depthScale, 25.0f, 400.0f, "%.1f"))
                            config->DADepthScale = depthScale;

                        ShowHelpMarker("控制跨深度边缘降低锐化的强度。\n\n"
                                       "值越大越积极防止跨物体边界锐化 "
                                       "（减少光晕）。\n"
                                       "值越小越允许锐化跨过边缘（更锐但 "
                                       "风险更高）。");
                    }

                    if (ImGui::Button("重置深度数值"))
                    {
                        config->DADepthBias.reset();
                        config->DADepthScale.reset();
                    }
                }
            }
            else
            {
                if (bool contrastEnabled = config->ContrastEnabled.value_or_default();
                    ImGui::Checkbox("对比度启用", &contrastEnabled))
                    config->ContrastEnabled = contrastEnabled;

                ShowHelpMarker("控制高对比区域的锐化。");

                ImGui::BeginDisabled(!config->ContrastEnabled.value_or_default());

                float contrast = config->Contrast.value_or_default();
                if (ImGui::SliderFloat("对比度", &contrast, -2.0f, 2.0f, "%.2f"))
                    config->Contrast = contrast;

                ShowHelpMarker("正值降低高对比区域的锐化。\n"
                               "负值增强高对比区域的锐化。");

                ImGui::EndDisabled();
            }

            ImGui::Spacing();
            if (auto ch = ScopedCollapsingHeader("运动自适应锐化##2"); ch.IsHeaderOpen())
            {
                ScopedIndent indent {};
                ImGui::Spacing();

                ImGui::BeginDisabled(!config->MotionSharpnessEnabled.value_or_default());

                if (Config::Instance()->SharpnessShader.value_or_default() == SharpenShader::RCAS)
                {
                    if (bool overrideMSDebug = config->MotionSharpnessDebug.value_or_default();
                        ImGui::Checkbox("MAS 调试", &overrideMSDebug))
                        config->MotionSharpnessDebug = overrideMSDebug;
                    ShowHelpMarker("越红的区域应用的锐化越多\n"
                                   "绿色区域会降低锐化");
                }

                float motionSharpness = config->MotionSharpness.value_or_default();
                ImGui::SliderFloat("运动锐化", &motionSharpness, -1.0f, 1.0f, "%.3f");
                config->MotionSharpness = motionSharpness;

                ShowHelpMarker("运动可增减的最大锐化量。\n\n"
                               "负值在运动中降低锐化（推荐）。\n"
                               "正值在运动中增强锐化。\n\n"
                               "最终调节随运动量缩放，并以该值为上限。");

                float motionThreshod = config->MotionThreshold.value_or_default();
                ImGui::SliderFloat("运动阈值", &motionThreshod, 0.0f, 100.0f, "%.2f");
                config->MotionThreshold = motionThreshod;

                ShowHelpMarker("开始运动锐化调节所需的最小运动量。\n\n"
                               "值越大越忽略小幅运动（更稳定）。\n"
                               "值越小对细微运动越敏感。");

                float motionScale = config->MotionScaleLimit.value_or_default();
                ImGui::SliderFloat("运动范围", &motionScale, 0.01f, 100.0f, "%.2f");
                config->MotionScaleLimit = motionScale;

                ShowHelpMarker("效果从零渐变到全强度对应的运动区间。\n\n"
                               "高于阈值的值映射到该区间。\n"
                               "值越大响应越平滑渐进。\n"
                               "值越小反应越快越激进。");

                ImGui::EndDisabled();

                ImGui::Spacing();
                ImGui::Spacing();
            }

            ImGui::EndDisabled();
        }

        // UPSCALE RATIO OVERRIDE -----------------

        auto minSliderLimit = config->ExtendedLimits.value_or_default() ? 0.1f : 1.0f;
        auto maxSliderLimit = config->ExtendedLimits.value_or_default() ? 6.0f : 3.0f;

        ImGui::SeparatorText("升频比例覆盖");

        if (bool upOverride = config->UpscaleRatioOverrideEnabled.value_or_default();
            ImGui::Checkbox("覆盖全部", &upOverride))
        {
            config->UpscaleRatioOverrideEnabled = upOverride;

            if (upOverride)
                config->QualityRatioOverrideEnabled = false;
        }
        ShowHelpMarker("用设定值覆盖所有升频器预设\n\n"
                       "1080p 屏幕下 1.5x 表示内部为 720p\n"
                       "1080 / 1.5 = 720");

        if (bool qOverride = config->QualityRatioOverrideEnabled.value_or_default();
            ImGui::Checkbox("按质量预设覆盖", &qOverride))
        {
            config->QualityRatioOverrideEnabled = qOverride;

            if (qOverride)
                config->UpscaleRatioOverrideEnabled = false;
        }

        ShowHelpMarker("可逐个覆盖各预设比例\n"
                       "注意并非所有游戏支持所有质量预设\n\n"
                       "1080p 屏幕下 1.5x 表示内部为 720p\n"
                       "1080 / 1.5 = 720");

        if (config->UpscaleRatioOverrideEnabled.value_or_default())
        {
            float urOverride = config->UpscaleRatioOverrideValue.value_or_default();
            ImGui::SliderFloat("全部比例", &urOverride, minSliderLimit, maxSliderLimit, "%.3f");
            config->UpscaleRatioOverrideValue = urOverride;
        }

        if (config->QualityRatioOverrideEnabled.value_or_default())
        {
            float qDlaa = config->QualityRatio_DLAA.value_or_default();
            if (ImGui::SliderFloat("DLAA", &qDlaa, minSliderLimit, maxSliderLimit, "%.3f"))
                config->QualityRatio_DLAA = qDlaa;

            float qUq = config->QualityRatio_UltraQuality.value_or_default();
            if (ImGui::SliderFloat("超高质量", &qUq, minSliderLimit, maxSliderLimit, "%.3f"))
                config->QualityRatio_UltraQuality = qUq;

            float qQ = config->QualityRatio_Quality.value_or_default();
            if (ImGui::SliderFloat("质量", &qQ, minSliderLimit, maxSliderLimit, "%.3f"))
                config->QualityRatio_Quality = qQ;

            float qB = config->QualityRatio_Balanced.value_or_default();
            if (ImGui::SliderFloat("均衡", &qB, minSliderLimit, maxSliderLimit, "%.3f"))
                config->QualityRatio_Balanced = qB;

            float qP = config->QualityRatio_Performance.value_or_default();
            if (ImGui::SliderFloat("性能", &qP, minSliderLimit, maxSliderLimit, "%.3f"))
                config->QualityRatio_Performance = qP;

            float qUp = config->QualityRatio_UltraPerformance.value_or_default();
            if (ImGui::SliderFloat("超级性能", &qUp, minSliderLimit, maxSliderLimit, "%.3f"))
                config->QualityRatio_UltraPerformance = qUp;
        }

        if (currentFeature != nullptr && !currentFeature->IsFrozen())
        {
            // OUTPUT SCALING -----------------------------
            // if (state.api == DX12 || state.api == DX11)
            {
                // if motion vectors are not display size
                ImGui::BeginDisabled(!currentFeature->LowResMV() &&
                                     currentFeature->RenderWidth() != currentFeature->DisplayWidth());

                ImGui::SeparatorText("输出缩放");

                float defaultRatio = 1.5f;

                if (_ssRatio == 0.0f)
                {
                    _ssRatio = config->OutputScalingMultiplier.value_or(defaultRatio);
                    _ssEnabled = config->OutputScalingEnabled.value_or_default();
                    _ssDownsampler = config->OutputScalingDownscaler.value_or_default();
                }

                ImGui::BeginDisabled((currentBackend == Upscaler::XeSS || currentBackend == Upscaler::DLSS) &&
                                     currentFeature->RenderWidth() > currentFeature->DisplayWidth());
                ImGui::Checkbox("启用", &_ssEnabled);
                ImGui::EndDisabled();

                ShowHelpMarker("先在内部放大到更高输出分辨率\n"
                               "再缩小回显示分辨率\n\n"
                               "小于 1.0 更省性能\n"
                               "大于 1.0 更清晰但更耗性能\n\n"
                               "若置灰请查 Git Wiki - Unreal Engine 调整\n\n"
                               "底部显示目标分辨率与总比例（最大总计 3.0！）");

                ImGui::SameLine(0.0f, 6.0f);

                ImGui::BeginDisabled(!_ssEnabled);
                {
                    ImGui::PushItemWidth(95.0f * menuResScale);

                    // clang-format off
                    std::vector<MenuOption<Scaler>> ds_options = {
                        { Scaler::FSR1, "FSR1",
                            "默认选项。\n画质够用且非常快。" },
                        { Scaler::Bicubic, "Bicubic",
                            "最快的传统选项。\n画面很柔/模糊，缩小尚可。" },
                        { Scaler::CatmullRom, "Catmull-Rom",
                            "主要为缩小设计。\n对比保留好、伪影少，但比 Lanczos 柔。" },
                        { Scaler::Lanczos2, "Lanczos2",
                            "比 Lanczos3 更轻更快。\n振铃更少，但稍模糊。" },
                        { Scaler::Lanczos3, "Lanczos3",
                            "Lanczos2 的重载版。\n最锐，但最易振铃。\n与 Kaiser3 并列最佳。" },
                        { Scaler::Kaiser2, "Kaiser2",
                            "类似 Lanczos2。\n比 Lanczos 更平滑、伪影更少，但稍模糊。" },
                        { Scaler::Kaiser3, "Kaiser3",
                            "类似 Lanczos3。\n比 Lanczos3 伪影少得多，但 GPU 开销大得多。\n与 Lanczos3 并列最佳。" },
                        { Scaler::Magic, "MAGIC",
                            "专为防伪影设计。\n消除生硬光晕、观感自然，但可能偏柔。" }
                    };
                    // clang-format on

                    const bool isUpsampleRatio = _ssRatio < 1.0f;
                    const std::string disabledReason = "比例低于 1.0 时仅支持 FSR1 与 Bicubic。";

                    for (auto& opt : ds_options)
                    {
                        if (isUpsampleRatio && opt.value > Scaler::Bicubic)
                            opt.set_disabled(true, opt.tooltip + "\n\n" + disabledReason);
                    }

                    if (isUpsampleRatio && _ssDownsampler > Scaler::Bicubic)
                        _ssDownsampler = Scaler::FSR1;

                    PopulateCombo("降采样器", _ssDownsampler, ds_options);

                    ImGui::PopItemWidth();
                }
                ImGui::EndDisabled();

                bool applyEnabled = _ssEnabled != config->OutputScalingEnabled.value_or_default() ||
                                    _ssRatio != config->OutputScalingMultiplier.value_or(defaultRatio) ||
                                    _ssDownsampler != config->OutputScalingDownscaler.value_or_default();

                ImGui::BeginDisabled(!applyEnabled);
                if (ImGui::Button("应用更改"))
                {
                    config->OutputScalingEnabled = _ssEnabled;
                    config->OutputScalingMultiplier = _ssRatio;

                    if (_ssRatio < 1.0f && _ssDownsampler > Scaler::Bicubic)
                        _ssDownsampler = Scaler::FSR1;

                    config->OutputScalingDownscaler = _ssDownsampler;

                    const bool usesDlssd = currentFeature->GetUpscalerType() == Upscaler::DLSSD;
                    if (usesDlssd)
                        state.newBackend = Upscaler::DLSSD;
                    else
                        state.newBackend = currentBackend;

                    MARK_ALL_BACKENDS_CHANGED();
                }
                ImGui::EndDisabled();

                ImGui::BeginDisabled(!_ssEnabled || currentFeature->RenderWidth() > currentFeature->DisplayWidth());
                ImGui::SliderFloat("比例", &_ssRatio, 0.5f, 3.0f, "%.2f");
                ImGui::EndDisabled();

                if (currentFeature != nullptr && !currentFeature->IsFrozen())
                {
                    ImGui::Text("输出缩放为 %s，目标分辨率：%dx%d (%.2f)\n抖动计数：%d",
                                config->OutputScalingEnabled.value_or_default() ? "已启用" : "已禁用",
                                (uint32_t) (currentFeature->DisplayWidth() * _ssRatio),
                                (uint32_t) (currentFeature->DisplayHeight() * _ssRatio),
                                ((float) currentFeature->DisplayWidth() * _ssRatio) /
                                    (float) currentFeature->RenderWidth(),
                                currentFeature->JitterCount());
                }

                ImGui::EndDisabled();
            }
        }

        // INIT -----------------------------
        ImGui::SeparatorText("初始化标志");
        if (ImGui::BeginTable("init", 2, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableNextColumn();

            // AutoExposure is always enabled for XeSS with native Dx11
            bool autoExposureDisabled = state.api == API::DX11 && currentBackend == Upscaler::XeSS;
            ImGui::BeginDisabled(autoExposureDisabled);

            if (bool autoExposure = currentFeature->AutoExposure(); ImGui::Checkbox("自动曝光", &autoExposure))
            {
                config->AutoExposure = autoExposure;
                ReInitUpscaler();
            }
            ShowResetButton(&config->AutoExposure, "R");
            ShowHelpMarker("部分 Unreal Engine 游戏需要\n\n"
                           "若颜色闪烁或\n"
                           "物体有鬼影拖尾可尝试");

            ImGui::EndDisabled();

            ImGui::TableNextColumn();
            auto accessToReactiveMask = currentFeature->AccessToReactiveMask();
            ImGui::BeginDisabled(!accessToReactiveMask);

            bool canUseReactiveMask =
                accessToReactiveMask && currentBackend != Upscaler::DLSS &&
                (currentBackend != Upscaler::XeSS || currentFeature->Version() >= feature_version { 2, 0, 1 });

            bool disableReactiveMask = config->DisableReactiveMask.value_or(!canUseReactiveMask);

            if (ImGui::Checkbox("禁用响应掩膜", &disableReactiveMask))
            {
                config->DisableReactiveMask = disableReactiveMask;

                if (currentBackend == Upscaler::XeSS)
                {
                    state.newBackend = currentBackend;
                    MARK_ALL_BACKENDS_CHANGED();
                }
            }

            ImGui::EndDisabled();

            if (accessToReactiveMask)
                ShowHelpMarker("允许使用 Reactive 掩膜\n"
                               "注意发往 DLSS 的 Reactive 掩膜\n"
                               "与 FSR/XeSS 合用画质不佳");
            else
                ShowHelpMarker("游戏未提供 Reactive 掩膜，故禁用此选项");

            ImGui::EndTable();

            ImGui::Spacing();
            if (auto ch = ScopedCollapsingHeader("高级初始化标志"); ch.IsHeaderOpen())
            {
                ScopedIndent indent {};
                ImGui::Spacing();

                if (ImGui::BeginTable("init2", 2, ImGuiTableFlags_SizingStretchProp))
                {
                    ImGui::TableNextColumn();
                    if (bool depth = currentFeature->DepthInverted(); ImGui::Checkbox("深度反转", &depth))
                    {
                        config->DepthInverted = depth;
                        ReInitUpscaler();
                    }
                    ShowResetButton(&config->DepthInverted, "R##2");
                    ShowHelpMarker("通常无需更改");

                    ImGui::TableNextColumn();
                    if (bool hdr = currentFeature->IsHdr(); ImGui::Checkbox("HDR", &hdr))
                    {
                        config->HDR = hdr;
                        ReInitUpscaler();
                    }
                    ShowResetButton(&config->HDR, "R##1");
                    ShowHelpMarker("可能缓解部分游戏的紫色偏色");

                    ImGui::TableNextColumn();
                    if (bool mv = !currentFeature->LowResMV(); ImGui::Checkbox("显示分辨率 MV", &mv))
                    {
                        config->DisplayResolution = mv;

                        // Disable output scaling when
                        // Display res MV is active
                        if (mv)
                        {
                            config->OutputScalingEnabled = false;
                            _ssEnabled = false;
                        }

                        ReInitUpscaler();
                    }
                    ShowResetButton(&config->DisplayResolution, "R##4");
                    ShowHelpMarker("多为 Unreal Engine 游戏的修复\n"
                                   "屏幕左上会变模糊");

                    ImGui::TableNextColumn();

                    if (bool jitter = currentFeature->JitteredMV(); ImGui::Checkbox("抖动消除", &jitter))
                    {
                        config->JitterCancellation = jitter;
                        ReInitUpscaler();
                    }
                    ShowResetButton(&config->JitterCancellation, "R##3");
                    ShowHelpMarker("修复发送带预应用抖动的运动数据的游戏");

                    ImGui::TableNextColumn();
                    ImGui::EndTable();
                }

                if (currentFeature->AccessToReactiveMask() && currentBackend != Upscaler::DLSS)
                {
                    ImGui::BeginDisabled(config->DisableReactiveMask.value_or(currentBackend == Upscaler::XeSS));

                    bool binaryMask = state.api == Vulkan || currentBackend == Upscaler::XeSS;
                    auto defaultBias = binaryMask ? 0.0f : 0.45f;
                    auto maskBias = config->DlssReactiveMaskBias.value_or(defaultBias);

                    if (!binaryMask)
                    {
                        if (ImGui::SliderFloat("响应掩膜偏移", &maskBias, 0.0f, 0.9f, "%.2f"))
                            config->DlssReactiveMaskBias = maskBias;

                        ShowHelpMarker("大于 0 即启用 Reactive 掩膜");
                    }
                    else
                    {
                        bool useRM = maskBias > 0.0f;
                        if (ImGui::Checkbox("使用二值响应掩膜", &useRM))
                        {
                            if (useRM)
                                config->DlssReactiveMaskBias = 0.45f;
                            else
                                config->DlssReactiveMaskBias.reset();
                        }
                    }

                    ImGui::EndDisabled();
                }
            }
        }
    }
}

void MenuCommon::RenderMagnifierSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;

    // Magnifier -----------------------------
    ImGui::Spacing();
    if (auto ch = ScopedCollapsingHeader("放大镜"); ch.IsHeaderOpen())
    {
        ScopedIndent indent {};
        ImGui::Spacing();

        bool magnifierEnabled = config->MagnifierEnabled.value_or_default();
        if (ImGui::Checkbox("启用放大镜", &magnifierEnabled))
            config->MagnifierEnabled = magnifierEnabled;

        ImGui::BeginDisabled(!magnifierEnabled);

        float magnifierSize = config->MagnifierSize.value_or_default();
        if (ImGui::SliderFloat("尺寸", &magnifierSize, 5.0f, 50.0f, "%.1f%% 屏幕"))
            config->MagnifierSize = magnifierSize;

        int zoomFactor = config->MagnifierZoomFactor.value_or_default();
        if (ImGui::SliderInt("缩放倍数", &zoomFactor, 2, 20, "%d倍"))
            config->MagnifierZoomFactor = zoomFactor;

        float borderSize = config->MagnifierBorderSize.value_or_default();
        if (ImGui::SliderFloat("边框尺寸", &borderSize, 0.0f, 2.0f, "%.2f%% 屏幕"))
            config->MagnifierBorderSize = borderSize;

        ImGui::Separator();
        ImGui::Text("定位");

        bool staticMode = config->MagnifierStaticPosX.has_value() && config->MagnifierStaticPosY.has_value();
        if (staticMode)
        {
            float staticX = config->MagnifierStaticPosX.value();
            if (ImGui::SliderFloat("静态 X", &staticX, 0.0f, 100.0f, "%.1f%%"))
                config->MagnifierStaticPosX = staticX;

            float staticY = config->MagnifierStaticPosY.value();
            if (ImGui::SliderFloat("静态 Y", &staticY, 0.0f, 100.0f, "%.1f%%"))
                config->MagnifierStaticPosY = staticY;

            if (ImGui::Button("重置静态位置 (跟随光标)"))
            {
                config->MagnifierStaticPosX.reset();
                config->MagnifierStaticPosY.reset();
            }
        }
        else
        {
            // Button to initialize static position mode
            if (ImGui::Button("设为静态位置"))
            {
                config->MagnifierStaticPosX = 50.0f;
                config->MagnifierStaticPosY = 50.0f;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(当前跟随光标)");

            float offsetX = config->MagnifierCursorOffsetX.value_or_default();
            if (ImGui::SliderFloat("光标偏移 X", &offsetX, -300.0f, 300.0f, "%.0f 像素"))
                config->MagnifierCursorOffsetX = offsetX;

            float offsetY = config->MagnifierCursorOffsetY.value_or_default();
            if (ImGui::SliderFloat("光标偏移 Y", &offsetY, -300.0f, 300.0f, "%.0f 像素"))
                config->MagnifierCursorOffsetY = offsetY;
        }

        ImGui::EndDisabled();
        ImGui::Spacing();
    }
}
void MenuCommon::RenderQuirksSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;

    // QUIRKS -----------------------------
    if (state.detectedQuirks.size() > 0)
    {
        ImGui::Spacing();
        if (auto ch = ScopedCollapsingHeader("当前 Quirks"); ch.IsHeaderOpen())
        {
            ScopedIndent indent {};
            ImGui::Spacing();

            for (const auto& quirk : state.detectedQuirks)
            {
                ImGui::TextWrapped("%s", quirk.c_str());
            }
        }
    }
}

void MenuCommon::RenderAdvancedSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;

    // ADVANCED SETTINGS -----------------------------
    ImGui::Spacing();
    if (auto ch = ScopedCollapsingHeader("高级设置"); ch.IsHeaderOpen())
    {
        ScopedIndent indent {};
        ImGui::Spacing();

        if (currentFeature != nullptr && !currentFeature->IsFrozen())
        {
            bool extendedLimits = config->ExtendedLimits.value_or_default();
            if (ImGui::Checkbox("启用扩展限制", &extendedLimits))
                config->ExtendedLimits = extendedLimits;

            ShowHelpMarker("扩展质量预设滑块上限\n\n"
                           "使用此选项会改变分辨率检测逻辑\n"
                           "可能导致问题与崩溃！");
        }

        bool pcShaders = config->UsePrecompiledShaders.value_or_default();
        if (ImGui::Checkbox("使用预编译着色器", &pcShaders))
        {
            config->UsePrecompiledShaders = pcShaders;
            state.newBackend = currentBackend;
            MARK_ALL_BACKENDS_CHANGED();
        }

        // DRS
        ImGui::SeparatorText("DRS (动态分辨率缩放)");
        if (ImGui::BeginTable("drs", 2, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableNextColumn();
            if (bool drsMin = config->DrsMinOverrideEnabled.value_or_default();
                ImGui::Checkbox("覆盖最小值", &drsMin))
                config->DrsMinOverrideEnabled = drsMin;
            ShowHelpMarker("修复忽略官方 DRS 限制的游戏");

            ImGui::TableNextColumn();
            if (bool drsMax = config->DrsMaxOverrideEnabled.value_or_default();
                ImGui::Checkbox("覆盖最大值", &drsMax))
                config->DrsMaxOverrideEnabled = drsMax;
            ShowHelpMarker("修复忽略官方 DRS 限制的游戏");

            ImGui::EndTable();
        }

        // Non-DLSS hotfixes -----------------------------
        if (currentFeature != nullptr && !currentFeature->IsFrozen() && currentBackend != Upscaler::DLSS)
        {
            // BARRIERS -----------------------------
            ImGui::Spacing();
            if (auto ch = ScopedCollapsingHeader("资源屏障"); ch.IsHeaderOpen())
            {
                ScopedIndent indent {};
                ImGui::Spacing();

                AddResourceBarrier("Color", &config->ColorResourceBarrier);
                AddResourceBarrier("Depth", &config->DepthResourceBarrier);
                AddResourceBarrier("运动", &config->MVResourceBarrier);
                AddResourceBarrier("Exposure", &config->ExposureResourceBarrier);
                AddResourceBarrier("Mask", &config->MaskResourceBarrier);
                AddResourceBarrier("Output", &config->OutputResourceBarrier);
            }

            // HOTFIXES -----------------------------
            if (state.api == DX12)
            {
                ImGui::Spacing();
                if (auto ch = ScopedCollapsingHeader("根签名"); ch.IsHeaderOpen())
                {
                    ScopedIndent indent {};
                    ImGui::Spacing();

                    if (bool crs = config->RestoreComputeSignature.value_or_default();
                        ImGui::Checkbox("恢复计算根签名", &crs))
                        config->RestoreComputeSignature = crs;

                    if (bool grs = config->RestoreGraphicSignature.value_or_default();
                        ImGui::Checkbox("恢复图形根签名", &grs))
                        config->RestoreGraphicSignature = grs;
                }
            }
        }
    }
}

void MenuCommon::RenderLoggingSettings(RenderMenuContext& ctx)
{
    auto config = ctx.config;

    // LOGGING -----------------------------
    ImGui::Spacing();
    if (auto ch = ScopedCollapsingHeader("日志"); ch.IsHeaderOpen())
    {
        ScopedIndent indent {};
        ImGui::Spacing();

        if (config->LogToConsole.value_or_default() || config->LogToFile.value_or_default() ||
            config->LogToNGX.value_or_default())
            spdlog::default_logger()->set_level((spdlog::level::level_enum) config->LogLevel.value_or_default());
        else
            spdlog::default_logger()->set_level(spdlog::level::off);

        if (bool toFile = config->LogToFile.value_or_default(); ImGui::Checkbox("输出到文件", &toFile))
        {
            config->LogToFile = toFile;
            PrepareLogger();
        }

        ImGui::SameLine(0.0f, 6.0f);
        if (bool toConsole = config->LogToConsole.value_or_default(); ImGui::Checkbox("输出到控制台", &toConsole))
        {
            config->LogToConsole = toConsole;
            PrepareLogger();
        }

        const char* logLevels[] = { "跟踪", "调试", "信息", "警告", "错误" };
        const char* selectedLevel = logLevels[config->LogLevel.value_or_default()];

        if (ImGui::BeginCombo("日志级别", selectedLevel))
        {
            for (int n = 0; n < 5; n++)
            {
                if (ImGui::Selectable(logLevels[n], (config->LogLevel.value_or_default() == n)))
                {
                    config->LogLevel = n;
                    spdlog::default_logger()->set_level(
                        (spdlog::level::level_enum) config->LogLevel.value_or_default());
                }
            }

            ImGui::EndCombo();
        }
    }
}

void MenuCommon::RenderProfilesSettings(RenderMenuContext& ctx)
{
    if (auto header = ScopedCollapsingHeader("配置"); header.IsHeaderOpen())
    {
        static char name[128] {};
        static std::string message;
        ImGui::InputText("名称", name, sizeof(name));
        if (ImGui::Button("保存配置"))
            message = ctx.config->SaveProfile(string_to_wstring(name)) ? "配置已保存。" : "无法保存配置。";
        ImGui::SameLine();
        if (ImGui::BeginCombo("加载配置", "选择已保存的配置"))
        {
            for (const auto& profile : ctx.config->ListProfiles())
                if (ImGui::Selectable(profile.c_str()))
                    message = ctx.config->LoadProfile(string_to_wstring(profile)) ? "配置已加载。"
                                                                                  : "无法加载配置。";
            ImGui::EndCombo();
        }
        ImGui::TextWrapped("配置保存全部设置。仅启动生效的更改需重启游戏。");
        if (!message.empty())
            ImGui::TextUnformatted(message.c_str());
    }
}

void MenuCommon::RenderThemeSettings(RenderMenuContext& ctx)
{
    auto config = ctx.config;

    // THEME -----------------------------
    ImGui::Spacing();
    if (auto ch = ScopedCollapsingHeader("菜单主题与颜色"); ch.IsHeaderOpen())
    {
        ScopedIndent indent {};
        ImGui::Spacing();

        bool lightTheme = config->LightTheme.value_or_default();

        const ImVec4 bgDark = lightTheme ? ImVec4(0.80f, 0.82f, 0.86f, 1.00f) : ImVec4(0.09f, 0.09f, 0.10f, 1.00f);
        const ImVec4 bgMid = lightTheme ? ImVec4(0.89f, 0.91f, 0.95f, 1.00f) : ImVec4(0.11f, 0.11f, 0.12f, 1.00f);
        const ImVec4 bgLight = lightTheme ? ImVec4(0.96f, 0.97f, 0.99f, 1.00f) : ImVec4(0.14f, 0.14f, 0.15f, 1.00f);

        auto Mix = [](const ImVec4& a, const ImVec4& b, float t, float alpha = 1.0f)
        { return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, alpha); };

        auto AccentSoft = [&](ImVec4 accent, float alpha = 1.0f)
        { return toneMapColor(lightTheme ? Mix(bgLight, accent, 0.24f, alpha) : Mix(bgDark, accent, 0.32f, alpha)); };

        auto AccentMed = [&](ImVec4 accent, float alpha = 1.0f)
        { return toneMapColor(lightTheme ? Mix(bgLight, accent, 0.42f, alpha) : Mix(bgDark, accent, 0.55f, alpha)); };

        auto AccentStrong = [&](ImVec4 accent, float alpha = 1.0f)
        { return toneMapColor(ImVec4(accent.x, accent.y, accent.z, alpha)); };

        if (ImGui::Checkbox("浅色主题", &lightTheme))
        {
            config->LightTheme = lightTheme;
            ApplyThemeStyle();
        }

        ImGui::SeparatorText("强调色");

        ImGui::Text("预设:");
        ImGui::SameLine(0.0f, 6.0f);

        ImVec4 colorBlue = { 0.00f, 0.40f, 0.77f, 1.0f };
        ImVec4 colorTeal = { 0.00f, 1.00f, 0.91f, 1.0f };
        ImVec4 colorGray = { 0.54f, 0.54f, 0.54f, 1.0f };
        ImVec4 colorYellow = { 1.00f, 0.89f, 0.00f, 1.0f };
        ImVec4 colorGreen = { 0.25f, 1.00f, 0.00f, 1.0f };
        ImVec4 colorRed = { 1.00f, 0.00f, 0.00f, 1.0f };
        ImVec4 colorOrange = { 1.00f, 0.52f, 0.00f, 1.0f };
        ImVec4 colorPurple = { 0.576f, 0.00f, 1.00f, 1.0f };

        ImVec4 color = {};

        color = colorBlue;
        ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

        if (ImGui::Button("蓝色"))
        {
            ImGui::PopStyleColor(3);

            config->MenuAccentColorR = color.x;
            config->MenuAccentColorG = color.y;
            config->MenuAccentColorB = color.z;
            ApplyThemeStyle();
        }
        else
        {
            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine(0.0f, 6.0f);

        color = colorTeal;
        ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

        if (ImGui::Button("青色"))
        {
            ImGui::PopStyleColor(3);

            config->MenuAccentColorR = color.x;
            config->MenuAccentColorG = color.y;
            config->MenuAccentColorB = color.z;
            ApplyThemeStyle();
        }
        else
        {
            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine(0.0f, 6.0f);

        color = colorGray;
        ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

        if (ImGui::Button("灰色"))
        {
            ImGui::PopStyleColor(3);

            config->MenuAccentColorR = color.x;
            config->MenuAccentColorG = color.y;
            config->MenuAccentColorB = color.z;
            ApplyThemeStyle();
        }
        else
        {
            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine(0.0f, 6.0f);

        color = colorYellow;
        ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

        if (ImGui::Button("黄色"))
        {
            ImGui::PopStyleColor(3);

            config->MenuAccentColorR = color.x;
            config->MenuAccentColorG = color.y;
            config->MenuAccentColorB = color.z;
            ApplyThemeStyle();
        }
        else
        {
            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine(0.0f, 6.0f);

        color = colorGreen;
        ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

        if (ImGui::Button("绿色"))
        {
            ImGui::PopStyleColor(3);

            config->MenuAccentColorR = color.x;
            config->MenuAccentColorG = color.y;
            config->MenuAccentColorB = color.z;
            ApplyThemeStyle();
        }
        else
        {
            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine(0.0f, 6.0f);

        color = colorRed;
        ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

        if (ImGui::Button("红色"))
        {
            ImGui::PopStyleColor(3);

            config->MenuAccentColorR = color.x;
            config->MenuAccentColorG = color.y;
            config->MenuAccentColorB = color.z;
            ApplyThemeStyle();
        }
        else
        {
            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine(0.0f, 6.0f);

        color = colorOrange;
        ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

        if (ImGui::Button("橙色"))
        {
            ImGui::PopStyleColor(3);

            config->MenuAccentColorR = color.x;
            config->MenuAccentColorG = color.y;
            config->MenuAccentColorB = color.z;
            ApplyThemeStyle();
        }
        else
        {
            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine(0.0f, 6.0f);

        color = colorPurple;
        ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

        if (ImGui::Button("紫色"))
        {
            ImGui::PopStyleColor(3);

            config->MenuAccentColorR = color.x;
            config->MenuAccentColorG = color.y;
            config->MenuAccentColorB = color.z;
            ApplyThemeStyle();
        }
        else
        {
            ImGui::PopStyleColor(3);
        }

        float accentColor[3] = { config->MenuAccentColorR.value_or_default(),
                                 config->MenuAccentColorG.value_or_default(),
                                 config->MenuAccentColorB.value_or_default() };

        if (ImGui::ColorEdit3("自定义强调色", accentColor))
        {
            config->MenuAccentColorR = accentColor[0];
            config->MenuAccentColorG = accentColor[1];
            config->MenuAccentColorB = accentColor[2];
            ApplyThemeStyle();
        }

        ImGui::Spacing();

        if (ImGui::Button("重置强调色"))
        {
            config->MenuAccentColorR.reset();
            config->MenuAccentColorG.reset();
            config->MenuAccentColorB.reset();
            ApplyThemeStyle();
        }

        ImGui::Spacing();

        ImGui::SeparatorText("背景色");

        ImGui::Text("预设:");
        ImGui::SameLine(0.0f, 6.0f);

        color = colorBlue;
        ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

        if (ImGui::Button("蓝色##2"))
        {
            ImGui::PopStyleColor(3);

            config->MenuBGColorR = color.x;
            config->MenuBGColorG = color.y;
            config->MenuBGColorB = color.z;
            ApplyThemeStyle();
        }
        else
        {
            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine(0.0f, 6.0f);

        color = colorTeal;
        ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

        if (ImGui::Button("青色##2"))
        {
            ImGui::PopStyleColor(3);

            config->MenuBGColorR = color.x;
            config->MenuBGColorG = color.y;
            config->MenuBGColorB = color.z;
            ApplyThemeStyle();
        }
        else
        {
            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine(0.0f, 6.0f);

        color = colorGray;
        ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

        if (ImGui::Button("灰色##2"))
        {
            ImGui::PopStyleColor(3);

            config->MenuBGColorR = color.x;
            config->MenuBGColorG = color.y;
            config->MenuBGColorB = color.z;
            ApplyThemeStyle();
        }
        else
        {
            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine(0.0f, 6.0f);

        color = colorYellow;
        ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

        if (ImGui::Button("黄色##2"))
        {
            ImGui::PopStyleColor(3);

            config->MenuBGColorR = color.x;
            config->MenuBGColorG = color.y;
            config->MenuBGColorB = color.z;
            ApplyThemeStyle();
        }
        else
        {
            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine(0.0f, 6.0f);

        color = colorGreen;
        ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

        if (ImGui::Button("绿色##2"))
        {
            ImGui::PopStyleColor(3);

            config->MenuBGColorR = color.x;
            config->MenuBGColorG = color.y;
            config->MenuBGColorB = color.z;
            ApplyThemeStyle();
        }
        else
        {
            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine(0.0f, 6.0f);

        color = colorRed;
        ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

        if (ImGui::Button("红色##2"))
        {
            ImGui::PopStyleColor(3);

            config->MenuBGColorR = color.x;
            config->MenuBGColorG = color.y;
            config->MenuBGColorB = color.z;
            ApplyThemeStyle();
        }
        else
        {
            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine(0.0f, 6.0f);

        color = colorOrange;
        ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

        if (ImGui::Button("橙色##2"))
        {
            ImGui::PopStyleColor(3);

            config->MenuBGColorR = color.x;
            config->MenuBGColorG = color.y;
            config->MenuBGColorB = color.z;
            ApplyThemeStyle();
        }
        else
        {
            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine(0.0f, 6.0f);

        color = colorPurple;
        ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

        if (ImGui::Button("紫色##2"))
        {
            ImGui::PopStyleColor(3);

            config->MenuBGColorR = color.x;
            config->MenuBGColorG = color.y;
            config->MenuBGColorB = color.z;
            ApplyThemeStyle();
        }
        else
        {
            ImGui::PopStyleColor(3);
        }

        float bgColor[3] = { config->MenuBGColorR.value_or_default(), config->MenuBGColorG.value_or_default(),
                             config->MenuBGColorB.value_or_default() };

        if (ImGui::ColorEdit3("自定义背景色", bgColor))
        {
            config->MenuBGColorR = bgColor[0];
            config->MenuBGColorG = bgColor[1];
            config->MenuBGColorB = bgColor[2];
            ApplyThemeStyle();
        }

        ImGui::Spacing();

        auto alpha = config->MenuBGColorA.value_or_default();
        if (ImGui::SliderFloat("背景透明度", &alpha, 0.0f, 1.0f))
        {
            config->MenuBGColorA = alpha;
            ApplyThemeStyle();
        }

        ImGui::Spacing();

        if (ImGui::Button("重置背景色"))
        {
            config->MenuBGColorR.reset();
            config->MenuBGColorG.reset();
            config->MenuBGColorB.reset();
            config->MenuBGColorA.reset();
            ApplyThemeStyle();
        }

        ImGui::Spacing();
    }
}

void MenuCommon::RenderFpsOverlaySettings(RenderMenuContext& ctx)
{
    auto config = ctx.config;

    // FPS OVERLAY -----------------------------
    ImGui::Spacing();
    if (auto ch = ScopedCollapsingHeader("FPS 浮层"); ch.IsHeaderOpen())
    {
        ScopedIndent indent {};
        ImGui::Spacing();

        bool fpsEnabled = config->ShowFps.value_or_default();
        if (ImGui::Checkbox("启用 FPS 浮层", &fpsEnabled))
            config->ShowFps = fpsEnabled;

        ImGui::SameLine(0.0f, 6.0f);

        bool fpsHorizontal = config->FpsOverlayHorizontal.value_or_default();
        if (ImGui::Checkbox("横向", &fpsHorizontal))
            config->FpsOverlayHorizontal = fpsHorizontal;

        const char* fpsPosition[] = { "左上", "右上", "左下", "右下" };
        const char* selectedPosition = fpsPosition[config->FpsOverlayPosition.value_or_default()];

        if (ImGui::BeginCombo("浮层位置", selectedPosition))
        {
            for (int n = 0; n < std::size(fpsPosition); n++)
            {
                if (ImGui::Selectable(fpsPosition[n], (config->FpsOverlayPosition.value_or_default() == n)))
                    config->FpsOverlayPosition = (FpsOverlayPos) n;
            }

            ImGui::EndCombo();
        }

        const char* fpsType[] = { "仅 FPS", "简洁",       "详细",      "详细 + 曲线",
                                  "完整",     "完整 + 曲线", "Reflex 计时" };
        const char* selectedType = fpsType[config->FpsOverlayType.value_or_default()];

        if (ImGui::BeginCombo("浮层类型", selectedType))
        {
            for (int n = 0; n < std::size(fpsType); n++)
            {
                if (ImGui::Selectable(fpsType[n], (config->FpsOverlayType.value_or_default() == n)))
                    config->FpsOverlayType = (FpsOverlay) n;
            }

            ImGui::EndCombo();
        }

        float fpsAlpha = config->FpsOverlayAlpha.value_or_default();
        if (ImGui::SliderFloat("背景透明度", &fpsAlpha, 0.0f, 1.0f, "%.2f"))
            config->FpsOverlayAlpha = fpsAlpha;

        const char* options[] = { "与菜单一致", "0.5", "0.6", "0.7", "0.8", "0.9", "1.0", "1.1", "1.2",
                                  "1.3",          "1.4", "1.5", "1.6", "1.7", "1.8", "1.9", "2.0" };
        int currentIndex = std::max(((int) (config->FpsScale.value_or(0.0f) * 10.0f)) - 4, 0);
        float values[] = { 0.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.2f,
                           1.3f, 1.4f, 1.5f, 1.6f, 1.7f, 1.8f, 1.9f, 2.0f };

        if (ImGui::SliderInt("缩放", &currentIndex, 0, IM_ARRAYSIZE(options) - 1, options[currentIndex],
                             ImGuiSliderFlags_ClampOnInput))
        {
            if (currentIndex == 0)
                config->FpsScale.reset();
            else
                config->FpsScale = values[currentIndex];
        }

        bool useTheme = config->OverlaysUseTheme.value_or_default();
        if (ImGui::Checkbox("使用主题色", &useTheme))
            config->OverlaysUseTheme = useTheme;
    }
}

void MenuCommon::RenderUpscalerInputsSettings(RenderMenuContext& ctx)
{
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;

    // UPSCALER INPUTS -----------------------------
    ImGui::Spacing();
    auto uiStateOpen = currentFeature == nullptr || currentFeature->IsFrozen();
    if (auto ch = ScopedCollapsingHeader("升频器输入", uiStateOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
        ch.IsHeaderOpen())
    {
        ScopedIndent indent {};
        ImGui::Spacing();

        if (config->EnableFsr2Inputs.value_or_default())
        {
            bool fsr2Inputs = config->UseFsr2Inputs.value_or_default();
            bool fsr2Pattern = config->Fsr2Pattern.value_or_default();

            if (ImGui::Checkbox("使用 Fsr2 输入", &fsr2Inputs))
                config->UseFsr2Inputs = fsr2Inputs;

            if (ImGui::Checkbox("使用 Fsr2 模式匹配", &fsr2Pattern))
                config->Fsr2Pattern = fsr2Pattern;
            ShowTooltip("此设置将在下次启动生效!");
        }

        if (config->EnableFsr3Inputs.value_or_default())
        {
            bool fsr3Inputs = config->UseFsr3Inputs.value_or_default();
            bool fsr3Pattern = config->Fsr3Pattern.value_or_default();

            if (ImGui::Checkbox("使用 Fsr3 输入", &fsr3Inputs))
                config->UseFsr3Inputs = fsr3Inputs;

            if (ImGui::Checkbox("使用 Fsr3 模式匹配", &fsr3Pattern))
                config->Fsr3Pattern = fsr3Pattern;
            ShowTooltip("此设置将在下次启动生效!");
        }

        if (config->EnableFfxInputs.value_or_default())
        {
            bool ffxInputs = config->UseFfxInputs.value_or_default();

            if (ImGui::Checkbox("使用 Ffx 输入", &ffxInputs))
                config->UseFfxInputs = ffxInputs;
        }
    }
}

void MenuCommon::RenderApiAndTextureSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;

    // DX11 & DX12 -----------------------------
    if (state.swapchainApi != Vulkan)
    {
        // V-SYNC -----------------------------
        ImGui::Spacing();
        if (auto ch = ScopedCollapsingHeader("垂直同步设置"); ch.IsHeaderOpen())
        {
            ScopedIndent indent {};
            ImGui::Spacing();

            auto forceVsyncOn = config->ForceVsync.has_value() && config->ForceVsync.value();
            auto forceVsyncOff = config->ForceVsync.has_value() && !config->ForceVsync.value();
            bool vsyncChanged = false;

            if (ImGui::Checkbox("垂直同步开", &forceVsyncOn))
            {
                if (forceVsyncOn)
                {
                    config->ForceVsync = true;
                    vsyncChanged = true;
                }
                else
                {
                    config->ForceVsync.reset();
                    vsyncChanged = true;
                }
            }
            ImGui::SameLine(0.0f, 16.0f);

            if (ImGui::Checkbox("垂直同步关", &forceVsyncOff))
            {
                if (forceVsyncOff)
                {
                    config->ForceVsync = false;
                    vsyncChanged = true;
                }
                else
                {
                    config->ForceVsync.reset();
                    vsyncChanged = true;
                }
            }
            ImGui::SameLine(0.0f, 16.0f);

            ImGui::BeginDisabled(!forceVsyncOn);

            ImGui::PushItemWidth(50.0f * menuResScale);

            auto vsyncBuf = StrFmt("%d", config->VsyncInterval.value_or_default());
            if (ImGui::BeginCombo("同步间隔", vsyncBuf.c_str()))
            {
                if (ImGui::Selectable("0", config->VsyncInterval.value_or_default() == 0))
                {
                    config->VsyncInterval = 0;
                    vsyncChanged = true;
                }

                if (ImGui::Selectable("1", config->VsyncInterval.value_or_default() == 1))
                {
                    config->VsyncInterval = 1;
                    vsyncChanged = true;
                }

                if (ImGui::Selectable("2", config->VsyncInterval.value_or_default() == 2))
                {
                    config->VsyncInterval = 2;
                    vsyncChanged = true;
                }

                if (ImGui::Selectable("3", config->VsyncInterval.value_or_default() == 3))
                {
                    config->VsyncInterval = 3;
                    vsyncChanged = true;
                }

                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            ShowHelpMarker("控制 DXGI Present 同步间隔，决定交换链如何等待垂直刷新。\n"
                           "0 = 立即呈现，不等待垂直同步。\n"
                           "1 = 与每次刷新同步，标准垂直同步。\n"
                           "2+ = 每 N 次刷新呈现一次，降低有效帧率。\n\n"
                           "较高值可减少撕裂，但可能增加延迟并限制 FPS。\n"
                           "多数游戏使用 0 以获得最低延迟，或使用 1 获得标准垂直同步。");

            ImGui::EndDisabled();
            ImGui::SameLine(0.0f, 16.0f);

            if (ImGui::Button("重置##10"))
            {
                config->ForceVsync.reset();
                vsyncChanged = true;
            }

            ShowHelpMarker("强制 垂直同步开/关与同步间隔选项");

            if (vsyncChanged && state.activeFgOutput == FGOutput::XeFG && state.currentFG != nullptr)
            {
                // To prevent XeLL issues
                LOG_DEBUG("V-Sync change detected, forcing XeFG reset");
                state.WAR_xefgRequestFGToggle = true;
            }
        }

        // MIPMAP BIAS & Anisotropy -----------------------------
        ImGui::Spacing();
        if (auto ch = ScopedCollapsingHeader("Mipmap 偏移", (currentFeature == nullptr || currentFeature->IsFrozen())
                                                                ? ImGuiTreeNodeFlags_DefaultOpen
                                                                : 0);
            ch.IsHeaderOpen())
        {
            ScopedIndent indent {};
            ImGui::Spacing();
            if (config->MipmapBiasOverride.has_value() && _mipBias == 0.0f)
                _mipBias = config->MipmapBiasOverride.value();

            ImGui::SliderFloat("Mipmap 偏移##2", &_mipBias, -15.0f, 15.0f, "%.6f");
            ShowHelpMarker("可修复破损游戏中的模糊纹理\n"
                           "负值使纹理更锐利\n"
                           "正值使纹理更模糊\n\n"
                           "有轻微性能影响");

            ImGui::BeginDisabled(!config->MipmapBiasOverride.has_value());
            {
                ImGui::BeginDisabled(config->MipmapBiasScaleOverride.has_value() &&
                                     config->MipmapBiasScaleOverride.value());
                {
                    bool mbFixed = config->MipmapBiasFixedOverride.value_or_default();
                    if (ImGui::Checkbox("MB 固定覆盖", &mbFixed))
                    {
                        config->MipmapBiasScaleOverride.reset();
                        config->MipmapBiasFixedOverride = mbFixed;
                    }

                    ShowHelpMarker("对全部纹理应用相同覆盖值");
                }
                ImGui::EndDisabled();

                ImGui::SameLine(0.0f, 6.0f);

                ImGui::BeginDisabled(config->MipmapBiasFixedOverride.has_value() &&
                                     config->MipmapBiasFixedOverride.value());
                {
                    bool mbScale = config->MipmapBiasScaleOverride.value_or_default();
                    if (ImGui::Checkbox("MB 缩放覆盖", &mbScale))
                    {
                        config->MipmapBiasFixedOverride.reset();
                        config->MipmapBiasScaleOverride = mbScale;
                    }

                    ShowHelpMarker("覆盖值作为缩放倍数应用\n"
                                   "使用缩放模式时，请使用正数\n"
                                   "覆盖值以提高锐度！");
                }
                ImGui::EndDisabled();

                bool mbAll = config->MipmapBiasOverrideAll.value_or_default();
                if (ImGui::Checkbox("MB 覆盖全部纹理", &mbAll))
                    config->MipmapBiasOverrideAll = mbAll;

                ShowHelpMarker("覆盖全部纹理 Mipmap 值\n"
                               "通常 OptiScaler 仅覆盖\n"
                               "小于零的 Mipmap 值！");
            }
            ImGui::EndDisabled();

            ImGui::BeginDisabled(config->MipmapBiasOverride.has_value() &&
                                 config->MipmapBiasOverride.value() == _mipBias);
            {
                if (ImGui::Button("设置"))
                {
                    config->MipmapBiasOverride = _mipBias;
                    state.lastMipBias = 100.0f;
                    state.lastMipBiasMax = -100.0f;
                }
            }
            ImGui::EndDisabled();

            ImGui::SameLine(0.0f, 6.0f);

            ImGui::BeginDisabled(!config->MipmapBiasOverride.has_value());
            {
                if (ImGui::Button("重置"))
                {
                    config->MipmapBiasOverride.reset();
                    _mipBias = 0.0f;
                    state.lastMipBias = 100.0f;
                    state.lastMipBiasMax = -100.0f;
                }
            }
            ImGui::EndDisabled();

            if (currentFeature != nullptr && !currentFeature->IsFrozen())
            {
                ImGui::SameLine(0.0f, 6.0f);

                if (ImGui::Button("计算 Mipmap 偏移"))
                    _showMipmapCalcWindow = true;
            }

            if (config->MipmapBiasOverride.has_value())
            {
                if (config->MipmapBiasFixedOverride.value_or_default())
                {
                    ImGui::Text("当前 : %.3f / %.3f，目标: %.3f", state.lastMipBias, state.lastMipBiasMax,
                                config->MipmapBiasOverride.value());
                }
                else if (config->MipmapBiasScaleOverride.value_or_default())
                {
                    ImGui::Text("当前 : %.3f / %.3f，目标: 基准 * %.3f", state.lastMipBias, state.lastMipBiasMax,
                                config->MipmapBiasOverride.value());
                }
                else
                {
                    ImGui::Text("当前 : %.3f / %.3f，目标: 基准 + %.3f", state.lastMipBias, state.lastMipBiasMax,
                                config->MipmapBiasOverride.value());
                }
            }
            else
            {
                ImGui::Text("当前 : %.3f / %.3f", state.lastMipBias, state.lastMipBiasMax);
            }

            ImGui::Text("将在 分辨率/预设 更改后生效！！！");
        }

        ImGui::Spacing();
        if (auto ch = ScopedCollapsingHeader(
                "各向异性过滤",
                (currentFeature == nullptr || currentFeature->IsFrozen()) ? ImGuiTreeNodeFlags_DefaultOpen : 0);
            ch.IsHeaderOpen())
        {
            ScopedIndent indent {};
            ImGui::Spacing();
            ImGui::PushItemWidth(65.0f * menuResScale);

            auto selectedAF =
                config->AnisotropyOverride.has_value() ? std::to_string(config->AnisotropyOverride.value()) : "自动";
            if (ImGui::BeginCombo("强制各向异性过滤", selectedAF.c_str()))
            {
                if (ImGui::Selectable("自动", !config->AnisotropyOverride.has_value()))
                    config->AnisotropyOverride.reset();

                if (ImGui::Selectable("1", config->AnisotropyOverride.value_or(0) == 1))
                    config->AnisotropyOverride = 1;

                if (ImGui::Selectable("2", config->AnisotropyOverride.value_or(0) == 2))
                    config->AnisotropyOverride = 2;

                if (ImGui::Selectable("4", config->AnisotropyOverride.value_or(0) == 4))
                    config->AnisotropyOverride = 4;

                if (ImGui::Selectable("8", config->AnisotropyOverride.value_or(0) == 8))
                    config->AnisotropyOverride = 8;

                if (ImGui::Selectable("16", config->AnisotropyOverride.value_or(0) == 16))
                    config->AnisotropyOverride = 16;

                ImGui::EndCombo();
            }

            ImGui::PopItemWidth();

            bool afComp = config->AnisotropyModifyComp.value_or_default();
            if (ImGui::Checkbox("修改比较", &afComp))
                config->AnisotropyModifyComp = afComp;

            ShowHelpMarker("更新比较过滤器");

            ImGui::SameLine(0.0f, 6.0f);

            bool afMinMax = config->AnisotropyModifyMinMax.value_or_default();
            if (ImGui::Checkbox("修改最小/最大", &afMinMax))
                config->AnisotropyModifyMinMax = afMinMax;

            ShowHelpMarker("更新最小/最大过滤器");

            bool afSkipPoint = config->AnisotropySkipPointFilter.value_or_default();
            if (ImGui::Checkbox("跳过点过滤", &afSkipPoint))
                config->AnisotropySkipPointFilter = afSkipPoint;

            ShowHelpMarker("跳过点过滤器更新");

            ImGui::Text("可能将在 分辨率/预设 更改后生效！！！");
        }
    }
}

void MenuCommon::RenderKeybindSettings(RenderMenuContext& ctx)
{
    auto config = ctx.config;

    ImGui::Spacing();
    if (auto ch = ScopedCollapsingHeader("快捷键"); ch.IsHeaderOpen())
    {
        ScopedIndent indent {};
        ImGui::Spacing();

        ImGui::Text("暂不支持组合键!");
        ImGui::Text("Esc 取消,Backspace 解绑");
        ImGui::Spacing();

        static auto menu = Keybind("菜单", 10);
        static auto fpsOverlay = Keybind("FPS 浮层", 11);
        static auto fpsOverlayCycle = Keybind("FPS 浮层切换", 12);
        static auto fgEnable = Keybind("帧生成", 13);
        static auto dlssNrToggle = Keybind("神经渲染", 14);

        menu.Render(config->ShortcutKey, config->ShortcutKeyRequireCtrl.value_or_default(),
                    config->ShortcutKeyRequireAlt.value_or_default());
        fpsOverlay.Render(config->FpsShortcutKey);
        fpsOverlayCycle.Render(config->FpsCycleShortcutKey);
        fgEnable.Render(config->FGShortcutKey);
        dlssNrToggle.Render(config->DlssNrToggleKey);
    }
}

void MenuCommon::RenderMainMenuTable(RenderMenuContext& ctx)
{
    if (ImGui::BeginTable("main", 2, ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableNextColumn();

        // Left column: active upscaler state, frame generation, FSR common, latency and fakenvapi controls.
        RenderActiveUpscalerSettings(ctx);
        RenderFrameGenerationSelection(ctx);
        RenderFrameGenerationRuntimeSettings(ctx);
        RenderFsrCommonSettings(ctx);
        RenderFramerateSettings(ctx);
#ifdef LOW_LATENCY_INPUTS
        RenderLowLatencySettings(ctx);
#else
        RenderFakenvapiSettings(ctx);
#endif

        ImGui::TableNextColumn();

        // Right column: image quality, initialization, advanced options, appearance, overlay and input settings.
        RenderActiveImageSettings(ctx);
        DlssNr::RenderMenu(ctx.config, ctx.menuResScale);
        RenderMagnifierSettings(ctx);
        RenderQuirksSettings(ctx);
        RenderAdvancedSettings(ctx);
        RenderLoggingSettings(ctx);
        RenderProfilesSettings(ctx);
        RenderThemeSettings(ctx);
        RenderFpsOverlaySettings(ctx);
        RenderUpscalerInputsSettings(ctx);
        RenderApiAndTextureSettings(ctx);
        RenderKeybindSettings(ctx);

        ImGui::EndTable();
    }
}

void MenuCommon::RenderMainMenuGraphs(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto& currentFeature = ctx.currentFeature;
    auto& frameTime = ctx.frameTime;
    auto& frameRate = ctx.frameRate;

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::BeginTable("plots", 2, ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableNextColumn();
        ImGui::Text("帧时间");
        auto ft = StrFmt("%7.2f ms / %6.1f fps", frameTime, frameRate);
        ImGui::PlotLines(
            ft.c_str(), [](void* rb, int idx) -> float
            { return static_cast<RingBuffer<float, plotWidth>*>(rb)->At(idx); }, &gFrameTimes, plotWidth);

        if (currentFeature != nullptr && !currentFeature->IsFrozen())
        {
            ImGui::TableNextColumn();
            ImGui::Text("升频器");

            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !state.detailedGpuTimes.empty())
            {
                ImGui::BeginTooltip();

                ImGui::TextDisabled("按着色器细分:");
                if (ImGui::BeginTable("ShaderTimes", 2, ImGuiTableFlags_SizingStretchProp))
                {
                    bool hasExtra = false;

                    for (auto& [name, time, includedInUpscalerTime] : state.detailedGpuTimes)
                    {
                        if (!includedInUpscalerTime)
                        {
                            hasExtra = true;
                            continue;
                        }

                        auto formattedTime = StrFmt("%7.2f ms", time);

                        ImGui::TableNextColumn();
                        ImGui::Text(name.c_str());

                        ImGui::TableNextColumn();
                        ImGui::Text(formattedTime.c_str());
                    }

                    std::optional<double> nrTime {};
                    nrTime = DlssNr::LastGpuTime();
                    if (hasExtra || nrTime.has_value())
                    {
                        ImGui::TableNextRow();
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextDisabled("额外着色器:");
                        ImGui::TableNextColumn();
                        ImGui::TextDisabled("");
                        for (auto& [name, time, includedInUpscalerTime] : state.detailedGpuTimes)
                        {
                            if (includedInUpscalerTime)
                                continue;

                            auto formattedTime = StrFmt("%7.2f ms", time);

                            ImGui::TableNextColumn();
                            ImGui::Text(name.c_str());

                            ImGui::TableNextColumn();
                            ImGui::Text(formattedTime.c_str());
                        }

                        if (nrTime.has_value())
                        {
                            ImGui::TableNextColumn();
                            ImGui::Text("神经渲染 (耗时)");
                            ImGui::TableNextColumn();
                            ImGui::Text(StrFmt("%.2f ms", nrTime.value()).c_str());
                        }
                    }

                    ImGui::EndTable();
                }

                ImGui::EndTooltip();
            }

            auto ups = StrFmt("%7.2f ms", state.upscaleTimes.back());
            ImGui::PlotLines(
                ups.c_str(), [](void* rb, int idx) -> float
                { return static_cast<RingBuffer<float, plotWidth>*>(rb)->At(idx); }, &gUpscalerTimes, plotWidth);
        }

        ImGui::EndTable();
    }
}

void MenuCommon::RenderMainMenuBottomBar(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& io = ctx.io;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;

    // BOTTOM LINE ---------------
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (currentFeature != nullptr && !currentFeature->IsFrozen())
    {
        ImGui::Text("%dx%d -> %dx%d (%.1f) [%dx%d (%.1f)]", currentFeature->RenderWidth(),
                    currentFeature->RenderHeight(), currentFeature->TargetWidth(), currentFeature->TargetHeight(),
                    (float) currentFeature->TargetWidth() / (float) currentFeature->RenderWidth(),
                    currentFeature->DisplayWidth(), currentFeature->DisplayHeight(),
                    (float) currentFeature->DisplayWidth() / (float) currentFeature->RenderWidth());

        ImGui::SameLine(0.0f, 4.0f);

        ImGui::Text("%d", currentFeature->FrameCount());

        ImGui::SameLine(0.0f, 10.0f);
    }

    ImGui::PushItemWidth(100.0f * menuResScale);

    auto autoText = config->MenuScale.has_value() ? "自动" : StrFmt("自动 (%3.1f)", menuResScale);
    // clang-format off
    const char* uiScales[] = { autoText.c_str(), "0.5", "0.6", "0.7", "0.8", "0.9", "1.0", "1.1",
                               "1.2", "1.3", "1.4", "1.5", "1.6", "1.7", "1.8", "1.9", "2.0" };
    // clang-format on

    const char* selectedScaleName = uiScales[_selectedScale];

    if (ImGui::BeginCombo("菜单缩放", selectedScaleName))
    {
        for (int n = 0; n < std::size(uiScales); n++)
        {
            if (ImGui::Selectable(uiScales[n], (_selectedScale == n)))
            {
                _selectedScale = n;

                if (n == 0)
                    config->MenuScale.reset();
                else
                    config->MenuScale = 0.4f + (float) n / 10.0f;
            }
        }

        ImGui::EndCombo();
    }

    ImGui::PopItemWidth();

    ImGui::SameLine(0.0f, 15.0f);

    if (ImGui::Button("保存设置"))
        config->SaveIni();

    ImGui::SameLine(0.0f, 6.0f);

    if (ImGui::Button("关闭"))
    {
        _isVisible = false;
        hasGamepad = (io.BackendFlags | ImGuiBackendFlags_HasGamepad) > 0;
        io.BackendFlags &= 30;
        io.ConfigFlags = ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoKeyboard;

        _showMipmapCalcWindow = false;
        _showHudlessWindow = false;
        io.MouseDrawCursor = false;
        io.WantCaptureKeyboard = false;
        io.WantCaptureMouse = false;
    }

    auto winSize = ImGui::GetWindowSize();
    auto winPos = ImGui::GetWindowPos();

    ImGui::Spacing();
    ImGui::Separator();

    if (state.nvngxIniDetected)
    {
        ImGui::Spacing();
        ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.f, 0.f, 1.f)),
                           "检测到 nvngx.ini，请迁移至 OptiScaler.ini 并删除旧配置");
        ImGui::Spacing();
    }

    if (lastPosition.x < -900.0f || (lastPosition.x >= winPos.x - 1.0f && lastPosition.y >= winPos.y - 1.0f &&
                                     lastPosition.x <= winPos.x + 1.0f && lastPosition.y <= winPos.y + 1.0f))
    {
        float posX;
        float posY;

        posX = ((float) io.DisplaySize.x - winSize.x) / 2.0f;
        posY = ((float) io.DisplaySize.y - winSize.y) / 2.0f;

        // don't position menu outside of screen
        if (posX < 0.0 || posY < 0.0)
        {
            posX = 50;
            posY = 50;
        }

        ImGui::SetWindowPos(ImVec2 { posX, posY });
        lastPosition.x = posX;
        lastPosition.y = posY;
    }
}

void MenuCommon::RenderMipmapBiasWindow(RenderMenuContext& ctx, ImGuiWindowFlags flags)
{
    auto config = ctx.config;
    auto& io = ctx.io;
    auto& currentFeature = ctx.currentFeature;

    // Metrics window (for debug)
    // ImGui::ShowMetricsWindow();

    // Mipmap calculation window
    if (_showMipmapCalcWindow && currentFeature != nullptr && !currentFeature->IsFrozen() && currentFeature->IsInited())
    {
        auto posX = (io.DisplaySize.x - 450.0f) / 2.0f;
        auto posY = (io.DisplaySize.y - 200.0f) / 2.0f;

        ImGui::SetNextWindowPos(ImVec2 { posX, posY }, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2 { 450.0f, 200.0f }, ImGuiCond_FirstUseEver);

        if (_displayWidth == 0)
        {
            if (config->OutputScalingEnabled.value_or_default())
            {
                _displayWidth = static_cast<uint32_t>(currentFeature->DisplayWidth() *
                                                      config->OutputScalingMultiplier.value_or_default());
            }
            else
            {
                _displayWidth = currentFeature->DisplayWidth();
            }

            _renderWidth = static_cast<uint32_t>(_displayWidth / 3.0f);
            _mipmapUpscalerQuality = 0;
            _mipmapUpscalerRatio = 3.0f;
            _mipBiasCalculated = log2((float) _renderWidth / (float) _displayWidth);
        }

        if (ImGui::Begin("Mipmap 偏移", nullptr, flags))
        {
            if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow))
                ImGui::SetWindowFocus();

            if (ImGui::InputScalar("显示宽度", ImGuiDataType_U32, &_displayWidth, NULL, NULL, "%u"))
            {
                if (_displayWidth <= 0)
                {
                    if (config->OutputScalingEnabled.value_or_default())
                    {
                        _displayWidth = static_cast<uint32_t>(currentFeature->DisplayWidth() *
                                                              config->OutputScalingMultiplier.value_or_default());
                    }
                    else
                    {
                        _displayWidth = currentFeature->DisplayWidth();
                    }
                }

                _renderWidth = static_cast<uint32_t>(_displayWidth / _mipmapUpscalerRatio);
                _mipBiasCalculated = log2((float) _renderWidth / (float) _displayWidth);
            }

            const char* q[] = { "超级性能", "性能", "均衡", "质量", "超高质量", "DLAA" };
            float fr[] = { 3.0f, 2.0f, 1.7f, 1.5f, 1.3f, 1.0f };
            auto configQ = _mipmapUpscalerQuality;

            const char* selectedQ = q[configQ];

            ImGui::BeginDisabled(config->UpscaleRatioOverrideEnabled.value_or_default());

            if (ImGui::BeginCombo("升频质量", selectedQ))
            {
                for (int n = 0; n < 6; n++)
                {
                    if (ImGui::Selectable(q[n], (_mipmapUpscalerQuality == n)))
                    {
                        _mipmapUpscalerQuality = n;

                        float ov = -1.0f;

                        if (config->QualityRatioOverrideEnabled.value_or_default())
                        {
                            switch (n)
                            {
                            case 0:
                                ov = config->QualityRatio_UltraPerformance.value_or(-1.0f);
                                break;

                            case 1:
                                ov = config->QualityRatio_Performance.value_or(-1.0f);
                                break;

                            case 2:
                                ov = config->QualityRatio_Balanced.value_or(-1.0f);
                                break;

                            case 3:
                                ov = config->QualityRatio_Quality.value_or(-1.0f);
                                break;

                            case 4:
                                ov = config->QualityRatio_UltraQuality.value_or(-1.0f);
                                break;
                            }
                        }

                        if (ov > 0.0f)
                            _mipmapUpscalerRatio = ov;
                        else
                            _mipmapUpscalerRatio = fr[n];

                        _renderWidth = static_cast<uint32_t>(_displayWidth / _mipmapUpscalerRatio);
                        _mipBiasCalculated = log2((float) _renderWidth / (float) _displayWidth);
                    }
                }

                ImGui::EndCombo();
            }

            ImGui::EndDisabled();

            auto minLimit = config->ExtendedLimits.value_or_default() ? 0.1f : 1.0f;
            auto maxLimit = config->ExtendedLimits.value_or_default() ? 6.0f : 3.0f;
            if (ImGui::SliderFloat("升频比例", &_mipmapUpscalerRatio, minLimit, maxLimit, "%.2f"))
            {
                _renderWidth = static_cast<uint32_t>(_displayWidth / _mipmapUpscalerRatio);
                _mipBiasCalculated = log2((float) _renderWidth / (float) _displayWidth);
            }

            if (ImGui::InputScalar("渲染宽度", ImGuiDataType_U32, &_renderWidth, NULL, NULL, "%u"))
                _mipBiasCalculated = log2((float) _renderWidth / (float) _displayWidth);

            ImGui::SliderFloat("Mipmap 偏移", &_mipBiasCalculated, -15.0f, 0.0f, "%.6f");

            // BOTTOM LINE
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::SameLine();
            ImGui::Spacing();

            constexpr float spacing = 6.0f;
            auto textSize = ImGui::CalcTextSize("使用该值");
            textSize += ImGui::CalcTextSize("关闭");
            textSize.x += ImGui::GetStyle().FramePadding.x * 5.0f + spacing; // 2 sides * 2 buttons + 1

            float avail = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - textSize.x);

            if (ImGui::Button("使用该值"))
            {
                _mipBias = _mipBiasCalculated;
                _showMipmapCalcWindow = false;
            }

            ImGui::SameLine(0.0f, spacing);

            if (ImGui::Button("关闭"))
                _showMipmapCalcWindow = false;

            ImGui::Spacing();
            ImGui::Separator();

            ImGui::End();
        }
    }
}

void MenuCommon::RenderHudlessResourcesWindow(RenderMenuContext& ctx, ImGuiWindowFlags flags)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& io = ctx.io;

    auto fg = state.currentFG;
    if (_showHudlessWindow && config->FGHUDFix.value_or_default() && fg != nullptr && fg->IsActive())
    {
        auto posX = (io.DisplaySize.x - 400.0f) / 2.0f;
        auto posY = (io.DisplaySize.y - 300.0f) / 2.0f;

        ImGui::SetNextWindowPos(ImVec2 { posX, posY }, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2 { 400.0f, 300.0f });

        if (ImGui::Begin("HUDless 资源", nullptr, flags))
        {
            if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow))
                ImGui::SetWindowFocus();

            int btnCount = 100;

            if (ImGui::BeginTable("HUDlessTable", 2, ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableSetupColumn("##1", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("##2", ImGuiTableColumnFlags_WidthFixed);

                ankerl::unordered_dense::map<void*, CapturedHudlessInfo>::iterator it;

                for (it = state.capturedHudlesses.begin(); it != state.capturedHudlesses.end(); it++)
                {
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);

                    ImGui::Text("%08x, %s->%s, 数量: %llu, %s", (size_t) it->first,
                                GetSourceString(it->second.captureInfo & 0xFF).c_str(),
                                GetDispatchString(it->second.captureInfo & 0xFF00).c_str(), it->second.usageCount,
                                it->second.enabled ? "启用" : "停用");

                    ImGui::TableSetColumnIndex(1);

                    btnCount++;
                    std::string text;

                    if (it->second.enabled)
                        text = StrFmt("禁用##%d", btnCount);
                    else
                        text = StrFmt("启用##%d", btnCount);

                    if (ImGui::Button(text.c_str()))
                    {
                        LOG_DEBUG("HUDless {:X}: {}", (size_t) it->first,
                                  it->second.enabled ? "Disabling" : "Enabling");
                        it->second.enabled = !it->second.enabled;
                    }
                }

                ImGui::EndTable();
            }

            if (ImGui::Button("清空##4"))
            {
                LOG_DEBUG("Clearing captured HUDless resources");
                state.clearCapturedHudlesses = true;
            }

            ImGui::SameLine(0.0f, 8.0f);

            if (ImGui::Button("关闭##4"))
                _showHudlessWindow = false;

            ImGui::End();
        }
    }
}

void MenuCommon::RenderMainMenuWindow(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& frameTime = ctx.frameTime;
    auto& frameRate = ctx.frameRate;
    auto& frameTimesCalculated = ctx.frameTimesCalculated;
    auto& menuResScale = ctx.menuResScale;

    if (!_isVisible)
        return;

    // Check for GPU support once and reuse the result in all menu sections.
    // DXVK might call Vulkan device creation, which would destroy our objects.
    State::Instance().vulkanSkipHooks = true;
    ctx.primaryGpu =
        std::make_unique<std::decay_t<decltype(IdentifyGpu::getPrimaryGpu())>>(IdentifyGpu::getPrimaryGpu());
    State::Instance().vulkanSkipHooks = false;

    // Overlay font
    if (config->UseHQFont.value_or_default())
        ImGui::PushFontSize(std::round(menuResScale * fontSize));

    // If overlay is not visible frame needs to be inited
    if (!frameTimesCalculated)
    {
        float frameCnt = 0;
        frameTime = 0;
        for (size_t i = 299; i > 199; i--)
        {
            if (state.frameTimes[i] > 0.0)
            {
                frameTime += state.frameTimes[i];
                frameCnt++;
            }
        }

        frameTime /= frameCnt;
        frameRate = 1000.0 / frameTime;
    }

    ImGuiWindowFlags flags = 0;
    flags |= ImGuiWindowFlags_NoSavedSettings;
    flags |= ImGuiWindowFlags_NoCollapse;
    flags |= ImGuiWindowFlags_AlwaysAutoResize;

    if (lastMenuScale != menuResScale)
    {
        lastMenuScale = menuResScale;

        // if UI scale is changed rescale the style
        ImGuiStyle& style = ImGui::GetStyle();
        ImGuiStyle styleold = style; // Backup colors
        style = ImGuiStyle();        // IMPORTANT: ScaleAllSizes will change the original size,
                                     // so we should reset all style config

        ApplyThemeStyle();

        style.ScaleAllSizes(menuResScale);
        style.MouseCursorScale = 1.0f;
        CopyMemory(style.Colors, styleold.Colors, sizeof(style.Colors)); // Restore colors

        ImGui::SetNextWindowSize({ 1.0f, 1.0f });
    }

    // Main menu window
    if (windowTitle.empty())
    {
        windowTitle = StrFmt("%s - %s %s %s %s", BuildInfo::ProductName(), state.gameExe.c_str(),
                             state.gameName.empty() ? "" : StrFmt("- %s", state.gameName.c_str()).c_str(),
                             (state.detectedQuirks.size() > 0) ? "(Q)" : "", state.isOptiPatcherSucceed ? "(OP)" : "");
    }

    if (ImGui::Begin(windowTitle.c_str(), NULL, flags))
    {
        // Header/status messages shown above the two-column settings table.
        RenderMainMenuHeaderMessages(ctx);

        // Main two-column settings content.
        RenderMainMenuTable(ctx);

        // Diagnostics and footer actions below the settings table.
        RenderMainMenuGraphs(ctx);
        RenderMainMenuBottomBar(ctx);

        ImGui::End();
    }

    // Detached utility windows owned by the main menu.
    RenderMipmapBiasWindow(ctx, flags);
    RenderHudlessResourcesWindow(ctx, flags);

    if (config->UseHQFont.value_or_default())
        ImGui::PopFontSize();
}

void KeyUp(UINT vKey)
{
    inputMenu = vKey == Config::Instance()->ShortcutKey.value_or_default();
    inputFps = vKey == Config::Instance()->FpsShortcutKey.value_or_default();
    inputFG = vKey == Config::Instance()->FGShortcutKey.value_or_default();
    inputFpsCycle = vKey == Config::Instance()->FpsCycleShortcutKey.value_or_default();
}

bool MenuCommon::RenderMenu()
{
    if (!_isInited)
        return false;

    RenderMenuContext ctx { State::Instance(), Config::Instance(), ImGui::GetIO() };
    ctx.now = Util::MillisecondsNow();
    ctx.currentFeature = ctx.state.currentFeature;

    // 1) Collect timing and input state before any ImGui drawing.
    UpdateRenderTiming(ctx);
    UpdateMenuInputMode(ctx);
    HandleMenuShortcuts(ctx);

    // 2) Prepare one-shot notifications and start a new ImGui frame only when needed.
    UpdateVersionAndStartupNotifications(ctx);
    BeginMenuFrameIfNeeded(ctx);
    OptiInput::EndFrame(_isVisible);

    // 3) Draw lightweight overlay windows first, preserving the original order.
    ctx.menuResScale = MenuResolutionScale(ctx.io);
    RenderSplashWindow(ctx);
    RenderNotifications(ctx);
    UpdateFrameTimeAverages(ctx);
    RenderPerformanceOverlay(ctx);

    // 4) Draw the full settings menu last so popups and child windows keep their existing behavior.
    RenderMainMenuWindow(ctx);

    if (ctx.newFrame)
        ImGui::EndFrame();

    return ctx.newFrame;
}

void MenuCommon::Init(HWND InHwnd, bool isUWP)
{
    // Reset shutdown flag in case of re-init
    State::Instance().isShuttingDown = false;

    HWND oldHandle = nullptr;

    if (_handle != nullptr)
    {
        oldHandle = _handle;
        LOG_DEBUG("Old Handle: {:X}, ImGui Handle: {:X}", (size_t) oldHandle,
                  (size_t) ImGui::GetMainViewport()->PlatformHandleRaw);
    }

    _handle = InHwnd;
    _isVisible = false;
    _isUWP = isUWP;
    lastPosition = { -1000.0f, -1000.0f };

    LOG_DEBUG("Handle: {0:X}", (size_t) _handle);

    // In case d3d12 wasn't yet used up to this point, try to update GPU info late here
    IdentifyGpu::updateD3d12Capabilities();

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    (void) io;

    hasGamepad = (io.BackendFlags | ImGuiBackendFlags_HasGamepad) > 0;
    io.BackendFlags &= 30;
    io.ConfigFlags = ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoKeyboard;

    io.MouseDrawCursor = _isVisible;
    io.WantCaptureKeyboard = _isVisible;
    io.WantCaptureMouse = _isVisible;
    io.WantSetMousePos = _isVisible;

    io.IniFilename = io.LogFilename = nullptr;

    bool initResult = false;

    if (io.BackendPlatformUserData == nullptr)
    {
        if (!isUWP)
        {
            initResult = ImGui_ImplWin32_Init(InHwnd);
            LOG_DEBUG("ImGui_ImplWin32_Init result: {0}", initResult);
        }
        else
        {
            initResult = ImGui_ImplUwp_Init(InHwnd);
            ImGui_BindUwpKeyUp(KeyUp);
            LOG_DEBUG("ImGui_ImplUwp_Init result: {0}", initResult);
        }
    }

    if (io.Fonts->Fonts.empty() && Config::Instance()->UseHQFont.value_or_default())
    {
        ImFontAtlas* atlas = io.Fonts;
        atlas->Clear();

        // This automatically becomes the next default font
        ImFontConfig fontConfig;

        if (Config::Instance()->FontSize.has_value())
            fontSize = Config::Instance()->FontSize.value();

        // 默认集与简体中文常用集合并，保证中文标签不缺字
        ImFontGlyphRangesBuilder rangeBuilder;
        rangeBuilder.AddRanges(io.Fonts->GetGlyphRangesDefault());
        static const ImWchar cjkRanges[] = { 0x2000, 0x206F, 0x3000, 0x30FF, 0x31F0, 0x31FF, 0xFF00,
                                                     0xFFEF, 0x4E00, 0x9FFF, 0 };
        rangeBuilder.AddRanges(cjkRanges);
        ImVector<ImWchar> mergedRanges;
        rangeBuilder.BuildRanges(&mergedRanges);

        // 微软雅黑加载函数，缺文件时返回空由调用方回退
        auto loadMsyh = [&]() -> ImFont*
        {
            const char* msyhPath = "C:\\Windows\\Fonts\\msyh.ttc";
            if (!std::filesystem::exists(msyhPath))
                return nullptr;
            return atlas->AddFontFromFileTTF(msyhPath, fontSize, &fontConfig, mergedRanges.Data);
        };

        if (Config::Instance()->TTFFontPath.has_value())
        {
            ImFont* customFont = nullptr;
            const auto customPath = Config::Instance()->TTFFontPath.value();
            if (!customPath.empty() && std::filesystem::exists(customPath))
            {
                customFont = atlas->AddFontFromFileTTF(wstring_to_string(customPath).c_str(), fontSize,
                                                       &fontConfig, mergedRanges.Data);
            }
            io.FontDefault = customFont != nullptr ? customFont : loadMsyh();
            if (io.FontDefault == nullptr)
            {
                io.FontDefault = atlas->AddFontFromMemoryCompressedBase85TTF(hack_compressed_compressed_data_base85,
                                                                             fontSize, &fontConfig);
            }
        }
        else
        {
            io.FontDefault = loadMsyh();
            if (io.FontDefault == nullptr)
            {
                io.FontDefault = atlas->AddFontFromMemoryCompressedBase85TTF(hack_compressed_compressed_data_base85,
                                                                             fontSize, &fontConfig);
            }
        }
    }

    if (!Config::Instance()->OverlayMenu.value_or_default())
    {
        _hdrTonemapApplied = false;
    }

    DWORD hwndPid = 0;
    DWORD hwndTid = GetWindowThreadProcessId(_handle, &hwndPid);

    LOG_DEBUG("HWND: {:X}, IsWindow: {}, HWND PID: {}, Current PID: {}, HWND TID: {}, Current TID: {}",
              (ULONG64) _handle, IsWindow(_handle), hwndPid, GetCurrentProcessId(), hwndTid, GetCurrentThreadId());

    OptiInput::Initialize(_handle, isUWP);

    ApplyThemeStyle();
    _isInited = true;
}

void MenuCommon::Shutdown()
{
    if (!MenuCommon::_isInited)
        return;

    // if (_oWndProc != nullptr)
    //{
    //     auto handle = (HWND) ImGui::GetMainViewport()->PlatformHandleRaw;
    //     SetLastError(0);
    //     auto restoreResult = SetWindowLongPtr(handle, GWLP_WNDPROC, (LONG_PTR) _oWndProc);
    //     auto error = GetLastError();

    //    if (restoreResult == 0 && error != 0)
    //    {
    //        LOG_ERROR("Failed to restore old WndProc. Error: {:X}", error);
    //    }

    //    _oWndProc = nullptr;
    //}

    if (!_isUWP)
        ImGui_ImplWin32_Shutdown();
    else
        ImGui_ImplUwp_Shutdown();

    ImGui::DestroyContext();

    _handle = nullptr;
    _isInited = false;
    _isVisible = false;
}

void MenuCommon::HideMenu()
{
    if (!_isVisible)
        return;

    _isVisible = false;

    ImGuiIO& io = ImGui::GetIO();
    (void) io;

    _showMipmapCalcWindow = false;
    _showHudlessWindow = false;

    io.MouseDrawCursor = _isVisible;
    io.WantCaptureKeyboard = _isVisible;
    io.WantCaptureMouse = _isVisible;
}
