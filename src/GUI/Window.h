
#pragma once

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <imgui.h>

#include <functional>
#include <iostream>
#include <string>

namespace GUI
{
    class Window
    {
    private:
        ImGuiWindowFlags flags = 0;

        std::function<void()> windowFunction;

    public:
        bool enabled = true;

        std::string name = "";

        ImVec2 position = ImVec2(0, 0);
        ImVec2 renderPosition = ImVec2(0, 0);
        ImVec2 size = ImVec2(220, 120);
        ImVec2 minSize = ImVec2(200, 120);
        ImVec2 maxSize = ImVec2(200, 2000);

    public:
        Window(
            const std::string& name,
            const std::function<void()>& windowFunction
        );

        void addFlag(ImGuiWindowFlags flag);

        void draw();

        bool visibleInScreen();

        void customTitlebar();

        void dropShadow(float size, ImU8 opacity);
    };
}
