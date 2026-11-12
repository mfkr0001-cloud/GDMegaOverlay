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
