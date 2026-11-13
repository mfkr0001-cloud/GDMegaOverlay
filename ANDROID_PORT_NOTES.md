# Android ARM64 port notes

Target: Geometry Dash 2.2081, Geode Android64, `arm64-v8a`.

## Implemented in this development branch

- Android-only touch overlay layer attached to each active Cocos scene.
- Floating Easy-style button rendered through the existing ImGui context.
- First-run default menu window positions and sizes scale from the original 1920px desktop layout to the current viewport; saved window geometry remains viewport-relative.
- Button drag with normalized position persistence.
- Menu open/close through the floating button and an ImGui close button.
- Software cursor while the menu is open.
- Primary touch mapped to ImGui left mouse input.
- Second touch used for right-click candidate / two-finger vertical scrolling.
- Touch state reset when a scene exits to avoid stuck mouse buttons or stale touch IDs.
- Android-only build workflow for Geode's `Android64` target.

## Deliberate Android limitations

The original project contains desktop executable offsets and Windows-only integrations. Those must not be reused on Android ARM64 without independently verified 2.2081 ARM64 symbols/addresses and ABI checks. The current port disables or stubs several desktop-only capabilities, including binary JSON patches, Windows process priority, Discord RPC, desktop file dialogs/FFmpeg rendering, and offset-based macro playback/recording features. This keeps the Android menu and touch bridge separate from unsafe desktop patch addresses; it does **not** mean every original feature is available on Android.

## Verification status

- Source/archive inspection: completed.
- Static source review: updated for Android window visibility and basic Macro Bot support.
- CMake configuration/build: not verified in this environment because a matching Geode SDK + Android NDK toolchain is not installed here.
- Runtime touch behavior on a physical Android device: not verified.
- `.geode` package: not yet produced or verified.

Do not treat this development branch as a tested release until the GitHub Actions Android64 job succeeds and the resulting package is tested in Geometry Dash 2.2081 on an ARM64 device.

## Release preparation pass

- GitHub Actions now runs on every branch and on `v*` tags (not only the upstream `geode` branch).
- GitHub Actions builds the Android64 package against the Geode SDK version pinned in `mod.json` (5.11.2).
- Pushing a `v*` tag creates a **draft prerelease**, not a public release. Review it before publishing.
- The mod metadata version is `v2.11.1-android.1`; use the same tag for the first candidate. The minimum Geode version is 5.11.2, and the package metadata declares Android 2.2081 only.
- The release job checks that at least one `.geode` file exists before creating the draft.

## Release blockers

This source snapshot has not been compiled against the Geode SDK in this environment because `GEODE_SDK` and Android NDK toolchains are not installed here. The Android touch bridge is a source-level implementation and has not been verified on a device. Therefore this archive is a release-candidate preparation, **not a verified release build**. Do not publish the draft until CI passes and the package is installed and tested on Geometry Dash 2.2081 + Geode Android ARM64.


## Android global overlay and patch list correction (revision 4)

- The floating overlay toggle is now attached to each running `CCScene`, rather than only `MenuLayer` and `PauseLayer`. This covers scene transitions into level browsing/search and gameplay.
- Bypass and Creator definitions are loaded from `res/hacks/*.json` on Android so their real entries remain visible. Their desktop opcode toggles are shown disabled with an explanation because those memory addresses are not valid on Android.
- The default Android layout now arranges all primary windows in a six-column, two-row grid. The layout revision is bumped so an old saved layout is refreshed once; the user's settings and patch preferences are not erased.
- Android UI scale remains `0.42f`.


## Practice-mode macros and Android video renderer (revision 5)

- Macro Bot: practice-mode checkpoints are supported. Each checkpoint remembers the macro frame it was created at; respawning at a checkpoint rewinds the frame counter and drops inputs recorded after it instead of wiping the macro. Known limitation: the Android frame counter still counts `GJBaseGameLayer::update` calls, not physics steps, so macros can drift if the frame rate differs between recording and playback.
- Recorder: an Android renderer now exists (`src/Macrobot/AndroidEncoder.*`, `Record.cpp`). It replays the loaded macro, draws `PlayLayer` into an OpenGL ES framebuffer, reads the pixels back and encodes H.264 into an `.mp4` with the NDK `AMediaCodec`/`AMediaMuxer` (no Java, no MediaProjection, no FFmpeg). Output goes to `renders/` in the mod's save folder.
- Recorder limitations: video only (no audio track yet); frame size is rounded to a multiple of 16; shader effects may render at the wrong resolution because the desktop `glViewport` hook is not used on Android; not yet verified on a physical device.
