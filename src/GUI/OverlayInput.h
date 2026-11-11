#pragma once

namespace OverlayInput {
    // Floating control is attached directly to the main menu and pause layer on Android.
    void install();

    // Draws the ImGui close control while the overlay menu is open.
    void draw();

    // Kept for compatibility with existing pause-layer hooks.
    void setPaused(bool paused);
}
