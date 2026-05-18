# imtool conventions

The toolkit's structural ground rules. Anything in this doc takes precedence over individual preferences when contributing. Bumping a convention here is a deliberate scope-change; bumping it implicitly in code is not.

## Namespace and naming

- Single top-level namespace: `imtool`. All public symbols live under it.
- Sub-namespaces only when grouping a closed family of types whose names would clash without them (`imtool::node`, `imtool::view`). Default to flat.
- Types: `CamelCase` (`Vec2f`, `SettingGroup`, `NodeGraphDisplay`).
- Functions and methods: `snake_case` (`length_sq`, `add_input`, `to_json`).
- Concepts: `CamelCase` (`Arithmetic`).
- Member variables: bare `snake_case` (`x`, `y`, `lo`, `hi`). No `m_` prefix. Lifted from 19-logos's `m_` style as part of the modernization sweep.
- Macros: `IMTOOL_UPPER_SNAKE`. Avoid macros wherever a `constexpr`, concept, or template solves the same problem.
- Files: `snake_case.hpp` / `snake_case.cpp`. Directory names match the sub-namespace they hold.

## Ownership and lifetime

The first thing a contributor (human or AI) should know about any pointer-shaped member is which lifetime category it falls into.

- `std::unique_ptr<T>` for **ownership**. One owner; the owner is responsible for destruction. Example: `Application` owns `std::unique_ptr<SettingGroup> settings`; `NodeGraph` owns `std::vector<std::unique_ptr<Node>>`.
- `std::shared_ptr<const T>` for **immutable data flowing through connectors**. Multiple readers may hold the data; nobody mutates it. The `const` is load-bearing — it both prevents downstream mutation and enables safe cross-thread reads.
- Raw `T*` only for **non-owning observers**. Examples: a selection set holding `Node*` pointers into the graph's owned vector; a hover-state field tracking the currently-hovered widget. The observer must outlive the observed; the owner guarantees that ordering.
- Reference `T&` for required, **non-storable** parameters. If the function might store the reference for later use, prefer `T*` (signals that the parameter can be nullptr in some overloads or stored).
- `std::weak_ptr<T>` only for **breaking cycles** in shared-ownership graphs. Should be rare in this toolkit; if it appears more than once or twice, the design has a problem.

Lifetime mismatches are the most common AI-introduced bug class in C++. Following these defaults makes the lifetime category readable from the type, not the comment.

## ImGui boundary

Internal toolkit code uses `imtool::Vec2f` / `imtool::Rect2f`. Conversion to/from ImGui's `ImVec2` / `ImVec4` happens **at the call site** where the ImGui API is invoked, not deeper inside the toolkit.

- Toolkit types are `explicit`ly convertible from/to ImGui types. `Vec2f v{io.MousePos}` and `ImVec2{v}` both compile; implicit cross-boundary conversion does not.
- Use the `ImScoped::` RAII guards (shipped as a skill asset at `skills/imgui-cpp-development/assets/imscoped.hpp`) for every `ImGui::Begin*` call. Pair `Begin` / `End` at compile time, not at runtime.
- Begin/End pairing rules:
  - Top-level `ImGui::Begin(...)` is **always** paired with `ImGui::End()`, regardless of whether `Begin` returned true.
  - `BeginChild` / `BeginPopup` / `BeginTreeNode` pair `End*` **only when the call returned true**.
  - The scope guards encode both rules — use them.

## Error handling

- `std::expected<T, ToolkitError>` at API boundaries that can fail (resource creation, JSON parse, file IO).
- Throw only for programmer errors (preconditions violated, invariants broken) — these are bugs, not runtime conditions.
- No `errno`-style return codes or output-parameter status flags.

(Concrete `ToolkitError` shape is Epic B's responsibility; this convention reserves the slot.)

## File organization

```
toolkit/include/imtool/<subsystem>/<name>.hpp
toolkit/src/<subsystem>/<name>.cpp
toolkit/docs/conventions.md
```

Subsystems map to epics: `common/`, `settings/` (B), `undo/` (C), `node/` (D, E), `view/` (F), `app/` (G).

Headers stay focused — one type per header when the type is non-trivial, related types together when they're tightly coupled (e.g., `Setting<T>` and `SettingGroup`).

## C++23 baseline

- Standard: C++23 (`target_compile_features(imtool PUBLIC cxx_std_23)`).
- Use `std::expected`, `std::print` / `std::println`, ranges where they clarify, `[[nodiscard]]` on returning functions, `constexpr` wherever feasible.
- No `using namespace` at namespace scope in headers.
- No raw `new` / `delete`.
- No bare exceptions in API surface (see Error handling).
- Templates that constrain numeric or pointer-shaped parameters use C++20 concepts, not SFINAE.
