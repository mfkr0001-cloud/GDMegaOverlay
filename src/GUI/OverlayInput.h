#pragma once

namespace OverlayInput {
    // Installs the Android floating button and its touch bridge into the active scene.
    void install();

    // Draws the ImGui close control while the overlay menu is open.
    void draw();

    // Kept for compatibility with existing pause-layer hooks.
    void setPaused(bool paused);
}
