
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

// Размер экрана в координатах ImGui.
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

// Не меняет заданные размеры окна.
// Только исправляет некорректные значения и не позволяет
// окну полностью уйти за пределы экрана.
bool fitWindowToScreen(Window& w) {
    const ImVec2 screen = androidScreenSize();

    if (screen.x < 100.f || screen.y < 100.f)
        return false;

    const ImVec2 oldPos = w.position;
    const ImVec2 oldSize = w.size;

    ImVec2 size = w.size;

    if (!finiteVec(size) || size.x <= 0.f || size.y <= 0.f)
        size = ImVec2(screen.x * 0.235f, screen.y * 0.31f);

    // Размер не ограничиваем процентами экрана.
    // Если окно больше экрана, разрешаем ему оставаться большим,
    // чтобы не ломать исходные пропорции раскладки.
    w.size = size;

    ImVec2 pos = w.position;

    if (!finiteVec(pos))
        pos = ImVec2(2.f, 2.f);

    const float margin = 2.f;

    const float maxX = std::max(
        margin,
        screen.x - std::min(size.x, screen.x - margin) + margin
    );

    const float maxY = std::max(
        margin,
        screen.y - std::min(size.y, screen.y - margin) + margin
    );

    pos.x = std::clamp(pos.x, margin, maxX);
    pos.y = std::clamp(pos.y, margin, maxY);

    w.position = pos;

    const bool renderWasSettled =
        std::fabs(w.renderPosition.x - oldPos.x) < 0.5f &&
        std::fabs(w.renderPosition.y - oldPos.y) < 0.5f;

    if (renderWasSettled || !finiteVec(w.renderPosition))
        w.renderPosition = pos;

    return
        pos.x != oldPos.x ||
        pos.y != oldPos.y ||
        size.x != oldSize.x ||
        size.y != oldSize.y;
}

#endif

Window::Window(
    const std::string& name,
    const std::function<void()>& windowFunction
) {
    this->name = name;
    this->windowFunction = windowFunction;
}

void Window::addFlag(ImGuiWindowFlags flag) {
    flags |= flag;
}

// Прокрутка содержимого окна пальцем (вертикально).
// Вызывать между ImGui::Begin и ImGui::End.
void Window::handleTouchScroll() {
    ImGuiIO& io = ImGui::GetIO();

    if (ImGui::IsMouseClicked(0)) {
        // Касание началось в этом окне (и не в открытом попапе поверх него).
        const ImVec2 mouse = ImGui::GetMousePos();
        const ImVec2 windowPos = ImGui::GetWindowPos();
        const float titleBottom = windowPos.y + ImGui::GetFrameHeight();
        touchScrollCandidate = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
            mouse.y > titleBottom;
        touchScrolling = false;
    }

    if (!ImGui::IsMouseDown(0)) {
        touchScrollCandidate = false;
        touchScrolling = false;
        return;
    }

    if (!touchScrollCandidate)
        return;

    if (!touchScrolling) {
        const ImVec2 d = ImGui::GetMouseDragDelta(0, 0.f);
        const float threshold = std::max(6.f, ImGui::GetFontSize() * 0.5f);

        // Начинаем скролл только при явном вертикальном движении,
        // чтобы не мешать слайдерам и обычным нажатиям.
        if (std::fabs(d.y) > threshold && std::fabs(d.y) > std::fabs(d.x)) {
            touchScrolling = true;

            // Отменяем нажатие на чекбокс/кнопку под пальцем,
            // иначе при отпускании функция включится случайно.
            ImGui::ClearActiveID();
        }
    }

    if (touchScrolling)
        ImGui::SetScrollY(ImGui::GetScrollY() - io.MouseDelta.y);
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
    fitWindowToScreen(*this);
#endif

    if (!GUI::isVisible || !visibleInScreen())
        return;

    const ImVec2 screen = ImGui::GetIO().DisplaySize;
#if defined(__ANDROID__)
    // Keep every panel usable on small screens, but let the user resize it.
    const float minW = std::min(115.f, std::max(80.f, screen.x - 8.f));
    const float minH = std::min(90.f, std::max(60.f, screen.y - 8.f));
    const float maxW = std::max(minW, screen.x - 4.f);
    const float maxH = std::max(minH, screen.y - 4.f);
    ImGui::SetNextWindowSizeConstraints(ImVec2(minW, minH), ImVec2(maxW, maxH));
#endif

    ImGui::SetNextWindowPos(renderPosition, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);

    const bool blurEnabled = Settings::get<bool>("menu/blur/enabled", false);
    const bool blurGD = Settings::get<bool>("menu/blur/gd", false);
    const float windowTransparency = Settings::get<float>("menu/window/opacity", 0.98f);
    const float blurDarkness = Settings::get<float>("menu/blur/darkness", 1.f);

    ImGui::SetNextWindowBgAlpha(
        blurEnabled ? (blurGD ? (blurDarkness - 0.1f) : 1.f) : windowTransparency
    );

    // Android supports touch dragging on the title bar and resizing from the
    // lower-right grip. Content itself remains vertically scrollable.
    bool contentsVisible = ImGui::Begin(name.c_str(), nullptr, flags);

    if (contentsVisible) {
        if (Settings::get<bool>("menu/title_gradient/enabled", true))
            customTitlebar();

        if (blurEnabled && !blurGD)
            Blur::blurWindowBackground();

        if (Settings::get<bool>("menu/box_shadow/enabled", true))
            dropShadow(Settings::get<float>("menu/box_shadow/size", 24.f), 255);

        const float globalScale = Settings::get<float>("menu/ui_scale", 1.f);
        ImGui::SetWindowFontScale(std::clamp(textScale * globalScale, 0.55f, 2.0f));

#if defined(__ANDROID__)
        // A desktop panel is about 12.6 font heights wide (0.105 of 1920 px at a
        // 16 px font). Keep that proportion on Android, otherwise labels get cut
        // off whenever the text is large compared to the screen-relative width.
        const float minPanelWidth = ImGui::GetFontSize() * 13.f;
        if (ImGui::GetWindowWidth() < minPanelWidth)
            ImGui::SetWindowSize(ImVec2(minPanelWidth, ImGui::GetWindowHeight()));

        handleTouchScroll();
#endif

        windowFunction();
    }

    // Capture geometry before End(), because End() restores the parent window.
    const ImVec2 actualPos = ImGui::GetWindowPos();
    const ImVec2 actualSize = ImGui::GetWindowSize();
    const bool userMovedWindow =
        std::fabs(actualPos.x - renderPosition.x) > 0.5f ||
        std::fabs(actualPos.y - renderPosition.y) > 0.5f;

    // Always close a successfully begun ImGui window, even when collapsed.
    ImGui::End();
    if (std::isfinite(actualPos.x) && std::isfinite(actualPos.y) &&
        std::isfinite(actualSize.x) && std::isfinite(actualSize.y)) {
        // renderPosition is also used by the open/close animation. Do not copy
        // its temporary off-screen animation coordinates into the persistent
        // position, otherwise every close changes the saved window position.
        // Only a real user drag may change the persistent position.
        if (userMovedWindow) {
            position = actualPos;
            renderPosition = actualPos;
        }

        // Keep the actual size so the user's resize survives the next frame
        // and is included in GUI::save().
        size = actualSize;
#if defined(__ANDROID__)
        fitWindowToScreen(*this);
#endif
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

    if (
        has_collapse_button &&
        style.WindowMenuButtonPosition == ImGuiDir_Right
    ) {
        pad_r += button_sz;

        collapse_button_pos = ImVec2(
            title_bar_rect.Max.x - pad_r - style.FramePadding.x,
            title_bar_rect.Min.y
        );
    }

    if (
        has_collapse_button &&
        style.WindowMenuButtonPosition == ImGuiDir_Left
    ) {
        collapse_button_pos = ImVec2(
            title_bar_rect.Min.x + pad_l - style.FramePadding.x,
            title_bar_rect.Min.y
        );

        pad_l += button_sz;
    }

    if (has_collapse_button) {
        if (
            ImGui::CollapseButton(
                window->GetID("#COLLAPSE"),
                collapse_button_pos
            )
        ) {
            window->WantCollapseToggle = true;
        }
    }

    if (has_close_button) {
        if (
            ImGui::CloseButton(
                window->GetID("#CLOSE"),
                close_button_pos
            )
        ) {
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

    if (
        style.WindowTitleAlign.x > 0.0f &&
        style.WindowTitleAlign.x < 1.0f
    ) {
        float centerness = ImSaturate(
            1.0f -
            ImFabs(style.WindowTitleAlign.x - 0.5f) * 2.0f
        );

        float pad_extend = ImMin(
            ImMax(pad_l, pad_r),
            title_bar_rect.GetWidth() -
                pad_l -
                pad_r -
                text_size.x
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

    ImVec2 min =
        ImGui::GetCurrentWindow()->TitleBarRect().Min;

    ImVec2 max =
        ImGui::GetCurrentWindow()->TitleBarRect().Max;

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
