
#include "Window.h"
#include "GUI.h"
#include "Settings.hpp"
#include "Blur.h"

#include <imgui_internal.h>

#include <Geode/Geode.hpp>

using namespace geode::prelude;
using namespace GUI;

float roundSnap(float n, int snap) {
    if (snap <= 0)
        return n;

    int a = (static_cast<int>(n) / snap) * snap;
    int b = a + snap;

    return static_cast<float>((n - a > b - n) ? b : a);
}

Window::Window(const std::string& name, const std::function<void()>& windowFunction) {
    this->name = name;
    this->windowFunction = windowFunction;
}

void Window::addFlag(ImGuiWindowFlags flag) {
    flags |= flag;
}

bool Window::visibleInScreen() {
    ImVec2 screenSize = ImGui::GetIO().DisplaySize;
    ImVec2 windowSize = size;

    return renderPosition.x + windowSize.x >= 0 &&
           renderPosition.y + windowSize.y >= 0 &&
           renderPosition.x <= screenSize.x &&
           renderPosition.y <= screenSize.y;
}

void Window::draw() {
    if (GUI::isVisible && visibleInScreen()) {
        if (maxSize.x != 0 && maxSize.y != 0) {
            ImGui::SetNextWindowSizeConstraints(
                {minSize.x * GUI::uiSizeFactor, minSize.y * GUI::uiSizeFactor},
                {maxSize.x * GUI::uiSizeFactor, maxSize.y * GUI::uiSizeFactor}
            );
        }

        // Apply the saved geometry only on first creation. Reapplying SetWindowSize
        // every frame prevents ImGui's native resize handle from working correctly.
        const ImGuiCond geometryCondition = geometryDirty ? ImGuiCond_Always : ImGuiCond_Once;
        ImGui::SetNextWindowPos(position, geometryCondition);
        ImGui::SetNextWindowSize(size, geometryCondition);

        bool blurEnabled = Settings::get<bool>("menu/blur/enabled", false);
        bool blurGD = Settings::get<bool>("menu/blur/gd", false);
        float windowTransparency = Settings::get<float>("menu/window/opacity", 0.98f);
        float blurDarkness = Settings::get<float>("menu/blur/darkness", 1.f);

        if (blurEnabled)
            ImGui::SetNextWindowBgAlpha(blurGD ? (blurDarkness - 0.1f) : 1.f);
        else
            ImGui::SetNextWindowBgAlpha(windowTransparency);

        const bool windowOpen = ImGui::Begin(name.c_str(), nullptr, flags);

        if (windowOpen) {
            if (Settings::get<bool>("menu/title_gradient/enabled", true))
                customTitlebar();

            if (blurEnabled && !blurGD)
                Blur::blurWindowBackground();

            if (Settings::get<bool>("menu/box_shadow/enabled", true))
                dropShadow(Settings::get<float>("menu/box_shadow/size", 24.f), 255);

            ImGui::SetWindowFontScale(ImClamp(fontScale, 0.5f, 2.5f));
            windowFunction();
        }

        // Read actual ImGui geometry after its move/resize handling (also works with touch input).
        geometryDirty = false;
        position = ImGui::GetWindowPos();
        renderPosition = position;
        size = ImGui::GetWindowSize();
        GUI::setJsonPosition(name, position);
        GUI::setJsonSize(name, size);
        ImGui::End();
    }
    else if (!GUI::isVisible) {
        windowFunction();
    }
}

// Adapted from Dear ImGui window title bar implementation.
void drawWindowTitleBarContents(
    ImGuiWindow* window,
    const ImRect& title_bar_rect,
    const char* name,
    bool* p_open
) {
    ImGuiContext& g = *GImGui;
    ImGuiStyle& style = g.Style;
    ImGuiWindowFlags flags = window->Flags;

    const bool has_close_button = (p_open != nullptr);
    const bool has_collapse_button =
        !(flags & ImGuiWindowFlags_NoCollapse) &&
        (style.WindowMenuButtonPosition != ImGuiDir_None);

    const ImGuiItemFlags item_flags_backup = g.CurrentItemFlags;
    g.CurrentItemFlags |= ImGuiItemFlags_NoNavDefaultFocus;
    window->DC.NavLayerCurrent = ImGuiNavLayer_Menu;

    float pad_l = style.FramePadding.x;
    float pad_r = style.FramePadding.x;
    float button_sz = g.FontSize;

    ImVec2 close_button_pos;
    ImVec2 collapse_button_pos;

    if (has_close_button) {
        pad_r += button_sz;
        close_button_pos = ImVec2(
            title_bar_rect.Max.x - pad_r - style.FramePadding.x,
            title_bar_rect.Min.y
        );
    }

    if (has_collapse_button &&
        style.WindowMenuButtonPosition == ImGuiDir_Right) {
        pad_r += button_sz;
        collapse_button_pos = ImVec2(
            title_bar_rect.Max.x - pad_r - style.FramePadding.x,
            title_bar_rect.Min.y
        );
    }

    if (has_collapse_button &&
        style.WindowMenuButtonPosition == ImGuiDir_Left) {
        collapse_button_pos = ImVec2(
            title_bar_rect.Min.x + pad_l - style.FramePadding.x,
            title_bar_rect.Min.y
        );
        pad_l += button_sz;
    }

    if (has_collapse_button) {
        if (ImGui::CollapseButton(
                window->GetID("#COLLAPSE"),
                collapse_button_pos
            )) {
            window->WantCollapseToggle = true;
        }
    }

    if (has_close_button) {
        if (ImGui::CloseButton(
                window->GetID("#CLOSE"),
                close_button_pos
            )) {
            *p_open = false;
        }
    }

    window->DC.NavLayerCurrent = ImGuiNavLayer_Main;
    g.CurrentItemFlags = item_flags_backup;

    const float marker_size_x =
        (flags & ImGuiWindowFlags_UnsavedDocument)
            ? button_sz * 0.80f
            : 0.0f;

    const ImVec2 text_size =
        ImGui::CalcTextSize(name, nullptr, true) +
        ImVec2(marker_size_x, 0.0f);

    if (pad_l > style.FramePadding.x)
        pad_l += g.Style.ItemInnerSpacing.x;

    if (pad_r > style.FramePadding.x)
        pad_r += g.Style.ItemInnerSpacing.x;

    if (style.WindowTitleAlign.x > 0.0f &&
        style.WindowTitleAlign.x < 1.0f) {

        float centerness = ImSaturate(
            1.0f - ImFabs(style.WindowTitleAlign.x - 0.5f) * 2.0f
        );

        float pad_extend = ImMin(
            ImMax(pad_l, pad_r),
            title_bar_rect.GetWidth() - pad_l - pad_r - text_size.x
        );

        pad_l = ImMax(pad_l, pad_extend * centerness);
        pad_r = ImMax(pad_r, pad_extend * centerness);
    }

    ImRect layout_r(
        title_bar_rect.Min.x + pad_l,
        title_bar_rect.Min.y,
        title_bar_rect.Max.x - pad_r,
        title_bar_rect.Max.y
    );

    ImRect clip_r(
        layout_r.Min.x,
        layout_r.Min.y,
        ImMin(
            layout_r.Max.x + g.Style.ItemInnerSpacing.x,
            title_bar_rect.Max.x
        ),
        layout_r.Max.y
    );

    if (flags & ImGuiWindowFlags_UnsavedDocument) {
        ImVec2 marker_pos;

        marker_pos.x = ImClamp(
            layout_r.Min.x +
                (layout_r.GetWidth() - text_size.x) *
                    style.WindowTitleAlign.x +
                text_size.x,
            layout_r.Min.x,
            layout_r.Max.x
        );

        marker_pos.y =
            (layout_r.Min.y + layout_r.Max.y) * 0.5f;

        if (marker_pos.x > layout_r.Min.x) {
            ImGui::RenderBullet(
                window->DrawList,
                marker_pos,
                ImGui::GetColorU32(ImGuiCol_Text)
            );

            clip_r.Max.x = ImMin(
                clip_r.Max.x,
                marker_pos.x - (int)(marker_size_x * 0.5f)
            );
        }
    }

    ImGui::RenderTextClipped(
        layout_r.Min,
        layout_r.Max,
        name,
        nullptr,
        &text_size,
        style.WindowTitleAlign,
        &clip_r
    );
}

void Window::customTitlebar() {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    draw_list->PushClipRectFullScreen();

    ImVec2 min = ImGui::GetCurrentWindow()->TitleBarRect().Min;
    ImVec2 max = ImGui::GetCurrentWindow()->TitleBarRect().Max;

    auto col_bg = ImGui::GetStyleColorVec4(ImGuiCol_TitleBg);

    ImVec4 bg_color_1 = {
        col_bg.x * 0.5f,
        col_bg.y * 1.5f,
        col_bg.z * 0.75f,
        col_bg.w
    };

    ImVec4 bg_color_2 = {
        col_bg.x * 1.5f,
        col_bg.y * 0.5f,
        col_bg.z * 1.25f,
        col_bg.w
    };

    ImGuiContext& g = *GImGui;

    int vert_start_idx = draw_list->VtxBuffer.Size;

    draw_list->AddRectFilled(
        min,
        max,
        ImGui::GetColorU32(bg_color_1),
        g.Style.FrameRounding
    );

    int vert_end_idx = draw_list->VtxBuffer.Size;

    ImGui::ShadeVertsLinearColorGradientKeepAlpha(
        draw_list,
        vert_start_idx,
        vert_end_idx,
        min,
        max,
        ImGui::GetColorU32(bg_color_1),
        ImGui::GetColorU32(bg_color_2)
    );

    if (g.Style.FrameBorderSize > 0.0f) {
        draw_list->AddRect(
            min,
            max,
            ImGui::GetColorU32(ImGuiCol_Border),
            g.Style.FrameRounding,
            0,
            g.Style.FrameBorderSize
        );
    }

    drawWindowTitleBarContents(
        ImGui::GetCurrentWindow(),
        ImGui::GetCurrentWindow()->TitleBarRect(),
        ImGui::GetCurrentWindow()->Name,
        nullptr
    );

    draw_list->PopClipRect();
}

void Window::dropShadow(float size, ImU8 opacity) {
    if (!GUI::shadowTexture)
        return;

    auto tex_id = (ImTextureID)GUI::shadowTexture->getName();

    ImVec2 p = ImGui::GetWindowPos();
    ImVec2 s = ImGui::GetWindowSize();
    ImVec2 m = {p.x + s.x, p.y + s.y};

    float uv0 = 0.0f;
    float uv1 = 0.333333f;
    float uv2 = 0.666666f;
    float uv3 = 1.0f;

    ImU32 col = (opacity << 24) | 0xFFFFFF;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->PushClipRectFullScreen();

    dl->AddImage(
        tex_id,
        {p.x - size, p.y - size},
        {p.x, p.y},
        {uv0, uv0},
        {uv1, uv1},
        col
    );

    dl->AddImage(
        tex_id,
        {p.x, p.y - size},
        {m.x, p.y},
        {uv1, uv0},
        {uv2, uv1},
        col
    );

    dl->AddImage(
        tex_id,
        {m.x, p.y - size},
        {m.x + size, p.y},
        {uv2, uv0},
        {uv3, uv1},
        col
    );

    dl->AddImage(
        tex_id,
        {p.x - size, p.y},
        {p.x, m.y},
        {uv0, uv1},
        {uv1, uv2},
        col
    );

    dl->AddImage(
        tex_id,
        {m.x, p.y},
        {m.x + size, m.y},
        {uv2, uv1},
        {uv3, uv2},
        col
    );

    dl->AddImage(
        tex_id,
        {p.x - size, m.y},
        {p.x, m.y + size},
        {uv0, uv2},
        {uv1, uv3},
        col
    );

    dl->AddImage(
        tex_id,
        {p.x, m.y},
        {m.x, m.y + size},
        {uv1, uv2},
        {uv2, uv3},
        col
    );

    dl->AddImage(
        tex_id,
        {m.x, m.y},
        {m.x + size, m.y + size},
        {uv2, uv2},
        {uv3, uv3},
        col
    );

    dl->PopClipRect();
}
