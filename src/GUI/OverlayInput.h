#pragma once

namespace OverlayInput {
    // Install the Android touch bridge into the current scene. The scene hook
    // keeps it available after Geometry Dash changes scenes.
    void install();

    // Draw the Android floating control, close button, and software cursor.
    void draw();
}
