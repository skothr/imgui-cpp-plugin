# Modals and popups

> **Load this file when:** authoring popups or modals — file/save dialogs, confirmation prompts, right-click context menus, dropdown helpers, or fixing "my popup never opens" / "my modal closes immediately".

This reference covers the popup lifecycle (the most-misunderstood part of the API), modal vs non-modal behavior, the context-menu helpers, ID scoping inside popups, and the small set of recurring pitfalls. The single rule worth memorizing first: **`OpenPopup` is a one-shot intent, not a per-frame command.** Almost every "my popup never opens" or "my popup closes immediately" report traces back to violating that.

## The popup lifecycle

Popups are driven by two distinct calls:

- **`OpenPopup(str_id)`** — queues the popup for opening. Sets internal state so the matching `BeginPopup`/`BeginPopupModal` will succeed on its next call. Per `imgui.h:867`:

  > `OpenPopup(...)` — call to mark popup as open (**don't call every frame!**).

- **`BeginPopup(str_id)` / `BeginPopupModal(name, p_open, flags)`** — query whether the popup is currently open and, if so, begin a window into which you submit its body. Returns `true` when the popup is open; you submit content and call `EndPopup` only when it returned true. Per `imgui.h:854-858`:

  > `BeginPopup()`: query popup state, if open start appending into the window. Call `EndPopup()` afterwards if returned true.
  > `BeginPopupModal()`: block every interaction behind the window, cannot be closed by user, add a dimming background, has a title bar.
  > `EndPopup()` — only call `EndPopup()` if `BeginPopupXXX()` returns true!

The asymmetry matters. Unlike `Begin`/`End` (where `End` always runs), `EndPopup` is conditional. The `ImScoped::Popup` and `ImScoped::PopupModal` guards already encode that — their destructors check the open flag before calling `EndPopup`.

The flow per frame, once a popup has been opened by an event:

```
Frame N:   user clicks Open button → OpenPopup("X") fires once
           BeginPopup("X") returns false (state set, but popup window not yet built)
Frame N+1: BeginPopup("X") returns true → submit body → EndPopup
Frame N+2: BeginPopup("X") returns true → submit body → EndPopup
...        until CloseCurrentPopup() or user clicks outside (non-modal) or hits Esc
```

## Why "OpenPopup every frame" is wrong

This is the most common popup bug, and it's a category error: treating `OpenPopup` as if it controlled visibility (like a `bool show_popup`) rather than as an event.

```cpp
// WRONG — pops up forever, won't dismiss.
ImGui::OpenPopup("confirm");
if (auto p = ImScoped::Popup("confirm")) {
    ImGui::Text("Are you sure?");
    if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
}
```

What happens: every frame, `OpenPopup` re-queues the popup. `CloseCurrentPopup` removes it from the stack at the *end* of this frame's body. Next frame, `OpenPopup` fires again at the top, re-adding it. The popup never appears to dismiss.

The fix is to gate `OpenPopup` on a triggering event:

```cpp
// CORRECT — OpenPopup fires on the click only.
if (ImGui::Button("Delete")) {
    ImGui::OpenPopup("confirm");
}
if (auto p = ImScoped::Popup("confirm")) {
    ImGui::Text("Are you sure?");
    if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
}
```

ImGui does silently de-dupe successive same-frame `OpenPopup` calls, so the bug is sometimes more subtle than "popup is stuck open" — it can manifest as "the popup re-anchors its position every frame" or "navigation focus resets." All have the same root cause and the same fix.

## Modal vs non-modal

`BeginPopup` is non-modal: a regular floating window the user can dismiss by clicking outside, pressing Escape, or your code calling `CloseCurrentPopup`. Background windows remain interactive.

`BeginPopupModal` blocks input outside the popup, dims the background, and adds a title bar. From `imgui.h:855` — "block every interaction behind the window, cannot be closed by user, add a dimming background, has a title bar." Internally it sets `ImGuiWindowFlags_Popup | ImGuiWindowFlags_Modal | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking`.

```cpp
// Non-modal — click outside dismisses.
if (auto p = ImScoped::Popup("tools")) {
    if (ImGui::Selectable("Cut"))   { /* ... */ }
    if (ImGui::Selectable("Copy"))  { /* ... */ }
    if (ImGui::Selectable("Paste")) { /* ... */ }
}

// Modal — click outside is blocked; only an explicit close ends it.
if (auto m = ImScoped::PopupModal("Settings", nullptr,
                                  ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Checkbox("Enable foo", &foo);
    if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
}
```

`BeginPopupModal` auto-centers on its first appearance — internally calling `SetNextWindowPos(viewport->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f))` if you didn't set a position yourself. You can override with `SetNextWindowPos` before `BeginPopupModal`.

## Closing

Three ways a popup closes:

1. **`CloseCurrentPopup()` from inside the popup body.** Use this for "OK"/"Cancel" buttons. It removes the popup from the stack; the popup stops rendering on the next frame. You still call `EndPopup` to balance `BeginPopup` for the current frame — the imscoped guard handles this.
2. **Click outside (non-modal only).** Free for `BeginPopup`. Blocked for `BeginPopupModal`.
3. **The OS-style close button.** Pass `bool* p_open` to `BeginPopupModal`; ImGui draws a close button in the title bar that toggles your bool. Combine with explicit close: when `*p_open` becomes false, the modal is dismissed.

```cpp
static bool show_settings = false;
if (ImGui::Button("Settings")) {
    show_settings = true;
    ImGui::OpenPopup("Settings");
}
if (auto m = ImScoped::PopupModal("Settings", &show_settings,
                                  ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("...");
    if (ImGui::Button("Close")) {
        show_settings = false;
        ImGui::CloseCurrentPopup();
    }
}
```

The Escape-to-close behavior the user expects from a modal *requires* the `p_open` argument — without it, the title bar has no close button and Escape has nothing to flip.

## Context-menu helpers

Three helpers fold the "open on right-click" pattern into a single call. From `imgui.h:881-883`:

> `BeginPopupContextItem(...)` — open+begin popup when clicked on last item.
> `BeginPopupContextWindow(...)` — open+begin popup when clicked on current window.
> `BeginPopupContextVoid(...)` — open+begin popup when clicked in void (where there are no windows).

All three take an optional `str_id` and `ImGuiPopupFlags`. When `str_id` is null and you used it on a previous item, the popup uses the item's ID as its identifier. As of v1.92.6 the default `popup_flags` value is `0` (per `imgui.h:881`); right-click is the default mouse button via the `ImGuiPopupFlags_MouseButtonRight = 2 << 2` value being implied for these helpers. The bundled `ImScoped::PopupContextItem`/`Window`/`Void` guards default to `flags = 1` to match the pre-1.92.6 default — pass an explicit flag if you need a non-default mouse button.

```cpp
// Right-click on the most recently submitted item
ImGui::Selectable("File 1");
if (auto m = ImScoped::PopupContextItem()) {  // null str_id -> uses item ID
    if (ImGui::MenuItem("Rename")) { /* ... */ }
    if (ImGui::MenuItem("Delete")) { /* ... */ }
}

// Right-click anywhere on the current window's background
if (auto m = ImScoped::PopupContextWindow()) {
    if (ImGui::MenuItem("Properties")) { /* ... */ }
}

// Right-click on empty space (no window underneath)
if (auto m = ImScoped::PopupContextVoid()) {
    if (ImGui::MenuItem("New Window")) { /* ... */ }
}
```

`ImGuiPopupFlags_NoOpenOverItems` (passed to `BeginPopupContextWindow`) suppresses the menu when the cursor is over a child item — useful when items have their own context menu and you want a "blank area" menu only.

## Nesting popups

Popups push onto an internal stack; they nest naturally. A `BeginPopup` inside another popup's body opens a child popup. Closing a parent closes children automatically. `CloseCurrentPopup` only closes the current level — call it again from the parent's body if you need to dismiss multiple levels.

The Demo's "Stacked Modals" example layers three modals; each has its own close button and they unwind in order.

## ID scoping

Popups inherit the parent window's ID stack at the time `OpenPopup` and `BeginPopup` are called. The popup's identity is `hash(parent_window_id, ID_stack..., str_id)`. Per `imgui.h:852`:

> Popup identifiers are relative to the current ID stack, so `OpenPopup` and `BeginPopup` generally needs to be at the same level of the stack.

Two consequences:

1. **Same `str_id` from different windows is safe.** Window A opening "settings" and Window B opening "settings" produce different popup IDs. They don't collide.
2. **Inside a loop, you need `ImScoped::ID` for disambiguation if you open per-row popups.**

```cpp
for (int i = 0; i < items.size(); ++i) {
    ImScoped::ID id{i};
    ImGui::Selectable(items[i].name.c_str());
    if (auto m = ImScoped::PopupContextItem()) {
        // each row's popup is a distinct ID
        if (ImGui::MenuItem("Edit")) edit(items[i]);
    }
}
```

Without the `ImScoped::ID`, all rows' context menus collide on the same popup name and the menu opens for the first row regardless of which one you right-clicked. See [id-stack.md](id-stack.md) for the underlying mechanism.

## Common pitfalls

**"Popup closes immediately" / "Popup never appears."**
`OpenPopup` is being called every frame, or `OpenPopup` is never being called at all. Gate it on an event (button click, menu selection, key press), and call `BeginPopup` unconditionally each frame to detect the open state.

**"Modal won't close on Escape."**
You didn't pass `p_open` to `BeginPopupModal`. Escape needs that bool to toggle. Without it, the modal can only be dismissed by `CloseCurrentPopup` from inside.

**"Right-click context menu fights with another menu."**
Two `BeginPopupContext*` helpers are attached to overlapping items, both grabbing right-click. Either disambiguate them with explicit `str_id` arguments (so only the correct one matches), or use `ImGuiPopupFlags_NoOpenOverExistingPopup` on the secondary one to defer to the primary. Wrap the inner item in `ImScoped::ID` if the conflict is two iterations of the same loop.

**"Popup opens off-screen."**
For non-modal popups, ImGui anchors them at the cursor by default. If the cursor is near a screen edge, the popup may extend off-screen on a non-multi-viewport setup. Use `SetNextWindowPos` *before* `BeginPopup` with a clamped position, or rely on multi-viewport to spawn the popup as its own OS window. For modals, `BeginPopupModal` auto-centers — if you've overridden `SetNextWindowPos` with a stale value, remove that override.

## Idiomatic recipe — confirmation modal

A complete, paste-ready confirmation modal using imscoped guards:

```cpp
if (ImGui::Button("Delete")) {
    ImGui::OpenPopup("confirm-delete");
}
if (auto m = ImScoped::PopupModal("confirm-delete", nullptr,
                                  ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Delete %d items? This cannot be undone.", count);
    ImGui::Separator();
    if (ImGui::Button("OK", ImVec2(120, 0))) {
        perform_delete();
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
    }
}
```

`ImGuiWindowFlags_AlwaysAutoResize` keeps the modal sized to its content — useful when the count or message text varies. The fixed-width buttons (`ImVec2(120, 0)`) keep the layout stable across messages of different length. Both branches call `CloseCurrentPopup`; the imscoped guard's destructor handles `EndPopup` whether the body returned early or ran to completion.

## See also

- [id-stack.md](id-stack.md) — for the ID-collision mechanism that affects per-row context menus.
- [widget-recipes.md](widget-recipes.md) — tooltips and dropdown helpers that are adjacent to popups but follow simpler rules.
- [pitfalls-catalog.md](pitfalls-catalog.md) — cross-cutting index linking popup pitfalls to their root-cause references.
