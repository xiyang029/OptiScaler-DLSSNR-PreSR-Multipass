#include "pch.h"

#include "DlssNr.h"
#include "DlssNrFeature_Vk.h"
#include "DlssNrFinished_Vk.h"
#include "DlssNr_PipelineUi.h"
#include "DlssNr_Upscaler.h"
#include "DlssNr_MenuSections.h"
#include "DlssNr_Placement.h"
#include <Config.h>
#include <menu/menu_common.h>
#include <algorithm>
#include <cmath>

namespace DlssNr::MenuSections
{

static void RenderPlacement(Config* config)
{
    const bool enabled = config->DlssNrEnabled.value_or_default();
    const bool finishedPicture = config->DlssNrFinishedPicture.value_or_default();
    if (finishedPicture && enabled)
    {
        const auto feature = State::Instance().currentFeature;
        if (State::Instance().swapchainApi == API::Vulkan)
            ImGui::TextWrapped("%s", DlssNr::FinishedVkStatus().c_str());
        else if (feature && (feature->Api() != API::DX12 ||
                             (feature->IsWithDx12() && State::Instance().swapchainApi != API::DX11 &&
                              State::Instance().swapchainInteropApi != SwapchainInteropApi::Dx11wDx12)))
            ImGui::TextWrapped("此选项需要DirectX 12或标记w/Dx12的DirectX 11超分器。");
        else
            ImGui::TextWrapped("%s", DlssNr::FinishedPictureStatus().c_str());
    }

    const auto placement =
        ResolvePlacement(config->DlssNrRunBeforeSr.value_or_default(), config->DlssNrDeferredDlss.value_or_default(),
                         config->DlssNrResidualAcrossRr.value_or_default(), finishedPicture);
    if (placement.deferred)
    {
        ImGui::TextWrapped("独立放大：%s", DlssNr::DeferredDlssStatus().c_str());
        ImGui::TextWrapped(finishedPicture ? "游戏经SR/RR及其特效处理纯净输入， "
                                             "独立放大的NR编辑应用于最终画面。"
                                           : "游戏经SR/RR处理纯净输入，独立放大的NR "
                                             "编辑在放大后应用。");
    }
}

static void RenderStatus(Config* config)
{
    const bool enabled = config->DlssNrEnabled.value_or_default();
    const bool finishedPicture = config->DlssNrFinishedPicture.value_or_default();
    const auto dx12 = ReadStatus(Backend::Dx12);
    const auto vk = ReadStatus(Backend::Vulkan);
    const bool vulkan = vk.running;

    // An existing model handle does not mean NR is enabled this frame.
    if (!enabled)
    {
        ImGui::TextDisabled("NR关闭。");
    }
    else if (!dx12.running && !vulkan)
    {
        const auto feature = State::Instance().currentFeature;
        const bool nativeVk = feature && feature->Api() == API::Vulkan && !feature->IsWithDx12();
        const auto& reason = nativeVk ? vk.failureReason : dx12.failureReason;

        if (!reason.empty())
        {
            ImGui::TextWrapped("%s", reason.c_str());
            ImGui::SameLine();

            if (nativeVk)
                ImGui::TextUnformatted("重启游戏以重试原生Vulkan NR。");
            else if (ImGui::SmallButton("重试"))
                DlssNr::RetryAfterFailure();
        }
        else if (feature && feature->Api() == API::DX11 && !feature->IsWithDx12())
        {
            ImGui::TextWrapped("D3D11上NR需要D3D12桥接，请选择标记w/Dx12的超分器并重启。");
        }
        else if (nativeVk && ResolvePlacement(config->DlssNrRunBeforeSr.value_or_default(),
                                              config->DlssNrDeferredDlss.value_or_default(),
                                              config->DlssNrResidualAcrossRr.value_or_default(), finishedPicture)
                                 .deferred)
        {
            ImGui::TextWrapped("独立编辑放大路径需要DirectX 12或其桥接，请关闭独立编辑 "
                               "放大以使用原生Vulkan NR。");
        }
        else
            ImGui::TextUnformatted("等待超分器运行。");
    }
    else
    {
        const auto ms = vulkan ? vk.gpuTime : dx12.gpuTime;

        // Hiding the edit keeps model evaluation running.
        const char* runSuffix = !config->DlssNrApplyModel.value_or_default() ? "（模型运行中，编辑隐藏）" : "";

        // Keep the running indicator green, using the theme's HDR-adjusted text brightness.
        const auto textColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ImVec4(textColor.x * 0.55f, textColor.y * 0.80f, textColor.z * 0.55f, textColor.w));
        if (ms.has_value())
            ImGui::Text("运行中%s-已用%.2f毫秒%s", vulkan ? "（原生Vulkan）" : "", ms.value(), runSuffix);
        else if (vulkan)
            // Measured but not yet read: the first few frames are still in the query ring.
            ImGui::Text("原生Vulkan运行中-%llu帧%s", vk.frames, runSuffix);
        else
            ImGui::Text("运行中。%s", runSuffix);
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("NR在GPU上起止时间，含其他任务等待延迟， "
                              "对比FPS查看对游戏性能的影响。");

        if (ms.has_value())
        {
            const auto& state = State::Instance();
            // The FG swapchain interval is between real game frames, not interpolated presents.
            // Native Vulkan has no DXGI timing, so use the existing overlay frame interval there.
            const double frameMs = state.swapchainApi == API::Vulkan
                                       ? (state.frameTimes.empty() ? 0.0 : state.frameTimes.back())
                                   : state.currentFG ? state.lastFGFrameTime
                                                     : state.presentFrameTime;
            PipelineUi::DrawTimingBar(ms.value(), frameMs);
        }
    }
}

} // namespace DlssNr::MenuSections

namespace DlssNr
{

void RenderMenu(Config* config, float menuResScale)
{
    using namespace MenuSections;
    ImGui::Spacing();
    if (auto header = ScopedCollapsingHeader("DLSS神经渲染"); header.IsHeaderOpen())
    {
        ScopedIndent indent {};
        const float toggleGap = ImGui::GetStyle().ItemSpacing.x;
        const float toggleWidth = (ImGui::GetContentRegionAvail().x - toggleGap) * 0.5f;
        const float toggleRight = ImGui::GetCursorPosX() + toggleWidth + toggleGap;
        bool enabled = config->DlssNrEnabled.value_or_default();
        if (PipelineUi::CheckboxWrapped("启用神经渲染", &enabled, toggleWidth))
            config->DlssNrEnabled = enabled;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("启用NR处理。");

        bool applyModel = config->DlssNrApplyModel.value_or_default();
        if (PipelineUi::CheckboxWrapped("应用模型", &applyModel, toggleWidth))
            config->DlssNrApplyModel = applyModel;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("显示或隐藏NR效果，隐藏时模型仍运行。关闭启用神经渲染 "
                              "以停止GPU开销。");

        const auto feature = State::Instance().currentFeature;
        const bool rayReconstruction = feature && feature->GetUpscalerType() == Upscaler::DLSSD;
        bool finished = config->DlssNrFinishedPicture.value_or_default();
        auto placement = ResolvePlacement(config->DlssNrRunBeforeSr.value_or_default(),
                                          config->DlssNrDeferredDlss.value_or_default(),
                                          config->DlssNrResidualAcrossRr.value_or_default(), finished);
        bool generateBefore = placement.beforeUpscale;
        const bool denoiseFirstOn = config->DlssNrDenoiseFirst.value_or_default();
        ImGui::SameLine(toggleRight);
        ImGui::BeginDisabled(placement.deferred || denoiseFirstOn);
        if (PipelineUi::CheckboxWrapped("超分前生成模型", &generateBefore, toggleWidth))
        {
            config->DlssNrRunBeforeSr = generateBefore;
            config->DlssNrResidualAcrossRr = false;
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip(placement.deferred ? "独立编辑路径始终在超分前生成。"
                              : denoiseFirstOn   ? "「先去噪」开启时由它决定处理位置。"
                                                 : "在游戏超分器（含RR）之前运行NR。");

        if (PipelineUi::CheckboxWrapped("将NR应用于最终画面", &finished, toggleWidth))
        {
            config->DlssNrFinishedPicture = finished;
            DlssNr::RetryAfterFailure();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "在游戏特效和HUD后应用NR，早期生成经独立超分器传递编辑。");

        placement = ResolvePlacement(config->DlssNrRunBeforeSr.value_or_default(),
                                     config->DlssNrDeferredDlss.value_or_default(),
                                     config->DlssNrResidualAcrossRr.value_or_default(), finished);
        ImGui::SameLine(toggleRight);
        bool deferred = placement.deferred;
        if (PipelineUi::CheckboxWrapped("超分前生成，超分后应用", &deferred, toggleWidth))
        {
            config->DlssNrDeferredDlss = deferred;
            config->DlssNrResidualAcrossRr = false; // Clear the legacy alias when the unified option changes.
            if (deferred || finished)
                config->DlssNrRunBeforeSr = deferred;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "保持游戏SR/RR输入纯净，仅用独立非RR后端放大NR编辑。"
                "超分后应用，终画面模式下在呈现时应用。");
        ImGui::Spacing();

        // Denoise first. Written for people who did not design it: what it does, what it costs, what each knob is.
        placement = ResolvePlacement(config->DlssNrRunBeforeSr.value_or_default(),
                                     config->DlssNrDeferredDlss.value_or_default(),
                                     config->DlssNrResidualAcrossRr.value_or_default(), finished);
        const bool denoiseBlocked = placement.deferred || finished;
        bool denoiseFirst = config->DlssNrDenoiseFirst.value_or_default();
        ImGui::BeginDisabled(denoiseBlocked);
        if (PipelineUi::CheckboxWrapped("先在原生分辨率去噪，再NR，再超分", &denoiseFirst, toggleWidth))
            config->DlssNrDenoiseFirst = denoiseFirst;
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip(
                denoiseBlocked
                    ? "请先关闭独立编辑和终画面选项。"
                    : "在渲染分辨率下再跑一次游戏自带超分器（游戏用RR则用RR，否则DLSS），让NR面对的是干净、"
                      "已去噪、稳定的画面，而不是带噪点和抖动的原始渲染。\nNR在这张画面上做编辑；下方第二步决定"
                      "编辑如何送到屏幕。\n仅限原生DirectX 12。每帧多一次超分开销。");
        if (denoiseFirst && !denoiseBlocked)
        {
            ScopedIndent denoiseIndent {};
            int step = std::clamp(config->DlssNrDenoiseFirstStep.value_or_default(), 0, 2);
            if (ImGui::Combo("第二步", &step,
                             "再次调用游戏超分器，抖动归零\0独立DLSS SR，抖动归零\0把编辑叠加到原始渲染，"
                             "再交给游戏超分器\0"))
                config->DlssNrDenoiseFirstStep = step;
            HelpMarker(
                "再次调用游戏超分器：NR处理后的干净画面交给游戏超分器。实现简单，但第二次超分拿不到真实的"
                "子像素采样，可能比原生更软。\n\n"
                "独立DLSS SR：同上，但由一次独立的普通DLSS超分完成并替换输出，避免重复运行光线重建。\n\n"
                "把编辑叠加到原始渲染：只从干净画面取出NR的变化量，按原始渲染的抖动对齐后叠加到其上。"
                "游戏超分器拿到的就是携带编辑的真实采样。理论上最锐利；光线重建可能削弱部分编辑。");
            if (step == 2)
            {
                int edit = config->DlssNrDenoiseFirstEdit.value_or_default() == 1 ? 1 : 0;
                if (ImGui::Combo("编辑形式", &edit, "差值（相加）\0比例（相乘）\0"))
                    config->DlssNrDenoiseFirstEdit = edit;
                HelpMarker("比例：每个原始像素乘以 NR/干净，噪点采样各自保留色相，只有亮度和色调变化。推荐。\n"
                           "差值：把 NR 减干净 作为绝对量相加。对于本身很暗的采样，这个量会超过采样值，使其偏色，"
                           "并让超分器收到的色彩噪声大约翻倍。");
                if (edit == 1)
                {
                    bool fireflies = config->DlssNrDenoiseFirstFireflyGuard.value_or_default();
                    if (ImGui::Checkbox("不放大萤火虫噪点", &fireflies))
                        config->DlssNrDenoiseFirstFireflyGuard = fireflies;
                    HelpMarker("比干净值亮得多的原始采样是路径追踪萤火虫，而不是受光像素。按比例相乘会让它更亮，"
                               "镜头静止时游戏的光线重建会把这些亮斑留在阴影里。开启后这些采样改用该像素本应"
                               "得到的绝对变化量。关闭可作对比。");
                }

                bool shift = config->DlssNrDenoiseFirstShift.value_or_default();
                if (ImGui::Checkbox("按该帧抖动偏移编辑", &shift))
                    config->DlssNrDenoiseFirstShift = shift;
                HelpMarker("干净画面位于稳定的像素网格上；原始渲染每帧都有一个子像素抖动偏移。"
                           "按该抖动偏移编辑可使两者对齐。仅在对比时关闭。");
                if (shift)
                {
                    bool flip = config->DlssNrDenoiseFirstFlipJitter.value_or_default();
                    if (ImGui::Checkbox("反转抖动方向", &flip))
                        config->DlssNrDenoiseFirstFlipJitter = flip;
                    HelpMarker("各引擎对抖动值指向的定义不一致。方向错误会让错位翻倍而非消除。"
                               "若开启偏移后边缘反而比关闭更差，就翻转此项。");
                    int kernel = std::clamp(config->DlssNrDenoiseFirstKernel.value_or_default(), 0, 2);
                    if (ImGui::Combo("编辑重采样", &kernel, "Bilinear\0Catmull-Rom\0Lanczos 2\0"))
                        config->DlssNrDenoiseFirstKernel = kernel;
                    HelpMarker("偏移位置上编辑的插值方式。双线性偏软且不会过冲。Catmull-Rom 锐利、过冲很小。"
                               "Lanczos 2 最锐利，但硬边缘周围可能出现振铃。");
                    bool clampEdit = config->DlssNrDenoiseFirstNeighbourhoodClamp.value_or_default();
                    if (ImGui::Checkbox("把编辑限制在邻域范围内", &clampEdit))
                        config->DlssNrDenoiseFirstNeighbourhoodClamp = clampEdit;
                    HelpMarker("把插值后的编辑限制在最近四个源像素的取值范围内。消除较锐核的振铃，"
                               "代价是略微损失锐度。");
                }
            }
            ImGui::TextWrapped("先去噪：%s", DlssNr::DenoiseFirstStatus().c_str());
            ImGui::Spacing();
        }

        // placement is current: nothing it depends on changes in the denoise-first block above.
        const bool nativePrivateVk = feature && feature->Api() == API::Vulkan && !feature->IsWithDx12();
        if (placement.deferred && !nativePrivateVk)
        {
            int backend = (int) GetPrivateUpscaler(config->DlssNrPrivateUpscaler.value_or_default());
            if (ImGui::Combo("独立NR超分器", &backend, "DLSS\0FSR 2.2\0FSR (FidelityFX)\0XeSS\0"))
                config->DlssNrPrivateUpscaler = backend;
            HelpMarker(
                "仅放大NR编辑，游戏RR可有可无，FSR与XeSS需对应运行时。");
        }

        PipelineUi::View view;
        view.privateUpscaler =
            PrivateUpscalerName(GetPrivateUpscaler(config->DlssNrPrivateUpscaler.value_or_default()));
        view.enabled = enabled;
        view.applyModel = config->DlssNrApplyModel.value_or_default();
        view.passes = config->DlssNrPasses.value_or_default();
        view.scalePercent = (int) lroundf(config->DlssNrWorkingScale.value_or_default() * 100.0f);
        view.rayReconstruction = rayReconstruction;
        if (finished)
            view.route = placement.deferred ? PipelineUi::Route::FinishedBefore : PipelineUi::Route::Finished;
        else if (placement.deferred)
            view.route = PipelineUi::Route::Deferred;
        else
            view.route = placement.beforeUpscale ? PipelineUi::Route::Before : PipelineUi::Route::After;

        static PipelineUi::Section selected = PipelineUi::Section::Placement;
        ImGui::Separator();
        PipelineUi::Draw(view, selected);
        ImGui::Separator();
        ImGui::Spacing();
        RenderStatus(config);
        ImGui::SeparatorText(PipelineUi::SectionName(selected));
        ImGui::PushItemWidth(std::min(220.0f * menuResScale, ImGui::GetContentRegionAvail().x * 0.42f));
        static constexpr void (*sections[])(Config*) = { RenderPlacement, RenderInput, RenderModel, RenderBlend };
        sections[(int) selected](config);
        ImGui::PopItemWidth();
        if (ImGui::CollapsingHeader("检查NR"))
        {
            ImGui::PushItemWidth(std::min(220.0f * menuResScale, ImGui::GetContentRegionAvail().x * 0.42f));
            RenderInspect(config);
            ImGui::PopItemWidth();
        }
    }
}

} // namespace DlssNr
