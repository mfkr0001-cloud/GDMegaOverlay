# Android64 release checklist

Release candidate: `v2.11.1-android.1`
Target: Geometry Dash `2.2081`, Android `arm64-v8a`, Geode `5.11.2` or newer.

## Before creating a tag

1. Push the project contents to the root of your own GitHub repository (keep this port in its own branch; do not overwrite the upstream repository).
2. Open **Actions** and wait for **Build GDMegaOverlay Android64** to finish.
3. If the build fails, do not create a release tag. Download the failed job log and fix the first compiler error first.
4. If the build succeeds, download the `GDMegaOverlay-Android64` artifact and install it on an ARM64 device running Geometry Dash 2.2081 and Geode 5.11.2+.
5. Verify: menu opens from the floating Easy button, the button can be moved, the close button returns to the floating button, touch input is captured only while the menu is open, ImGui controls respond, two-finger scrolling works, and ordinary gameplay touch input works again after closing the menu.
6. Test the original tabs/features that are expected to be available on Android. Features documented as unavailable in `ANDROID_PORT_NOTES.md` must not be advertised as supported.

## Create the draft prerelease

After CI and device testing pass, create and push the tag matching `mod.json`:

```bash
git tag v2.11.1-android.1
git push origin v2.11.1-android.1
```

The tag workflow builds Android64 again, validates the `.geode` archive, and creates a **draft prerelease** with the package attached. Review the draft, release notes, and asset before publishing it.

## Do not publish if

- The Android64 workflow is red or was skipped.
- No `.geode` artifact was produced.
- The package has not been installed and tested on the target game/loader versions.
- The floating button or close button blocks ordinary gameplay touch input after the menu closes.
