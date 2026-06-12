# imtool conventions

The toolkit's structural ground rules. Anything in this doc takes precedence over individual preferences when contributing. Bumping a convention here is a deliberate scope-change; bumping it implicitly in code is not.

## Namespace and naming

- Single top-level namespace: `imtool` (lowercase) — **locked for 1.0**. All public symbols live under it. The ImGui-family `ImTool::` CamelCase alternative (the Epic A "TBD") was considered and rejected: lowercase `imtool::` matches the `std`-style convention and reads as visibly distinct from ImGui's own `ImXxx` symbols (`imtool::Application` vs `ImGui::Begin`). Renaming after 1.0 is a breaking change, so this is final.
- Sub-namespaces only when grouping a closed family of types whose names would clash without them (`imtool::node`, `imtool::view`). Default to flat.
- Types: `CamelCase` (`Vec2f`, `SettingGroup`, `NodeGraphDisplay`).
- Functions and methods: `camelCase` for member methods (`length`, `addInput`, `toString`) matching prior-art conventions; `snake_case` is acceptable for free functions when it reads more naturally (`to_json`, `from_json` follow the nlohmann convention they integrate with). Mirror the source when lifting.
- Concepts: `CamelCase` (`Arithmetic`).
- Member variables: bare `snake_case` for new code; `camelCase` is retained where the source uses it (lift faithfully, don't sweep). No `m_` prefix on new members.
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

## ImGui boundary — implicit interop

Internal toolkit code uses `imtool::Vec2f` / `imtool::Vec4f` / etc. Interop with ImGui's `ImVec2` / `ImVec4` is **implicit, bidirectional, and free** at the call site — `toolkit/include/imtool/common/imgui_ops.hpp` provides the full operator set so mixed-type expressions (`ImVec2 + Vec2f`, `Vec2f * float * ImVec2`, etc.) compile without explicit conversion.

Rationale: the prior "explicit-only conversion" position was reversed during the Epic A audit. The 19-logos call sites prove the readability cost of explicit conversion is real — every render line gains an explicit `Vec2f(...)` wrap, and the boundary visibility benefit doesn't outweigh that. Implicit interop matches the source pattern and what consumers will actually want.

For cases where the explicit form helps (e.g. converting a typed `Vec2f` to a one-off `ImVec2`), `imtool::toImVec(vec)` is provided.

### Begin/End pairing rules

- Top-level `ImGui::Begin(...)` is **always** paired with `ImGui::End()`, regardless of whether `Begin` returned true.
- `BeginChild` / `BeginPopup` / `BeginTreeNode` pair `End*` **only when the call returned true**.
- Use the `ImScoped::` RAII scope guards shipped at `skills/imgui-cpp-development/assets/imscoped.hpp` to enforce both rules at compile time.

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

Subsystems map to epics: `common/` (Epic A), `settings/` (B), `undo/` (C), `node/` (D, E), `view/` (F), `app/` (G).

Headers stay focused — one type per header when the type is non-trivial, related types together when they're tightly coupled (e.g., `Setting<T>` and `SettingGroup`).

**Never consolidate per-subsystem headers into umbrella mega-headers.** The 19-logos source keeps `vector.hpp`, `rect.hpp`, `matrix.hpp`, `types.hpp` as separate files deliberately — granular includes let consumers pull only what they need, and the per-file separation reflects the design boundaries.

## C++23 baseline

- Standard: C++23 (`target_compile_features(imtool PUBLIC cxx_std_23)`).
- Use `std::expected`, `std::print` / `std::println`, ranges where they clarify, `[[nodiscard]]` on returning functions, `constexpr` wherever feasible.
- No `using namespace` at namespace scope in headers (`using namespace nlohmann;` in 19-logos's `vector.hpp` was an anti-pattern; the lift fully qualifies `nlohmann::json` instead).
- No raw `new` / `delete`.
- No `extern` globals at namespace scope. Use function-static caches inside `inline` accessor functions (one address across all TUs, deferred init).
- No bare exceptions in API surface (see Error handling).
- Templates that constrain numeric or pointer-shaped parameters use C++20 concepts, not SFINAE.

## Carve-outs documented

These features were deliberately excluded from Epic A's lift and have explicit re-engagement triggers:

- **CUDA shims** (`float2`/`float3`/`float4` interop, `__NVCC__` modulo guards). Deferred to a CUDA-integration ticket; lifts back when a downstream consumer wires CUDA into a NodeGraph pipeline.
- **Vector swizzles** (`.xy()`, `.xyz()`, `.xxx()`, etc., ~120 generated methods). YAGNI — zero grep hits in 19-logos's own code. Not re-engaged unless a concrete use case appears.
- **Geometry line/line + rect/line intersection** (`intersects()`/`intersection()` for segment-vs-segment and segment-vs-rect, from `03-astrolograph/inc/base/geometry.hpp`). Not lifted: the toolkit's `geometry.hpp` carries only `lerp` + polar conversions, and `NodeGraphDisplay` draws bezier edges, not the orthogonal/right-angle routing that consumed these in astrolograph. Re-engagement trigger: lifts back if `NodeGraphDisplay` gains orthogonal edge routing. (Note: rect/rect AABB `intersects`/`intersection` *are* present, in `rect.hpp`.)

## Lifted-from index — Epic A common/

Each toolkit header maps to a prior-art source. Modernization is at the surface level (concepts, `constexpr`, `[[nodiscard]]`, scoped enums, function-static caches replacing extern globals); content is preserved unless explicitly flagged in the relevant Linear ticket.

| Toolkit header | Source |
|---|---|
| `include/imtool/common/vector.hpp` | `19-logos/include/vector.hpp` (sans swizzles + CUDA) |
| `include/imtool/common/rect.hpp` | `19-logos/include/rect.hpp` (fixes `operator*`/`operator/` const-version infinite recursion and `/=`-vs-`*=` aliasing bug) |
| `include/imtool/common/matrix.hpp` | `19-logos/include/matrix.hpp` (sans CUDA) |
| `include/imtool/common/type_registry.hpp` | `19-logos/include/types.hpp` (registry + `getTypeIndex<T>`) |
| `include/imtool/common/imgui_ops.hpp` | `19-logos/include/imtools.hpp:25-56` (full free-operator suite, implicit conversion) |
| `include/imtool/common/geometry.hpp` | `astrolograph-old/inc/geometry.hpp` (`lerp` + polar conversions accidentally orphaned in 19-logos lineage). The richer `03-astrolograph/inc/base/geometry.hpp` line/line + rect/line `intersects`/`intersection` were intentionally *not* lifted — see Carve-outs above. |
| `include/imtool/common/range.hpp` | `03-astrolograph/inc/base/range.hpp` (`Range<T>` with contains/clip/span/extend/fit) |
| `include/imtool/common/colors.hpp` | `03-astrolograph/inc/base/colors.hpp` (X11/CSS4 named colors) |
| `include/imtool/common/timing.hpp` | `19-logos/include/utils.hpp` (`getTimestamp`) |
| `include/imtool/common/paths.hpp` | `19-logos/include/utools.hpp` (`getHomeDir`, `getLocalStorageDir`; function-static cache replaces extern global) |
| `include/imtool/common/logging.hpp` + `src/common/logging.cpp` | `19-logos/include/logger.hpp` + `logging.hpp` (scoped `enum class LogLevel`, function-static singleton accessor `imtool::log()` replaces `extern Logger LOG`) |

## Epic G `AppConfig` switches (forward-looking design note)

Some features carried forward from `03-astrolograph/inc/base/mainWindow.hpp` are universally useful but not universally needed. The toolkit's `Application` exposes them via `AppConfig` switches in the same snake-case style as the existing `imgui_docking` / `docking_shift` / `vsync` flags. Working naming (to be finalized when Epic G work starts):

- `multi_project_mode = false` — when `true`, `Application` shows the project-tab UI and treats `loadProject` / `saveProject` / `newProject` / cross-project clipboard as live operations. When `false`, a single implicit project is the only context and the tab UI is hidden.
- `update_thread_enabled = false` — when `true`, `Application` spawns a dedicated update thread (`std::thread mUpdateThread`) with mutex-guarded shared state. When `false`, update logic runs on the render thread.

Default is "feature off, lower setup cost"; consumers opt in by flipping the flag in their `AppConfig` instance. Multi-project and update-thread are decoupled — either can be enabled independently.

Source-of-truth for these design decisions lives on `MAIN-123` in Linear.
