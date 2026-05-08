> **Load this file when:** planning an upgrade from v1.91.x or an earlier v1.92.x point release, or wondering whether a tutorial / Stack Overflow answer pre-dates a relevant API change. Distilled from upstream's CHANGELOG.txt, organized by user-impact.

# Dear ImGui v1.92.x changelog (distilled)

## The headline change: the font system rework (v1.92.0)

The defining change of the v1.92 series is that **fonts are no longer fixed-size**. Before v1.92, an `ImFont*` was one TTF rasterized at one size, baked into the atlas at build time. After v1.92, an `ImFont*` represents source data; sizes bake on demand into per-size `ImFontBaked` instances cached internally. This makes DPI-aware rendering, runtime size switching, and large-font rendering work without rebuilding the atlas.

Quoting upstream verbatim (CHANGELOG.txt:1142-1151):

> Fonts: **IMPORTANT** on Font Sizing:
>    - Before 1.92, fonts were of a single size. They can now be dynamically sized.
>    - PushFont() API now has a REQUIRED size parameter.
>        void PushFont(ImFont* font) --> void PushFont(ImFont* font, float size);
>        - PushFont(font, 0.0f)             // Keep current size
>        - PushFont(font, 20.0f)            // Set size to 20.0f
>        - PushFont(font, font->LegacySize) // Same as pre-1.92 behavior

Plain-English impact: existing `AddFontFromFileTTF("fontname.ttf", 16.0f)` calls keep working — but `PushFont(font)` is gone; pick a size or pass `font->LegacySize` to mimic the old behavior. Glyph ranges become optional (the engine loads glyphs on demand), and `style.FontSizeBase` / `style.FontScaleDpi` provide a clean DPI-scaling pipeline.

## v1.92.0 (Released 2025-06-25)

Largest set of breaking changes since 2015 (CHANGELOG.txt:1107).

### Breaking changes (selected)

> Fonts: **IMPORTANT**: if your app was solving the OSX/iOS Retina screen specific logical vs display scale problem by setting io.DisplayFramebufferScale [...] + setting io.FontGlobalScale + loading fonts at scaled sizes: This WILL NOT map correctly to the new system! (CHANGELOG.txt:1130-1141)

— *What to do:* drop `io.FontGlobalScale` (now `style.FontScaleMain`); let the v1.92+ backend set per-viewport `FramebufferScale`; load fonts at logical sizes.

> Textures: All API functions taking a 'ImTextureID' parameter are now taking a 'ImTextureRef' [...] Affected: `Image()`, `ImageWithBg()`, `ImageButton()`, `AddImage()`/`AddImageQuad()`/`AddImageRounded()`. (CHANGELOG.txt:1186-1194)

— *What to do:* nothing for plain `ImTextureID` callers — `ImTextureRef` constructs implicitly. Custom backends and bindings need explicit conversion.

> Fonts: obsoleted ImFontAtlas::GetTexDataAsRGBA32(), GetTexDataAsAlpha8(), Build(), SetTexID() and IsBuilt() functions. The new protocol for backends to handle textures doesn't need them. (CHANGELOG.txt:1196-1198)

— *What to do:* custom renderer backends switch to the texture-update protocol (`ImDrawData::Textures[]` with `ImTextureStatus_*` lifecycle states). Stock backends already handle this.

> Fonts: specifying glyph ranges is now unnecessary. The value of ImFontConfig::GlyphRanges[] is only useful for legacy backends. All GetGlyphRangesXXXX() functions are now marked obsolete. (CHANGELOG.txt:1209-1215)

— *What to do:* delete your `GetGlyphRangesJapanese`/`Cyrillic`/etc. setup. Glyphs load on demand.

> Layout: commented out legacy ErrorCheckUsingSetCursorPosToExtendParentBoundaries() fallback [...] which allowed a SetCursorPos()/SetCursorScreenPos() call WITHOUT AN ITEM to extend parent window/cell boundaries. (CHANGELOG.txt:1304-1313)

— *What to do:* after `SetCursorPos()`, follow with `Dummy(ImVec2(0,0))` to commit layout extent.

> [Docking] renamed/moved ImGuiConfigFlags_DpiEnableScaleFonts -> bool io.ConfigDpiScaleFonts. [Same for DpiEnableScaleViewports.] (CHANGELOG.txt:1327-1328)

— *What to do:* find/replace the old `ConfigFlags_*` names.

### Notable features

- Dynamic font sizing: `PushFont(font, desired_size)` rasterizes on demand.
- Texture-update protocol: backends can stream font/atlas updates without re-uploading the whole texture.
- `ImFontConfig::GlyphExcludeRanges[]`: invert of `GlyphRanges`, useful for merged icon fonts.
- TreeNode line-drawing flags `ImGuiTreeNodeFlags_DrawLinesFull` / `DrawLinesToNodes`.
- Per-viewport `FramebufferScale` (docking branch) finally fixes Retina multi-monitor clipping.

### Notable bug fixes

- `InputText`: buffer overrun with dynamic resize callbacks (#8689); cursor positioning near end of lines with non-ASCII text (regression from 1.91.2; #8635).
- Tables: assert combining frozen rows + clipper + multi-select (#8595).
- Tooltips: max size capped to host monitor — fixes abnormally large framebuffer requests in multi-viewport setups.

## v1.92.1 (Released 2025-07-09)

No breaking changes. Notable: `ImFontAtlas::SetFontLoader()` for runtime loader switching; large-font path now loads only metrics up front (#8758); 512px font-size cap to reduce OOM. Fixes: `FontNo` restored to 32 bits (#8775); `PushFont()` no longer no-ops after a hidden table column (#8865).

## v1.92.2 (Released 2025-08-11)

### Breaking changes

> Tabs: Renamed ImGuiTabBarFlags_FittingPolicyResizeDown to ImGuiTabBarFlags_FittingPolicyShrink. Kept inline redirection enum (will obsolete). (#261, #351) (CHANGELOG.txt:956-957)

— *What to do:* rename at convenience; the alias buys time but is marked for removal.

> Backends: SDL_GPU3: changed ImTextureID type from SDL_GPUTextureSamplerBinding* to SDL_GPUTexture* [...] (CHANGELOG.txt:958-960)

— *What to do:* SDL_GPU3 users update their `ImTextureID` casts. Sampler is now retrieved via `ImGui_ImplSDLGPU3_RenderState`.

### Notable features

- New `ImGuiTabBarFlags_FittingPolicyMixed` (now default): shrink first, then enable scroll buttons.
- `style.TabMinWidthBase` / `TabMinWidthShrink` to configure tab fitting.
- `IsItemHovered()` consistency fix: no longer returns true after first-clicking the background of a non-moveable window then mousing over the item.

### Notable bug fixes

- Vulkan: fixed texture update corruption introduced in 1.92.0 affecting some drivers (#8801).
- SDL_GPU3: expose current sampler in render state struct (#8866).

### v1.92.2b (Released 2025-08-13)

Hotfix for `IsItemHovered()` regression on disabled / no-ID items (#8877).

## v1.92.3 (Released 2025-09-17)

No breaking changes. Notable: `ImGuiInputTextFlags_WordWrap` (beta) for multi-line word-wrapping — ~10x slower than non-wrapped, track #3237 for refinements; `style.ScrollbarPadding` replaces a hardcoded value (#8895); `ImGuiSelectableFlags_SelectOnNav` for auto-selection during keyboard nav. Fixes: Escape-revert in `InputText` not writing back during `IsItemDeactivatedAfterEdit()` frame (#8915); font merging when target font isn't last added (#8912); docking-branch `io.ConfigDpiScaleFonts` finally functional (#8832).

## v1.92.4 (Released 2025-10-14)

### Breaking changes

> Viewports: for consistency with other config flags, renamed io.ConfigViewportPlatformFocusSetsImGuiFocus to io.ConfigViewportsPlatformFocusSetsImGuiFocus. (#6299, #6462) It was really a typo in the first place, and introduced in 1.92.2. (CHANGELOG.txt:661-664)

— *What to do:* fix the typo at the call site (note the added `s` after `Viewports`).

> TreeNode, Selectable, Clipper: commented out legacy names obsoleted in 1.89.7 / 1.89.9: `*Flags_AllowItemOverlap` --> `*Flags_AllowOverlap`; `IncludeRangeByIndices()` --> `IncludeItemsByIndex()`. (CHANGELOG.txt:666-670)

— *What to do:* mechanical rename; redirects had been live for two years.

> Vulkan: moved some fields in ImGui_ImplVulkan_InitInfo: RenderPass / Subpass / MSAASamples / PipelineRenderingCreateInfo --> init_info.PipelineInfoMain.* (CHANGELOG.txt:671-675)

— *What to do:* Vulkan backend users move those fields under `PipelineInfoMain`. New `PipelineInfoForViewports` configures secondary viewports separately.

### Notable

Features: child-window resize grip with both `Resize{X,Y}` flags (#8501); `ImGuiCol_UnsavedMarker`; `ImGuiInputFlags_RouteFocused | RouteOverActive` for active-item shortcut stealing (#9004). Fixes: DirectX12 sync rework likely closes hard-to-repro crashes (#3463); OpenGL3 loader works on Haiku (#8952); docking-branch `NoBringToFrontOnFocus` honored with viewports (#7008).

## v1.92.5 (Released 2025-11-20)

### Breaking changes

> Keys: commented out legacy names obsoleted in 1.89.0: ImGuiKey_Mod{Ctrl,Shift,Alt,Super} --> ImGuiMod_*. (CHANGELOG.txt:500-504)

— *What to do:* find/replace `ImGuiKey_Mod*` with `ImGuiMod_*`.

> IO: commented out legacy io.ClearInputCharacters() obsoleted in 1.89.8. Using io.ClearInputKeys() is enough. (CHANGELOG.txt:505-506)

— *What to do:* delete calls.

> BeginChild: commented out legacy names obsoleted in 1.90.0 [...]: ImGuiChildFlags_Border --> ImGuiChildFlags_Borders; ImGuiWindowFlags_NavFlattened / AlwaysUseWindowPadding --> ImGuiChildFlags_*. (CHANGELOG.txt:507-514)

— *What to do:* `BeginChild` flags moved from window-flags arg to child-flags arg.

### Notable

Features: Escape cancels active drag-and-drop (#9071); `AcceptDrawAsHovered` lets a `Button()` be a drop target (#8632); new `imgui_impl_null` for headless contexts. Fixes: nested `BeginTable() -> Begin() -> BeginTable()` no longer leaves wrong current-table (#9005); disabled items with nav focus no longer keyboard-activatable (#9036); multi-context font sharing across `Render()` reordering (#9039).

## v1.92.6 (Released 2026-02-17)

Repaid a decade of font-system technical debt — and broke `.ini` files in the process.

### Breaking changes

> AddFontDefault() now automatically selects an embedded font between AddFontDefaultBitmap() (classic pixel-clean font [...]) and AddFontDefaultVector() (new scalable font [...]). [...] Prefer explicitly calling either of them based on your own logic! (CHANGELOG.txt:227-235)

— *What to do:* if your default font appearance changed unexpectedly, call `AddFontDefaultBitmap()` explicitly.

> Fixed handling of ImFontConfig::FontDataOwnedByAtlas = false which did erroneously make a copy of the font data [...] (undetected since July 2015 [...]). [...] Since 1.92, font data needs to available until atlas->RemoveFont(), or more typically until a shutdown of the owning context. (CHANGELOG.txt:236-246)

— *What to do:* if you set `FontDataOwnedByAtlas = false`, keep the source buffer alive for the lifetime of the ImGui context.

> Popups: changed compile-time ImGuiPopupFlags popup_flags = 1 default value to be = 0 for BeginPopupContextItem(), BeginPopupContextWindow(), BeginPopupContextVoid(), OpenPopupOnItemClick(). [...] Before: literal 0 means MouseButtonLeft. After: literal 0 means MouseButtonRight. (CHANGELOG.txt:249-275)

— *What to do:* `BeginPopupContextItem("foo", 0)` now silently flips to right-click. Replace with `ImGuiPopupFlags_MouseButtonLeft`. The bare default (no flag arg) is unchanged.

> Hashing: handling of "###" operator to reset to seed within a string identifier doesn't include the "###" characters in the output hash anymore:
>     Before: GetID("Hello###World") == GetID("###World") != GetID("World")
>     After:  GetID("Hello###World") == GetID("###World") == GetID("World")
> [...] This will invalidate hashes (stored in .ini data) for Tables and Windows that are using the "###" operators. (#713, #1698) (CHANGELOG.txt:282-289)

— *What to do:* delete your `imgui.ini` after upgrading, or accept that windows / tables using `###` overrides lose their saved layout. There is no migration shim.

> Commented out legacy names obsoleted in 1.90 (Sept 2023): BeginChildFrame() --> BeginChild() with ImGuiChildFlags_FrameStyle; EndChildFrame() --> EndChild(); ShowStackToolWindow() --> ShowIDStackToolWindow(). (CHANGELOG.txt:276-281)

— *What to do:* mechanical replacements; aliased for two-plus years.

> Renamed helper macro IM_ARRAYSIZE() -> IM_COUNTOF(). Kept redirection. (CHANGELOG.txt:290)

### Notable features

- `AddFontDefaultVector()` — MIT-licensed embedded scalable monospace font (ProggyForever, ~14 KB), first scalable default in ImGui's history.
- `ColorEdit` R/G/B/A markers per component (`NoColorMarkers` opts out; `style.ColorMarkerSize`).
- Nav: Shift+F10 / Menu key opens context menus from `BeginPopupContext*` / `OpenPopupOnItemClick` (#8803).
- Scrollbar hit-test extends past window edge for fullscreen / edge-docked windows (#9276).
- `style.ImageRounding` for `Image()` widgets.

### Notable bug fixes

- Fonts: crash with `AddFont()` `MergeMode==true` on already-rendered font (#9162); `PushFont()` from collapsed implicit Debug window (#9210).
- Tables: column display order lost on count reduction or .ini with missing/duplicate values (#9108).
- `InvisibleButton`: zero size fits available content instead of being invalid (#9166).
- Win32: `WM_IME_CHAR` / `WM_IME_COMPOSITION` for Unicode on MBCS (#9099).
- OpenGL3: embedded loader survives multiple init/shutdown cycles (#8792).
- SDL2/SDL3: X11 mouse-capture default starts on mouse-down except X11; new `SetMouseCaptureMode` for debugger use (#3650).

## v1.92.7 (Released 2026-04-02)

### Breaking changes

> Separator: fixed a legacy quirk where Separator() was submitting a zero-height item for layout purpose, even though it draws a 1-pixel separator. [...] In 1.92.7 the resulting window will have unexpected 1 pixel scrolling range. (CHANGELOG.txt:46-57)

— *What to do:* in hand-computed footer/status-bar heights that include a `Separator()`, add `style.SeparatorSize`.

> Multi-Select: renamed ImGuiMultiSelectFlags_SelectOnClick to ImGuiMultiSelectFlags_SelectOnAuto. Kept inline redirection enum. (CHANGELOG.txt:58-59)

— *What to do:* rename at convenience.

> Combo(), ListBox(): commented out legacy signatures obsoleted in 1.90 (Nov 2023): callback type was changed from `bool (*)(void*, int, const char**)` to `const char* (*)(void*, int)`. (CHANGELOG.txt:60-64)

— *What to do:* update old getter signatures to return `const char*` directly.

### Notable features

- `TreeNodeGetOpen()` promoted to public API for efficient tree clipping via `SetNextItemStorageID()` + `TreeNodeGetOpen()` (#3823).
- InputText multi-line: Shift+Enter always adds a newline regardless of `CtrlEnterForNewLine` (#9239).
- Tables: column reordering via context menu (#9312).
- Border sizes scaled and rounded by `style.ScaleAllSizes()`; `style.SeparatorSize` exposed.
- `ImGuiButtonFlags_AllowOverlap` promoted from `imgui_internal.h` to `imgui.h`.

### Notable bug fixes

- InputTextMultiline: edit buffer reapplied on `IsItemDeactivatedAfterEdit()` frame (#9308); `CallbackResize` crash from 1.92.6 (#9174).
- Scrollbar divide-by-zero on tiny ranges (#9089); `PathArcTo()` segment count for tiny arcs (#9331).
- Docking branch: viewport hover detection and X11 multi-viewport regressions (#9254, #9284).

## Upgrade checklist (v1.91.x → v1.92.7)

Action items in roughly the order they bite:

1. **PushFont calls** — add size parameter or pass `font->LegacySize`. (v1.92.0)
2. **`SetCursorPos` without item** — append `Dummy(ImVec2(0,0))`. (v1.92.0)
3. **`io.FontGlobalScale`** — replace with `style.FontScaleMain`. (v1.92.0)
4. **`ImGuiConfigFlags_DpiEnableScale*`** — replace with `io.ConfigDpiScaleFonts` / `io.ConfigDpiScaleViewports`. (v1.92.0)
5. **Custom renderer backends** — implement `ImTextureStatus_*` protocol; remove `CreateFontsTexture` calls. (v1.92.0)
6. **`ImGuiTabBarFlags_FittingPolicyResizeDown`** — rename to `_FittingPolicyShrink`. (v1.92.2)
7. **`io.ConfigViewportPlatformFocusSetsImGuiFocus`** — fix typo (`s` after `Viewports`). (v1.92.4)
8. **`*Flags_AllowItemOverlap` / `IncludeRangeByIndices`** — rename to non-`Item` versions. (v1.92.4)
9. **`ImGuiKey_ModCtrl` etc.** — replace with `ImGuiMod_*`. (v1.92.5)
10. **`io.ClearInputCharacters()`** — delete. (v1.92.5)
11. **`BeginChild` flag arg position** — `Border`, `NavFlattened`, `AlwaysUseWindowPadding` moved to child-flags. (v1.92.5)
12. **`AddFontDefault()` appearance** — call `AddFontDefaultBitmap()` explicitly for the classic look. (v1.92.6)
13. **`FontDataOwnedByAtlas = false`** — keep the source buffer alive for the context lifetime. (v1.92.6)
14. **`BeginPopupContextItem(..., 0)`** — replace `0` with `ImGuiPopupFlags_MouseButtonLeft` for left-click. (v1.92.6)
15. **Delete `imgui.ini`** — `###` hash space changed; layouts will not migrate. (v1.92.6)
16. **`BeginChildFrame` / `EndChildFrame`** — replace with `BeginChild(..., ImGuiChildFlags_FrameStyle)` / `EndChild`. (v1.92.6)
17. **Manual footer-height math containing `Separator()`** — add `style.SeparatorSize`. (v1.92.7)
18. **`ImGuiMultiSelectFlags_SelectOnClick`** — rename to `_SelectOnAuto`. (v1.92.7)
19. **Combo/ListBox legacy getter signature** — update to `const char* (*)(void*, int)`. (v1.92.7)

## What's still pending upstream

Selected user-affecting items from `vendor/imgui/docs/TODO.txt`:

- `SetNextWindowSize()` with ≤0 on a single axis — keep one axis as-is (TODO.txt:14, #690).
- InputText: `INSERT` to toggle overwrite — disabled because `stb_textedit` is unsatisfactory on multi-line (TODO.txt:71, #2863).
- InputText multi-line: better horizontal scrolling (TODO.txt:89, #383).
- Layout: horizontal flow wrapping when space runs out (TODO.txt:97, #404).
- Group: `IsItemHovered()` after `EndGroup()` covers whole group AABB rather than item intersection (TODO.txt:108).
- Docking: central node resizing incorrect (TODO.txt:132); tab bar make selected tab always show full title (TODO.txt:146, #261).
- Nav: `Home/End/PageUp/PageDown` should work without `NavEnableKeyboard` (TODO.txt:291); restore/find nearest `NavId` when current disappears (TODO.txt:296).

For the live list, read `TODO.txt` directly — it's a working scratchpad with stale and low-priority entries.

## See also

- [styling-fonts-dpi.md](styling-fonts-dpi.md) — deep-dive on the font rework, DPI scaling, and the new style fields.
- [pitfalls-catalog.md](pitfalls-catalog.md) — when an upgrade triggers a runtime symptom, the catalog maps it back to the change.
- [bootstrap.md](bootstrap.md) — minimum project setup; the bundled CMake template pins `v1.92.7-docking`.
