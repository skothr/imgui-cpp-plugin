# Releasing imtool

Releases are driven by [release-please](https://github.com/googleapis/release-please)
(`.github/workflows/release-please.yml`) from [Conventional Commits](https://www.conventionalcommits.org/).
The bot opens/maintains a **Release PR** that accumulates the version bump +
changelog; **merging that PR cuts the release** (tag + GitHub release). Nothing is
ever auto-merged — the merge is the human release gate.

## Pre-1.0 (beta) cadence

The project is in the `0.x` beta line. `release-please-config.json` encodes:

- `release-type: simple`
- `bump-minor-pre-major: true` — a breaking change bumps the **minor**, never to `1.0.0` automatically.
- `bump-patch-for-minor-pre-major: false` — `feat:` → minor, `fix:` → patch.

So during beta: `fix:` → `0.1.x`, `feat:` and `feat!:`/breaking → `0.x.0`. Reaching
`1.0.0` is a deliberate, manual milestone (a `Release-As: 1.0.0` commit), not something
conventional commits trigger on their own.

## First release must be cut as 0.1.0 (gotcha)

`.release-please-manifest.json` is seeded at `0.1.0`. release-please treats the manifest
as the **last released** version, so the first Release PR generated from `feat:` history
proposes **`0.2.0`**, not `0.1.0`.

To make the first tag read `0.1.0`, do one of:

1. **Seed a `Release-As` commit** before the Release PR is generated (recommended):

   ```
   git commit --allow-empty -m "chore: release 0.1.0" -m "Release-As: 0.1.0"
   ```

   (The `Release-As:` footer is case-insensitive and overrides the computed version.)

2. **Pre-create the tag/release** `v0.1.0` manually, so release-please's next PR continues from there.

## What gets version-bumped

| File | How it's kept in sync |
|---|---|
| `version.txt` | `simple` strategy's built-in default updater (whole-file overwrite; no marker needed). |
| `toolkit/CMakeLists.txt` (`project(... VERSION ...)`) | `extra-files` generic updater via the `x-release-please-version` marker comment. |
| `toolkit/src/version.cpp` (`kVersion`) | `extra-files` generic updater via the `x-release-please-version` marker comment. |
| `.claude-plugin/plugin.json` (`"version"`) | **Not currently managed** — JSON can't carry the line-comment marker the generic updater needs. Tracked as low-priority cleanup (convert to a `json`-type `extra-files` entry with jsonpath `$.version` if published-manifest accuracy becomes load-bearing). |

## Consuming-side note (scaffold pin)

The bootstrap scaffold (`skills/imgui-cpp-development/assets/CMakeLists-imtool-glfw-opengl3.txt.template`)
fetches the toolkit at `IMTOOL_GIT_TAG` (default `main`). Once `0.1.0` is tagged, prefer
repointing that default at the released tag so consumers (e.g. agent-nexus) pin a stable
ref instead of a moving branch. Until the toolkit is on `main` **or** a tag exists, a
default scaffold fetch resolves a ref with no `toolkit/` directory and fails to configure.
