# Styling, fonts, and DPI (v1.92.x docking)

> **Load this file when:** customizing the look (theme/colors), loading TTF fonts, merging icon fonts, or fixing fuzzy/blurry text or wrong-size UI on a hi-DPI / per-monitor-DPI / Retina display.
>
> **Tier guidance:** Tier 1 (lines 18-219 — common recipes: style stack, themes, font loading, icon merge, DPI baseline, non-ASCII labels, pitfalls) covers ~80% of queries. Tier 2 (lines 220-354 — mechanism: v1.92 rework, full `ImGuiStyle` reference, the three DPI knobs explained) handles "why does this work this way" questions. Tier 3 (lines 355-end — legacy glyph ranges, multi-viewport DPI) is rare. Default load: Tier 1 only (`Read offset=1 limit=219`), or jump straight to a single section via the Quick navigation below.

<!-- QUICK_NAV_BEGIN -->
> **Quick navigation** (jump to a section instead of loading the whole file - `Read offset=N limit=M`):
>
> - L  39-72   1. The style stack
> - L  73-95   2. Theme setup
> - L  96-132  3. Loading custom fonts
> - L 133-173  4. Merging an icon font
> - L 174-201  5. DPI baseline (recommended setup)
> - L 202-224  6. Non-ASCII characters in widget labels
> - L 225-240  7. Pitfalls quick reference
> - L 241-261  8. The v1.92 font-system rework
> - L 262-288  9. `ImGuiStyle` field reference
> - L 289-310  10. The live theme editor: `ShowStyleEditor()`
> - L 311-328  11. Font atlas
> - L 329-347  12. `ImFontConfig` fields that matter
> - L 348-375  13. The three DPI knobs explained
> - L 376-396  14. Glyph ranges (legacy)
> - L 397-410  15. Multi-viewport DPI
> - L 411-415  See also
<!-- QUICK_NAV_END -->





ImGui's style is a stack of named values pushed and popped per frame; fonts live in a single atlas the renderer uploads as a texture; DPI is handled via three orthogonal knobs that are easy to confuse. **v1.92 reworked the font system** — fonts are now dynamically sized, glyph ranges are auto-loaded on demand, and the atlas is rebuilt by the backend rather than the application. Older guidance from the web is often stale: if a tutorial passes glyph ranges to `AddFontFromFileTTF()` or treats `PushFont(font)` as a one-arg call, it predates v1.92.

This reference targets `IMGUI_VERSION 1.92.7` (`imgui.h:32`).

---

# Tier 1 — Common recipes

## 1. The style stack

The whole point of the style stack is *temporary, scoped overrides*. Setting `ImGui::GetStyle().FrameRounding = 5.0f` directly *would* work — but only persistently, and ImGui's comment at `imgui.h:407` is explicit: **"Always use PushStyleColor(), PushStyleVar() to modify style mid-frame!"** Mid-frame mutation of the live struct is undefined; the push/pop API is what ImGui validates.

Every push has a matching pop. Five paired stacks matter (`imgui.h:538-554`):

| Push                        | Pop                | Pushes onto              |
| --------------------------- | ------------------ | ------------------------ |
| `PushStyleColor(idx, col)`  | `PopStyleColor(n)` | Color stack              |
| `PushStyleVar(idx, val)`    | `PopStyleVar(n)`   | Style-var stack          |
| `PushStyleVarX/Y(idx, val)` | `PopStyleVar(n)`   | Style-var stack (1 axis) |
| `PushItemFlag(flag, on)`    | `PopItemFlag()`    | Item-flag stack          |
| `PushItemWidth(w)`          | `PopItemWidth()`   | Item-width stack         |
| `PushTextWrapPos(x)`        | `PopTextWrapPos()` | Text-wrap stack          |

The variants `PushStyleVarX` / `PushStyleVarY` (v1.92, `imgui.h:543-544`) modify just one component of an `ImVec2` style var, which avoids the boilerplate of reading the current value, mutating one axis, and pushing the whole vec.

**Mismatched stacks assert** at end-of-frame. ImGui keeps a per-stack count and validates on `EndFrame()`. The asserts are loud but the failure can be dozens of lines from the actual missing pop, especially across early returns.

**Use `ImScoped::*` for every paired call.** The bundled `imscoped.hpp` wraps these in RAII:

```cpp
{
    ImScoped::StyleColor text_col{ImGuiCol_Text, IM_COL32(255, 200, 0, 255)};
    ImScoped::StyleVar   round  {ImGuiStyleVar_FrameRounding, 5.0f};
    ImScoped::ItemWidth  w      {200.0f};
    if (ImGui::Button("OK")) { /* ... */ }
}   // ~ItemWidth -> PopItemWidth(); ~StyleVar -> PopStyleVar(1); ~StyleColor -> PopStyleColor(1)
```

Why bother: the destructor runs on early return, exception, and every other exit path. Manual `Pop*` calls drop the moment your function gains its second `if (!thing) return;`.

---

## 2. Theme setup

Three themes ship in the box (`imgui.h:426-428`, defined at `imgui_draw.cpp:187`, `:256`, `:326`):

```cpp
ImGui::StyleColorsDark();    // recommended, default (applied by ImGuiStyle::ImGuiStyle())
ImGui::StyleColorsLight();   // best with borders + a thicker font
ImGui::StyleColorsClassic(); // the original blue-ish look
```

Call once at startup before submitting any frame. The `ImGuiStyle` constructor already calls `StyleColorsDark()` (`imgui.cpp:1571`), so dark is the no-op default. To start from a theme and override individual colors:

```cpp
ImGui::StyleColorsLight();
ImGuiStyle& style = ImGui::GetStyle();
style.Colors[ImGuiCol_Button]        = ImVec4(0.9f, 0.5f, 0.1f, 1.0f);
style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 0.6f, 0.2f, 1.0f);
```

This mutates the persistent style struct, which is fine *outside* a frame. Inside a frame use `PushStyleColor`. To tune more colors live in-app, see `ShowStyleEditor()` in Tier 2.

---

## 3. Loading custom fonts

```cpp
ImGuiIO& io = ImGui::GetIO();

// Size auto-derived from style.FontSizeBase. Returns nullptr on failure.
ImFont* roboto = io.Fonts->AddFontFromFileTTF("assets/Roboto-Medium.ttf");
if (!roboto) {
    std::println(stderr, "Failed to load Roboto-Medium.ttf");
    // Atlas falls back to the default font; do not abort.
}

// Or pin a specific size:
ImFont* heading = io.Fonts->AddFontFromFileTTF("assets/Roboto-Bold.ttf", 22.0f);
```

Use the returned `ImFont*` mid-frame:

```cpp
{
    ImScoped::Font f{heading};                 // keep current size
    ImGui::Text("Section header");
}
{
    ImScoped::Font f{nullptr, 32.0f};          // keep current font, resize to 32px
    ImGui::Text("Hero text");
}
```

`ImScoped::Font` (assets/imscoped.hpp:304) calls `PushFont(font, size_unscaled)` on construction and `PopFont()` on destruction. Pass `nullptr` for font to keep the current font; pass `0.0f` for size to keep the current size. Both fields are now legitimate inputs in v1.92.

**Common error:** missing the failure path. `AddFontFromFileTTF` returns `nullptr` if the file can't be opened (commonly: wrong working directory, see `FONTS.md:48`). If you don't check, the atlas falls back to the default and your "custom font" silently doesn't load.

For the version-keyed default-font story (`AddFontDefault` vs `AddFontDefaultVector` vs `AddFontDefaultBitmap`), see Tier 2 §8.

---

## 4. Merging an icon font

The pattern: load the base font first, then load each additional font with `MergeMode = true`. Glyphs from the secondary fonts get folded into the first font's glyph table. `FONTS.md:291-302`:

```cpp
io.Fonts->AddFontDefaultVector();              // base font (or your own)

ImFontConfig cfg;
cfg.MergeMode = true;
cfg.GlyphMinAdvanceX = 13.0f;                  // monospace icon advance
io.Fonts->AddFontFromFileTTF("assets/fontawesome-webfont.ttf", 13.0f, &cfg);
```

After merging, FontAwesome icons render inline with normal text:

```cpp
ImGui::Text("%s among %d items", ICON_FA_SEARCH, count);
ImGui::Button(ICON_FA_SEARCH " Search");
```

The `ICON_FA_*` macros come from [IconFontCppHeaders](https://github.com/juliettef/IconFontCppHeaders) — they expand to UTF-8 string literals for the icon codepoints.

**`GlyphMinAdvanceX` requires a non-zero size.** From `FONTS.md:329`: "If you `GlyphMinAdvanceX` you need to pass a `font_size` to `AddFontXXX()` calls, as the MinAdvanceX value will be specified for the given size and scaled otherwise." So when merging icons monospace-style, pass an explicit size (here `13.0f`) rather than `0.0f`.

**Overlapping ranges.** When the base font also covers the icon codepoints (e.g. a font with rich Unicode coverage), the icon font is shadowed because the merge order is "first font that has the glyph wins" (`CHANGELOG.txt:1166-1171`). Use `cfg1.GlyphExcludeRanges` on the base font to skip the icon range:

```cpp
static ImWchar exclude_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
ImFontConfig cfg1;
cfg1.GlyphExcludeRanges = exclude_ranges;
io.Fonts->AddFontFromFileTTF("segoeui.ttf", 0.0f, &cfg1);

ImFontConfig cfg2;
cfg2.MergeMode = true;
io.Fonts->AddFontFromFileTTF("FontAwesome4.ttf", 0.0f, &cfg2);
```

`Metrics/Debugger > Fonts > Font > Input Glyphs Overlap Detection Tool` shows which font is providing each glyph.

---

## 5. DPI baseline (recommended setup)

ImGui has three independent DPI multipliers; for the common case, the recipe below is enough. The full mechanism is in Tier 2 §11.

```cpp
const float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(
    glfwGetPrimaryMonitor());   // or your platform's equivalent

ImGuiIO& io = ImGui::GetIO();
io.ConfigDpiScaleFonts     = true;      // auto-rescale fonts on monitor DPI change
io.ConfigDpiScaleViewports = true;      // auto-rescale viewport sizes (multi-viewport)

ImGuiStyle& style = ImGui::GetStyle();
style.ScaleAllSizes(main_scale);        // bake spacing/padding/borders once at startup
style.FontScaleDpi = main_scale;        // initial; ConfigDpiScaleFonts overrides on change
```

This matches the upstream guidance (`CHANGELOG.txt:1554-1557`):

> Call `style.ScaleAllSizes()` and set `style.FontScaleDpi` with this factor.
> Multi-viewport applications may set both of those flags:
> - `io.ConfigDpiScaleFonts = true;`
> - `io.ConfigDpiScaleViewports = true;`

**Apply this before the first `NewFrame()`.** A known caveat from `imgui.h:2532`: `ConfigDpiScaleFonts` rescales fonts on monitor DPI change but does **not** auto-rescale spacing/padding. If your app supports live monitor switching across DPIs, re-bake spacing yourself (cache an "unscaled" copy of `ImGuiStyle` and re-`ScaleAllSizes` from it).

---

## 6. Non-ASCII characters in widget labels

When a user submits a label containing characters outside basic ASCII — for example `Button("X")` (multiplication sign U+00D7), `Button("->")` (right arrow U+2192), or any glyph in the emoji blocks — the codepoint may or may not actually render depending on **two** things, and the skill should bring up both:

1. **Is the codepoint in the loaded glyph range?** `GetGlyphRangesDefault()` returns just `0x0020-0x00FF` (Basic Latin + Latin-1 Supplement, see `imgui_draw.cpp:4838-4847`). A codepoint outside that range will not render unless you load a font that includes it. With `RendererHasTextures` (v1.92+), the atlas auto-bakes any requested codepoint, so this layer is largely a non-issue on modern backends. On legacy backends, you must add a wider range via `glyph_ranges` (see Tier 3 §13).
2. **Does the actual font face contain a glyph for that codepoint?** Even if the range allows it, the font file (or the embedded `ProggyClean` / `ProggyForever` data) may not have a glyph drawn for that specific character. `ProggyClean` is a tight bitmap font — many Latin-1 Supplement codepoints (multiplication sign, division sign, accented letters, fraction one-half, degree, etc.) are missing or render as a fallback box. `ProggyForever` (vector, v1.92+) has broader coverage but is still not a full Unicode font.

**When you see non-ASCII characters in a user's `Button(...)` / `Text(...)` / `MenuItem(...)` label**, surface this as a follow-up to the main answer. Don't conflate it with the user's primary bug, but note the risk: "this glyph will render correctly only if your loaded font has a glyph for it; default `ProggyClean` does not have a multiplication-sign glyph". Recommend either an ASCII fallback (`"x"` instead of `"X-glyph"`) or loading a font face with the needed glyphs (and a wider range on legacy backends).

Quick reference for codepoints frequently seen in the wild:

| Codepoint | Description                  | In default range? | In ProggyClean? | In ProggyForever? |
|-----------|------------------------------|-------------------|-----------------|-------------------|
| U+00D7    | multiplication sign / close  | yes (Latin-1 Supp.) | no - renders as tofu | partial |
| U+00B0    | degree sign                  | yes               | no              | partial           |
| U+2192    | right arrow                  | no - Arrows block | n/a             | n/a               |
| U+2713    | check mark                   | no - Dingbats     | n/a             | n/a               |
| U+1F4DD-U+1F6FF | most emoji             | no                | n/a             | n/a               |

For anything outside Basic Latin (0x20-0x7E), prefer ASCII alternatives unless you have a specific reason and a font that covers the glyph. Common ASCII swaps: multiplication sign -> `x` or `X`; right arrow -> `->`; left arrow -> `<-`; degree -> `deg`; check -> `[ok]`.

---

## 7. Pitfalls quick reference

| Symptom                                                 | Cause                                                                                   | Fix                                                                                       |
| ------------------------------------------------------- | --------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------- |
| Blurry text on Retina / hi-DPI                          | `style.FontScaleDpi` and `ScaleAllSizes` not set, or set after backend init             | Set both **before** `ImGui_Impl*_Init`, or before first `NewFrame()`                      |
| Fonts vanish after monitor switch                       | Atlas not rebuilt for new DPI                                                           | `io.ConfigDpiScaleFonts = true;` (requires v1.92 backend)                                 |
| Padding doubled / UI looks bloated after second resize  | `style.ScaleAllSizes` called twice                                                      | Call it exactly once. Cache an "unscaled style" and re-apply if you need to re-scale.     |
| Style stack assert at frame end                         | Unbalanced `Push*` / `Pop*`                                                             | Use `ImScoped::StyleColor / StyleVar / ItemFlag / ItemWidth`                              |
| Icon glyphs render as "?"                               | `MergeMode` not set, or base font shadows the icon range                                | Set `cfg.MergeMode = true`; use `GlyphExcludeRanges` to free the icon codepoints in base  |
| Custom font silently falls back to default              | `AddFontFromFileTTF` returned `nullptr` (wrong path / working dir, `FONTS.md:48-50`)    | Check the return value; verify working directory; use absolute paths in development       |
| Empty white rectangles for every glyph                  | Atlas texture failed to upload (size limit, missing texture id)                         | Reduce `OversampleH = 1`; set `ImFontAtlasFlags_NoPowerOfTwoHeight`; check backend logs   |

---

# Tier 2 — Mechanism and full reference

## 8. The v1.92 font-system rework

Read this if you're updating from a 1.91 or earlier project, or copying snippets from old tutorials.

From `CHANGELOG.txt:1097-1108` (v1.92.0, released 2025-06-25):

> **THIS VERSION CONTAINS THE LARGEST AMOUNT OF BREAKING CHANGES SINCE 2015!**

The relevant items, paraphrased from `CHANGELOG.txt:1144-1162` and `FONTS.md:84-93`:

1. **Dynamic glyph loading.** Glyphs are baked into the atlas the first time they're requested, not at `Build()` time. Specifying `glyph_ranges` is no longer needed. The legacy `GetGlyphRangesXXX()` helpers are marked obsolete (still callable for older backends).
2. **`AddFontFromFileTTF()` size is now optional.** `size_pixels` defaults to `0.0f`, which means "auto-derive from `style.FontSizeBase`" (`imgui.h:3812`).
3. **`PushFont` requires a size.** The signature changed: `PushFont(ImFont* font)` -> `PushFont(ImFont* font, float size)` (`CHANGELOG.txt:1144`). Pass `0.0f` for size to keep current; pass `nullptr` for font to keep current font and just resize. To match the *exact* pre-1.92 behavior of a fixed-size font, use `font->LegacySize`.
4. **Default-font entry points.** v1.92.0 added `AddFontDefaultVector()` (ProggyForever, scalable) and kept `AddFontDefaultBitmap()` (ProggyClean, 13px pixel-perfect). v1.92.6 made `AddFontDefault()` *auto-select* between them based on the expected size — this is a breaking change for code that relied on `AddFontDefault()` always returning ProggyClean (`imgui.h:3809-3811`, `FONTS.md:14-16`, `CHANGELOG.txt:229-235`). When advising on default-font behavior and the user's patch version is unclear, name both: pre-1.92.6 = ProggyClean; 1.92.6+ = auto-select.
5. **Backend-managed textures.** Renderer backend flag `ImGuiBackendFlags_RendererHasTextures` (`imgui.h:35`) opts in. Without it you fall back to the legacy build-and-upload-yourself flow.
6. **Font data lifetime extended.** Pre-1.92, font data only had to live until `Build()`. Since 1.92, font data must live until `RemoveFont()` or atlas shutdown (`FONTS.md:256`, fixed in 1.92.6 per `CHANGELOG.txt`).

**If your guidance came from a pre-v1.92 tutorial, expect API differences in roughly that order: glyph ranges, font sizing, `PushFont` arity, default-font selection, and texture management.**

---

## 9. `ImGuiStyle` field reference

From `imgui.h:2366-2462` and the constructor in `imgui.cpp`. The full struct has ~50 fields; you will only ever care about ~12.

| Field                       | Default     | Affects                                | Typical adjustment                            |
| --------------------------- | ----------- | -------------------------------------- | --------------------------------------------- |
| `Alpha`                     | 1.0f        | Global opacity                         | 0.85 for HUD-style overlays                   |
| `WindowPadding`             | (8, 8)      | Inside-edge padding of every window    | Tighter UIs: (4, 4); spacious UIs: (12, 12)   |
| `WindowRounding`            | 0.0f        | Outer corner radius                    | 4-8 for soft modern look                      |
| `FramePadding`              | (4, 3)      | Padding inside buttons / inputs        | Larger for touch targets: (8, 6)              |
| `FrameRounding`             | 0.0f        | Button / input corner radius           | 3-5 for modern look                           |
| `ItemSpacing`               | (8, 4)      | Vertical gap between rows of widgets   | (8, 6) for breathing room                     |
| `ItemInnerSpacing`          | (4, 4)      | Gap *within* a widget (slider + label) | Rarely tuned                                  |
| `IndentSpacing`             | 21.0f       | Tree / collapsing-header indent        | 12 for dense file trees                       |
| `ScrollbarSize`             | 14.0f       | Scrollbar thickness                    | 10 for compact panels                         |
| `ScrollbarRounding`         | 9.0f        | Scrollbar corner radius                | Match `WindowRounding`                        |
| `GrabMinSize`               | 12.0f       | Minimum slider/scrollbar grab size     | Increase to 16+ for touch                     |
| `GrabRounding`              | 0.0f        | Slider grab corner radius              | Match `FrameRounding`                         |
| `ButtonTextAlign`           | (0.5, 0.5)  | Text alignment inside buttons          | (0.0, 0.5) for left-aligned button menus      |
| `SelectableTextAlign`       | (0.0, 0.0)  | Text alignment of `Selectable`         | (0.0, 0.5) for vertical center                |
| `WindowMenuButtonPosition`  | `_Left`     | Where the collapse arrow sits          | `_Right` or `_None`                           |
| `ColorButtonPosition`       | `_Right`    | Where the swatch sits in `ColorEdit*`  | `_Left` to mirror most OSes                   |

Don't sweep all of these at once — change two or three, look at it, change two more. Small adjustments to `FramePadding` and `ItemSpacing` reshape the entire UI more than any color choice.

---

## 10. The live theme editor: `ShowStyleEditor()`

`ImGui::ShowStyleEditor()` (`imgui.h:419`, implemented at `imgui_demo.cpp:8482`) renders an interactive panel for *every* color and *every* numeric style field. It also includes `ShowStyleSelector("Colors##Selector")` (`imgui_demo.cpp:8443`) for one-click theme switching, and "Save Ref" / "Save to clipboard" so you can paste the result back into your own init code.

**Recommendation:** wire it into a debug menu. Designers iterate without rebuilding; you copy the resulting numbers into source when satisfied:

```cpp
static bool show_style_editor = false;
if (auto m = ImScoped::Menu("Debug")) {
    ImGui::MenuItem("Style editor", nullptr, &show_style_editor);
}
if (show_style_editor) {
    if (auto w = ImScoped::Window("Style editor", &show_style_editor)) {
        ImGui::ShowStyleEditor();
    }
}
```

The same panel appears under `Demo > Tools > Style Editor`, so if `ImGui::ShowDemoWindow()` is already in your debug menu you may not need a second entry.

---

## 11. Font atlas

A single `ImFontAtlas`, accessed as `io.Fonts`, owns every glyph the renderer can draw. Multiple TTF/OTF files merge into the same atlas. The atlas is uploaded to the GPU as one (or a few) textures by the renderer backend. The backend with `ImGuiBackendFlags_RendererHasTextures` (a v1.92 feature, `imgui.h:35`) handles creation, updates, and teardown automatically.

Your code's only job is:
1. Call `AddFontXXX()` during init (and optionally any time before `NewFrame()`).
2. Optionally call `PushFont(font, size)` mid-frame to switch faces or sizes.

You no longer need to:
- Pre-build the atlas (`io.Fonts->Build()`).
- Pre-specify glyph ranges.
- Manually call `GetTexDataAsRGBA32()` and upload.
- Re-upload after a font change.

…provided the renderer backend supports `ImGuiBackendFlags_RendererHasTextures`. All upstream backends as of v1.92 do (CHANGELOG entry "Backends: DX9/DX10/DX11/DX12, Vulkan, OpenGL2/3, Metal, SDLGPU3, SDLRenderer2/3, WebGPU, Allegro5: Added ImGuiBackendFlags_RendererHasTextures support for all backends", `CHANGELOG.txt:1419`). Custom backends need to opt in.

---

## 12. `ImFontConfig` fields that matter

`imgui.h:3686-3725`. Skim these once; for normal use you'll touch maybe three.

| Field                    | Default | What it does                                                                                  |
| ------------------------ | ------- | --------------------------------------------------------------------------------------------- |
| `MergeMode`              | false   | Merge into the previous font. Set true on every font *after* the base.                        |
| `GlyphMinAdvanceX`       | 0       | Force glyph advance to at least this many pixels — for monospace icon fonts.                  |
| `OversampleH`            | 0       | Auto: 1 or 2 depending on size. Was 3 pre-v1.92. Lower = less memory, slightly worse anti-aliasing. |
| `OversampleV`            | 0       | Auto: 1. Y-axis sub-pixel positioning isn't used; usually leave alone.                        |
| `PixelSnapH`             | false   | Pixel-align glyph X positions. Good for crisp ProggyClean-style fonts; bad for general TTF.   |
| `RasterizerDensity`      | 1.0     | **Legacy.** Pre-v1.92 DPI multiplier. With v1.92 backends, this is automatic.                 |
| `FontDataOwnedByAtlas`   | true    | If false, the atlas keeps a pointer to your buffer — you must keep it alive until shutdown.   |
| `GlyphExcludeRanges`     | nullptr | Exclude codepoint ranges from this font source (for resolving merge collisions, see §4).      |

Note on `OversampleH`: the v1.92 default of `0` (auto) typically resolves to 1 or 2. The `imgui.h:3697` comment says: *"the difference between 2 and 3 is minimal. You can reduce this to 1 for large glyphs save memory."* Pre-v1.92 the default was 3; old tutorials suggesting "set OversampleH = 1 to halve atlas size" reflect that older default.

---

## 13. The three DPI knobs explained

ImGui has *three* unrelated multipliers and they handle different problems. Get this layered model in your head before changing values:

### `io.DisplayFramebufferScale` — pixel/coord ratio (set by backend)

`imgui.h:2495`. Framebuffer pixels per logical pixel: `(2, 2)` on Retina, `(1, 1)` on most Windows/Linux setups. **Set by the platform backend** (GLFW, SDL3, Win32). You read it; you don't write it. Used by the renderer for clip-rect / scissor math; also baked into `ImDrawData::FramebufferScale` (`imgui.h:3586`). Does **not** scale fonts or layout — that's what the next two knobs are for.

### `style.FontScaleDpi` — font size DPI multiplier

`imgui.h:2372`. Multiplied into `GetFontSize()` per the formula at `imgui.h:2369`:

```
GetFontSize() == FontSizeBase * (FontScaleMain * FontScaleDpi * other_factors)
```

Set this to your monitor's content scale (1.0 at 96 DPI, 1.25 at 120 DPI, 2.0 on Retina). Affects fonts only. With `io.ConfigDpiScaleFonts = true` (`imgui.h:2532`), ImGui auto-overwrites `FontScaleDpi` whenever the active monitor's DPI changes — you don't need to listen for `WM_DPICHANGED` yourself.

### `style.ScaleAllSizes(factor)` — bake spacing/padding/borders

`imgui.h:2456`, implemented at `imgui.cpp:1577`. Multiplies every spacing, padding, rounding, and border field through `ImTrunc` (truncates to int). **Lossy and persistent**: each call mutates the live `ImGuiStyle`. Calling it twice with `1.5f` is *not* the same as calling once with `2.25f` — the second call rounds again. **Call it once** at startup, or replace the whole style each frame.

This does **not** scale fonts. Pair it with `FontScaleDpi`. The recommended baseline recipe is in Tier 1 §5.

---

# Tier 3 — Appendix

## 14. Glyph ranges (legacy)

`io.Fonts->GetGlyphRanges*()` returns a static array of codepoint ranges:

```cpp
const ImWchar* GetGlyphRangesDefault();                 // Latin + Extended Latin
const ImWchar* GetGlyphRangesGreek();                   // + Greek
const ImWchar* GetGlyphRangesJapanese();                // + Hiragana, Katakana, ~3000 Kanji
const ImWchar* GetGlyphRangesChineseSimplifiedCommon(); // + ~2500 common CJK
const ImWchar* GetGlyphRangesChineseFull();             // + ~21000 CJK
// also: Cyrillic, Thai, Vietnamese
```

(`imgui.h:3849-3859`, `FONTS.md:197`.)

**With a v1.92-compliant backend, you don't need any of these.** They're marked obsolete. The atlas auto-bakes glyphs as text references them.

**Without `RendererHasTextures` (legacy or custom backends),** you still need to pass `glyph_ranges` to `AddFontFromFileTTF()` for any non-Latin glyphs. The array must persist until atlas build (`FONTS.md:62-63`), so pass the static return of `GetGlyphRanges*()` directly — never a stack-allocated array that escapes its scope.

---

## 15. Multi-viewport DPI

With viewports enabled, each `ImGuiViewport` exposes its own `DpiScale` field. Helper accessor:

```cpp
const float dpi = ImGui::GetWindowDpiScale();   // current window's viewport DpiScale
```

Backends with both `BackendFlags_PlatformHasViewports` and `BackendFlags_RendererHasViewports` get per-monitor DPI handling for free, including window movement between monitors of different DPI. The platform backend implements `Platform_GetWindowDpiScale(viewport)` and ImGui calls it when a window changes monitor.

When `io.ConfigDpiScaleViewports = true`, ImGui rescales the *viewport size itself* on DPI change. Combined with `ConfigDpiScaleFonts = true`, font and viewport stay in sync; only spacing/padding still requires manual handling.

---

## See also

- [`frame-loop.md`](frame-loop.md) — where in the frame these calls actually run; backend/platform NewFrame sequencing.
- [`changelog-1.92.x.md`](changelog-1.92.x.md) — full upgrade notes for the v1.92 font/texture rework.
- [`pitfalls-catalog.md`](pitfalls-catalog.md) — cross-cutting bugs and assert messages mapped to the deep-dive that fixes them.
