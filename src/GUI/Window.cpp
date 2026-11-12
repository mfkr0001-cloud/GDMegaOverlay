
#include "Window.h"
#include "GUI.h"
#include "Settings.hpp"
#include "Blur.h"

#include <imgui_internal.h>
#include <algorithm>
#include <cmath>

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace GUI {

float roundSnap(float n, int snap) {
    if (snap <= 0)
        return n;

    int a = (static_cast<int>(n) / snap) * snap;
    int b = a + snap;

    return static_cast<float>((n - a > b - n) ? b : a);
}


#if defined(__ANDROID__)
// Android-only helpers. They are also used by GUI.cpp (declared there).

// Real size of the screen in ImGui coordinates. Falls back to the cocos
// frame size while ImGui has not received a display size yet.
ImVec2 androidScreenSize() {
    ImVec2 screen = ImGui::GetIO().DisplaySize;
    if (screen.x > 1.f && screen.y > 1.f)
        return screen;

    auto* director = CCDirector::sharedDirector();
    if (director && director->getOpenGLView()) {
        auto frame = director->getOpenGLView()->getFrameSize();
        if (frame.width > 1.f && frame.height > 1.f)
            return ImVec2(frame.width, frame.height);
    }
    return ImVec2(0.f, 0.f);
}

static bool finiteVec(const ImVec2& v) {
    return std::isfinite(v.x) && std::isfinite(v.y);
}

struct AndroidLimits {
    float margin;
    float minW, maxW;
    float minH, maxH;
};

static AndroidLimits androidLimits(const Window& w, const ImVec2& screen) {
    AndroidLimits l;
    l.margin = std::max(2.f, screen.x * 0.005f);

    const float availW = std::max(40.f, screen.x - l.margin * 2.f);
    const float availH = std::max(40.f, screen.y - l.margin * 2.f);

    // A window is never narrower than ~18% of the screen (the old limit was
    // ~10%, which produced unreadable columns) and never wider than 60%.
    l.minW = std::min(availW, screen.x * 0.18f);
    l.maxW = std::min(availW, std::max(l.minW, screen.x * 0.60f));

    l.minH = std::min(availH, std::max(screen.y * 0.12f, w.minSize.y * GUI::uiSizeFactor));
    l.maxH = availH;
    if (l.minH > l.maxH)
        l.minH = l.maxH;

    return l;
}

// Limits size and position of a window to the real screen.
// Returns true when something had to be corrected.
bool fitWindowToScreen(Window& w) {
    const ImVec2 screen = androidScreenSize();
    if (screen.x < 100.f || screen.y < 100.f)
        return false;

    const AndroidLimits l = androidLimits(w, screen);

    const ImVec2 oldPos = w.position;
    const ImVec2 oldSize = w.size;
    const bool renderWasSettled =
        std::fabs(w.renderPosition.x - oldPos.x) < 0.5f &&
        std::fabs(w.renderPosition.y - oldPos.y) < 0.5f;

    // 1) size
    ImVec2 size = w.size;
    if (!finiteVec(size) || size.x <= 0.f || size.y <= 0.f)
        size = ImVec2(screen.x * 0.235f, screen.y * 0.31f);

    size.x = std::clamp(size.x, l.minW, l.maxW);
    size.y = std::clamp(size.y, l.minH, l.maxH);

    // 2) position (after the size is known)
    ImVec2 pos = w.position;
    if (!finiteVec(pos))
        pos = ImVec2(l.margin, l.margin);

    const float maxX = std::max(l.margin, screen.x - size.x - l.margin);
    const float maxY = std::max(l.margin, screen.y - size.y - l.margin);
    pos.x = std::clamp(pos.x, l.margin, maxX);
    pos.y = std::clamp(pos.y, l.margin, maxY);

    const bool changed =
        pos.x != oldPos.x || pos.y != oldPos.y ||
        size.x != oldSize.x || size.y != oldSize.y;

    w.size = size;
    w.position = pos;

    // Do not touch renderPosition while the open/close animation is running
    // (it is intentionally off-screen then); only follow the target when the
    // window was resting at its old position.
    if (renderWasSettled || !finiteVec(w.renderPosition))
        w.renderPosition = pos;

    return changed;
}
#endif

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
#if defined(__ANDROID__)
    // Fix size and coordinates of EVERY window before the visibility check,
    // so old layouts (desktop pixels, narrow columns, off-screen windows) are
    // always brought back to the real screen.
    fitWindowToScreen(*this);
#endif
    if (GUI::isVisible && visibleInScreen()) {
#if defined(__ANDROID__)
        {
            const ImVec2 screen = androidScreenSize();
            if (screen.x > 100.f && screen.y > 100.f) {
                const AndroidLimits l = androidLimits(*this, screen);
                ImGui::SetNextWindowSizeConstraints(
                    {l.minW, l.minH},
                    {l.maxW, l.maxH}
                );
            }
        }
#else
        if (maxSize.x != 0 && maxSize.y != 0) {
            const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
            const float minWidth = displaySize.x * 0.098f;
            const float maxWidth = displaySize.x * 0.112f;
            ImGui::SetNextWindowSizeConstraints(
                {minWidth, minSize.y * GUI::uiSizeFactor},
                {maxWidth, maxSize.y * GUI::uiSizeFactor}
            );
        }
#endif

        bool blurEnabled = Settings::get<bool>("menu/blur/enabled", false);
        bool blurGD = Settings::get<bool>("menu/blur/gd", false);
        float windowTransparency = Settings::get<float>("menu/window/opacity", 0.98f);
        float blurDarkness = Settings::get<float>("menu/blur/darkness", 1.f);

        if (blurEnabled)
            ImGui::SetNextWindowBgAlpha(blurGD ? (blurDarkness - 0.1f) : 1.f);
        else
            ImGui::SetNextWindowBgAlpha(windowTransparency);

        ImGui::Begin(name.c_str(), nullptr, flags);

        if (Settings::get<bool>("menu/title_gradient/enabled", true))
            customTitlebar();

        if (blurEnabled && !blurGD)
            Blur::blurWindowBackground();

        if (Settings::get<bool>("menu/box_shadow/enabled", true))
            dropShadow(Settings::get<float>("menu/box_shadow/size", 24.f), 255);

        // Keep function labels compact inside the narrow Android columns.
        // UI Scale still works as before, with an additional 20% text reduction.
        ImGui::SetWindowFontScale(
            Settings::get<float>("menu/ui_scale", 1.f) * 0.80f
        );

        if (ImGui::IsMouseDragging(0, 0.1f) &&
            (ImGui::IsWindowFocused() ||
             ImGui::IsMouseHoveringRect(
                 ImGui::GetWindowPos(),
                 ImGui::GetWindowPos() + ImGui::GetWindowSize()
             ))) {

            position = ImGui::GetWindowPos();
            size = ImGui::GetWindowSize();

            int snap = Settings::get<int>("menu/window/snap", 10);
            if (snap > 1)
                position = {
                    roundSnap(position.x, snap),
                    roundSnap(position.y, snap)
                };

            int sizeSnap = Settings::get<int>("menu/window/size_snap", 10);
            if (sizeSnap > 1)
                size = {
                    roundSnap(size.x, sizeSnap),
                    roundSnap(size.y, sizeSnap)
                };

#if defined(__ANDROID__)
            // Keep dragged / resized windows inside the screen (after snapping).
            fitWindowToScreen(*this);
#endif
            renderPosition = position;

            GUI::setJsonPosition(name, position);
            GUI::setJsonSize(name, size);
        }

        ImGui::SetWindowSize(size);
        ImGui::SetWindowPos(renderPosition);
    }

    if ((GUI::isVisible && visibleInScreen()) || !GUI::isVisible)
        windowFunction();

    if (GUI::isVisible && visibleInScreen())
        ImGui::End();
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

} // namespace GUI
