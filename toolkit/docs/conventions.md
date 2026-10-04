# imtool conventions

The toolkit's structural ground rules. Anything in this doc takes precedence over individual preferences when contributing. Bumping a convention here is a deliberate scope-change; bumping it implicitly in code is not.

## Namespace and naming

- Single top-level namespace: `imtool` (lowercase) — **locked for 1.0**. All public symbols live under it. The ImGui-family `ImTool::` CamelCase alternative (the Epic A "TBD") was considered and rejected: lowercase `imtool::` matches the `std`-style convention and reads as visibly distinct from ImGui's own `ImXxx` symbols (`imtool::Application` vs `ImGui::Begin`). Renaming after 1.0 is a breaking change, so this is final.
- Sub-namespaces only when grouping a closed family of types whose names would clash without them (`imtool::node`, `imtool::view`). Default to flat.
- Types: `CamelCase` (`Vec2f`, `SettingGroup`, `NodeGraphDisplay`).
- Functions and methods: `camelCase` for member methods (`length`, `addInput`, `toString`) matching prior-art conventions; `snake_case` is acceptable for free functions when it reads more naturally (`to_json`, `from_json` follow the nlohmann convention they integrate with). Mirror the source when lifting.
- Concepts: `CamelCase` (`Arithmetic`).
- Member variables: `m_camelCase` (`m_nextId`, `m_nodes`). Public data members of plain value types keep bare names (`Vec2::x`, `Rect::p1`).
- Macros: `IMTOOL_UPPER_SNAKE`. Avoid macros wherever a `constexpr`, concept, or template solves the same problem.
- Files: `snake_case.hpp` / `snake_case.cpp`. Directories are named after subsystems (see File organization). A subsystem that has a sub-namespace uses the same name for both: `node/` holds `imtool::node`, `view/` holds `imtool::view`. The other directories hold code in the flat `imtool` namespace.

## Ownership and lifetime

The first thing a contributor (human or AI) should know about any pointer-shaped member is which lifetime category it falls into.

- `std::unique_ptr<T>` for **ownership**. One owner; the owner is responsible for destruction. Example: `Application` owns `std::unique_ptr<SettingGroup> m_settings`; `NodeGraph` owns `std::vector<std::unique_ptr<Node>>`.
- `std::shared_ptr<const T>` for **immutable data flowing through connectors**. Multiple readers may hold the data; nobody mutates it. The `const` is load-bearing — it both prevents downstream mutation and enables safe cross-thread reads.
- Raw `T*` only for **non-owning observers**. Examples: a selection set holding `Node*` pointers into the graph's owned vector; a hover-state field tracking the currently-hovered widget. The observed object must outlive every observer that points at it; the owner guarantees that ordering.
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

Each toolkit header maps to a prior-art source. Modernization is at the surface level (concepts, `constexpr`, `[[nodiscard]]`, scoped enums, function-static caches replacing extern globals); content is preserved unless explicitly flagged in the relevant GitHub issue.

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
- `update_thread_enabled = false` — when `true`, `Application` spawns a dedicated update thread (`std::thread m_updateThread`) with mutex-guarded shared state. When `false`, update logic runs on the render thread.

Default is "feature off, lower setup cost"; consumers opt in by flipping the flag in their `AppConfig` instance. Multi-project and update-thread are decoupled — either can be enabled independently.

Source-of-truth for these design decisions is GitHub issue #19 (Epic G).

## Layout

Format with `toolkit/tools/format/imtool_format.py`. `imtool_format.py --check` lists files that are not in this layout and reports control statements without braces. The rules below are what the formatter produces, except where a rule says the formatter only reports.

### Indentation and line length

- Indent 2 spaces. No tabs.
- Keep lines at or under 140 columns. Prefer one long line over several short ones.
- Indent the body of a namespace one level.

  ```cpp
  namespace ng
  {
    class Node;
  }
  ```

- Put `public:`, `protected:` and `private:` at the column of `class`.
- Put `case` and `default` labels at the column of the switch brace. See Braces for the layout of a whole `switch`.
- Keep at most 3 consecutive blank lines.

### Braces

- Put the braces of functions, classes, structs, unions and namespaces on their own line, at the column of the construct.

  ```cpp
  void NodeGraph::clear()
  {
    m_nodes.clear();
    m_nextId = 0;
    m_dirty = true;
    notify();
  }
  ```

- Put the braces of `if`, `else`, `for`, `while`, `do`, `switch`, `try` and `catch` on their own line, indented one level. Indent the body one level further. A `switch` is the one exception for the body: see below.

  ```cpp
  for(auto &n : m_nodes)
    {
      n->step();
      n->draw();
      n->clearFlags();
      n->notify();
    }
  ```

  ```cpp
  try
    {
      load(path);
    }
  catch(const std::exception &e)
    {
      report(e);
    }
  ```

- Indent the braces of an `enum` the same way.

  ```cpp
  enum NodeFlags
    {
      NODE_NONE = 0,
      NODE_SELECTED,
    };
  ```

  The column limit takes precedence. When indenting an `enum` would move one of its lines past 140 columns, the formatter leaves the whole `enum` with its braces at the column of `enum`. A line inside an `enum` that holds only a comment stays at the column it was written at.

- In a `switch`, the brace is indented one level like any control brace. The `case` and `default` labels are at the column of that brace, not one level further. Statements under a label that do not fit on the label line are indented one level from the label. The braces of a block after a label are indented one level from the label, and its statements one level further.

  ```cpp
  switch(kind)
    {
    case Kind::Input: addInput(); break;
    case Kind::Group:
      {
        int n = countChildren();
        resize(n);
        break;
      }
    default:
      warn(kind);
      reset();
      break;
    }
  ```

- Put `else` on its own line after the closing brace.
- Close a `do` body with `} while(cond);` on the brace line.
- Put the brace of a lambda body that does not fit on one line on its own line, at the column where the lambda starts or at the statement's indentation.

  ```cpp
  std::sort(v.begin(), v.end(),
            [&count](const Aspect &a, const Aspect &b)
            {
              count++;
              return a.orb < b.orb;
            });
  ```

- Always put braces around the body of `if`, `else`, `for`, `while` and `do`. The formatter does not add them. It reports each unbraced body with file and line: as an error with `--check`, as a warning when it rewrites. The exit status is 1 either way. An attribute such as `[[likely]]` may stand between the statement and its braces.

  ```cpp
  if(n) { n->step(); }   // not: if(n) n->step();
  ```

### One line, next line, or expanded

These rules apply to function bodies, including member functions defined in a class, and to the bodies of `if`, `else`, `for` and `while`.

- Put a body of one, two or three statements on the line of its statement or signature when the whole line fits in 140 columns.

  ```cpp
  if(n) { n->setId(m_nextId++); }
  for(auto &n : m_nodes) { n->disconnectAll(); n->clearFlags(); }
  Rect& operator=(const Rect &o) { p1 = o.p1; p2 = o.p2; return *this; }
  ```

- When that line would be longer than 140 columns, put the braced body alone on the next line, if that line fits. The body keeps all of its statements on that one line: one, two or three. For a control statement the body line is indented one level. For a function it starts at the column of the signature.

  ```cpp
  if(ImGui::InputDouble(("##" + m_id + "X").c_str(), &v, m_step.x, m_bigStep.x, m_format.c_str(), ImGuiInputTextFlags_EnterReturnsTrue))
    { m_data->x = v; m_changed = true; }
  ```

- The same next-line form is used when the statement or signature already spans several lines, for example a constructor whose initializer list is on its own line.

  ```cpp
  Auth::Auth(const std::string &clientId, const std::string &redirectUrl, const std::string &accessType, const std::string &tokenPath)
    : m_clientId(clientId), m_redirectUrl(redirectUrl), m_accessType(accessType), m_tokenPath(tokenPath)
  { loadToken(); }
  ```

- Otherwise expand the block: when the body line itself does not fit, or the body has four or more statements.
- Never join a body that contains a nested block, a preprocessor line, or a comment. Such a body stays expanded.

  ```cpp
  if(changed)
    {
      // keep the old value for undo
      push(m_value);
    }
  ```

- Write an empty body as `{ }`.
- Keep a constructor's initializer list on the signature line when it fits. Otherwise start a new line at the colon, indented one level, with the initializers packed on that line and commas trailing.
- Keep a `case` body on the label line when it fits.
- Keep a struct or union with a single member, or none, on one line.
- Leave `template<...>` where it was written: on its own line or on the declaration's line.

### Spaces

- No space between a keyword or name and its parenthesis: `if(`, `for(`, `while(`, `switch(`, `catch(`, `f(`.
- No space after `template`: `template<typename T>`.
- No space inside parentheses or after a C-style cast: `(float)x`.
- One space after a comma.
- Spaces around assignment, comparison, logical, additive, shift and ternary operators: `a + b`, `x == y`.
- No spaces around binary `*`, `/` and `%`: `a*b + c/d`.
- A space before an initializer brace and spaces inside it: `A a { x, y };`.
- Do not align `=` or declared names across lines. One space on each side of `=`.

### Pointers and references

- A declared variable, parameter or member takes the `*` or `&`: `Node *n`, `const T &x`.
- A function's return type keeps the `*` or `&`: `Node* find(int id)`, `T& operator[](int i)`.
- A variable initialized with parentheses is a declared variable: `Node *n(graph.find(id));`. The formatter tells it from a function declaration by what is inside the parentheses, so a declaration whose only parameters are unnamed class types (`Node *find(Key);`) is left as written. Name the parameter or bind it by hand.
- With no name after it, the `*` or `&` attaches to the type: `std::vector<Node*>`, `(Node*)p`, `void f(int*, T&)`.

### Naming

- Name member variables `m_camelCase`: `m_nextId`, `m_nodes`. The formatter does not rename anything.

### Includes, comments, macros

- The formatter does not sort or regroup includes.
- The formatter does not re-wrap comments or move trailing comments.
- The formatter does not format macro bodies.

## Kept by hand, not enforced

The formatter does not produce these hand layouts. Each item says what a format run does to one that is already in the code. Put a block between `// clang-format off` and `// clang-format on` to keep its layout; neither clang-format nor the post-pass changes the lines in between.

Removed by the formatter:

- Alignment of `=`, member names, declaration names and argument columns across neighbouring lines, including the extra space that lines up digits under a minus sign. Removed: runs of spaces become one space.
- One-line function bodies lined up in a column across neighbouring functions. Removed.
- Tables of braced initializers with aligned columns. Removed, and a table that fits on one line is joined.
- Aligned `case` bodies. Removed.
- Break points in long argument lists and expressions. Removed: the formatter chooses the breaks.
- Several statements on one line outside braces: `ImGui::SameLine(); ImGui::Text(...)`. Removed: each statement gets its own line.
- A body on its own line, or an expanded block, where the one-line form would fit. Removed: the body is joined.
- An initializer list started on a new line where the signature line would fit it. Removed: the list is joined to the signature.
- Indentation of commented-out code. Removed: a comment line is indented like the code after it. A comment line inside an `enum` is the exception and stays at its column.
- Whitespace on blank lines. Removed: the formatter empties them.

Left as written:

- Trailing comments. The spaces before a trailing comment are left as written, so an aligned column survives until the code before it changes length.
- A comment line under a trailing comment, at that comment's column.
- The second and later lines of a block comment.
- The body of a macro.

## Corpus evidence

Counts are from a survey of 739 hand-formatted files (181,297 lines, 12 projects). "Ruling" marks a point the owner decided where the counts were mixed.

| Rule | Count |
|---|---|
| Namespace body indented one level | 239 of 240 |
| Access specifiers at the `class` column | 746 of 747 |
| `case` at the switch brace column | 2564 of 2564 |
| Blank-line runs of 4 or more | 371 of 21,075 runs |
| Class brace on its own line, not indented | 359 of 359 |
| Namespace brace on its own line, not indented | 241 of 247 |
| Multi-line function body: brace on its own line, not indented | 3922 of 4018 |
| `if` block brace indented one level | 4011 of 4058 expanded blocks |
| `for` block brace indented one level | 1355 of 1363 |
| `switch` brace indented one level | 184 of 185 |
| `do` brace indented, `} while(..);` on the brace line | 4 of 4 |
| `try` / `catch` brace indented one level | 12 of 13 |
| `enum` brace indented one level | 126 of 137 |
| Braced `case` block indented from the label | 64 of 64 |
| `else` on its own line | 3192 of 3411 |
| Expanded lambda body: brace on its own line | 69 of 77 |
| Control statements with braces | 17,781 of 18,144 |
| One-statement control body that fits: on the statement line | 7410 of 8957 bodies whose one-line form is under 140 columns |
| One-statement control body: laid out as "one line if it fits, else next line, else expanded" | 82.1% of 9,252 bodies |
| Two-statement control body that fits: on the statement line | 1080 of 1646 |
| Three-statement control body that fits: on the statement line | 142 of 308 (ruling) |
| One-statement function body that fits: on the signature line | 1955 of 2266 at namespace scope; 2223 of 2346 in class bodies |
| Two-statement function body that fits: on the signature line | 252 of 328 at namespace scope; 237 of 269 in class bodies |
| Three-statement function body that fits: on the signature line | 11 of 38 at namespace scope (ruling); 216 of 225 in class bodies |
| Empty function body written `{ }` | 835 of 851 |
| Initializers packed on one line | 566 of 624 lists with several initializers |
| Initializer list with leading commas | 0 of 624 |
| Colon line of an initializer list indented one level | 449 of 518 |
| `case` body on the label line | 2275 of 2541 |
| Struct written on one line, any number of members | 592 of 982 structs |
| `template<...>` on its own line | 1602 of 3478 (left as written) |
| No space after a C-style cast | 2247 of 2318 |
| Space after a comma | 57,933 of 67,160 |
| Binary `*` `/` `%` without spaces | 8298 of 10,156 |
| Spaces inside initializer braces | 2353 with, 2148 without (ruling) |
| Space before an initializer brace | 537 with, 1493 without (ruling) |
| `=` runs aligned by padding | 1283 of 3216 runs (ruling: not enforced) |
| Declared name takes `*` / `&` | 13,400 of 14,582 |
| Return type keeps `*` / `&` | 1830 of 1832 |
| `<T*>` without a space | 901 of 901 |
| `(T*)` without a space | 287 of 327 |
| Include groups not in alphabetical order | 653 of 908 |
