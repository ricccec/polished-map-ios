# Metatile workflow spec — finding & reusing metatiles for complex structures

## Motivation

The core loop of Polished Map is: pick a metatile from the left **sidebar** (a flat,
4-column vertical scroll of up to 256 `Metatile_Button`s) and paint it onto the **map**
on the right. This works well for simple objects (a house is a 2×2 block) but becomes slow
and painful for complex structures:

- **Irregular shapes** like rocky cliffs, where every joint (edge, L-corner, inner corner,
  T-junction) is a *different* metatile that must be hunted down in the sidebar.
- **Background variants** of the same shape — e.g. rocks on grass vs. rocks on sand —
  which are yet more distinct metatiles to find.

Today the only navigation aids are scrolling and ten `0`–`9` hotkeys. There is no search,
filter, grouping, favorites, recents, or way to save/reuse a prebuilt structure.

This spec defines a set of features to reduce the "hunt for the right metatile" cost and
to let users build and reuse complex structures. It is **phased**: quick, low-risk UX wins
first, then heavier features. Everything reusable is associated with the **tileset**, not
the map, so it follows the tiles across every map that uses them.

## Current implementation (reference)

- Sidebar created in `src/main-window.cpp:195-201` (`_sidebar`, a `Workspace` with
  `VERTICAL_ALWAYS` scroll), 4 columns via `METATILES_PER_ROW`.
- Buttons laid out in `update_layout()` (`src/main-window.cpp:2431-2456`), where the
  **grid slot is derived directly from the metatile id**:
  `dx = ms * (i % METATILES_PER_ROW)`, `dy = ms * (i / METATILES_PER_ROW)`.
- Single selection: `_selected` (`src/main-window.h:109`); `select_metatile()`
  (`src/main-window.cpp:2479-2492`) also auto-scrolls, computing the row from `id`.
- Painting: `change_block_cb()` (`src/main-window.cpp:3637-3675`).
- Hotkeys `0`–`9`: `src/main-window.cpp:1006-1012`, maps in `src/main-window.h:115-116`.
- Per-quadrant **collision strings** exist on every metatile
  (`Metatile::collision(Quadrant)`, `src/metatile.h:34`) — e.g. "GRASS", "CLIFF",
  "WATER". Free semantic data to filter by.
- Tileset sidecar files and their path helpers live in `src/config.{h,cpp}`
  (`metatileset_path`, `attributes_path`, `collisions_path`, …).
- An unbuilt rectangular select/move/stamp design already exists in
  `doc/edit-mode-spec.md`; Phase 3 (stamps) builds on it.

---

## Phase 1 — Quick wins

### 1a. Filter / search the picker

Add a one-line text input above the sidebar that filters which metatiles are shown.

**Matching** (case-insensitive substring) against:
- metatile **id** (decimal and hex),
- assigned **hotkey** digit,
- **collision strings** of any quadrant (so `cliff` shows only cliff metatiles).

**Architectural change — decouple display order from id.** Today position is a function
of `id`. Filtering requires an explicit visible list:

- Add `std::vector<uint8_t> _visible_metatiles` (filtered, in ascending id order).
- `rebuild_visible_metatiles()` recomputes it from the current filter string.
- In `update_layout()`, iterate `_visible_metatiles`, position each shown button by its
  **visible index**, and `hide()` buttons not in the list. Set `_sidebar->contents()`
  from the visible count, not the total.
- Update the scroll-to math in `select_metatile()` to use the **visible index** of the id
  rather than the id itself (add a small id→visible-index lookup).
- Hotkey selection (`0`–`9`): if the target id is currently filtered out, clear the filter
  first so the selection becomes visible.

### 1b. Recents & favorites tray

A thin strip between the filter input and the scrollable grid, with two short rows of
`Metatile_Button`-style cells:

- **Recents** — the last ~12 *distinct* metatiles painted, newest first. Push the id in
  `change_block_cb()` when a block is painted. Session-only is acceptable for v1.
- **Favorites** — user-pinned metatiles (the "cliff kit"). Toggle via a right-click
  context entry on a sidebar button (or a dedicated key). **Persisted with the tileset**
  (see Persistence) so they survive reloads.

Clicking a tray cell calls the existing `select_metatile()`. Render via the existing
`Metatile_Button` draw path so zoom/ids/hex settings are honored. The tray + filter input
reduce the sidebar's usable height; account for this in `update_layout()` where the
sidebar `y`/`h` and the dependent ruler / map-scroll geometry are computed
(`src/main-window.cpp:2436-2442`).

**Files touched (Phase 1):** `src/main-window.cpp` (sidebar construction, `update_layout`,
`select_metatile`, new filter callback + `rebuild_visible_metatiles()`, recents push in
`change_block_cb`), `src/main-window.h` (new widgets + `_visible_metatiles`, recents/
favorites state). Reuses `Metatile::collision()`, `_metatile_hotkeys`, `Metatile_Button`.

---

## Phase 2 — Tileset scratch canvas

A second, large freeform grid (separate window or tab) tied to the tileset, where the user
assembles complex structures at leisure and later copies regions into real maps. Reuse the
existing `Map` / `Block` grid and undo machinery (`src/map.{cpp,h}`) for a detached canvas,
persisted as a sidecar keyed to the tileset.

## Phase 3 — Stamp library with variants

A **stamp** = a saved W×H rectangle of metatile ids (e.g. a finished cliff corner). A stamp
may hold multiple **variants** — the same shape on different backgrounds (rock-on-grass vs
rock-on-sand) — cycled while placing.

- **Capture** depends on rectangular region selection from `doc/edit-mode-spec.md` (build
  that first), then add "Save selection as stamp".
- **Placement:** select a stamp → painting stamps the whole block at the cursor; a hotkey
  (e.g. `Tab`) cycles variants; reuse `Map::remember()` for undo.
- **UI:** a collapsible "Stamps" panel (its own `Workspace`) of stamp thumbnails grouped by
  variant. Stamps are **stored with the tileset**, not the map.

New `src/stamps.{cpp,h}` (model + load/save) plus additions to `src/main-window.*`.

## Future / optional — auto-tiling terrain brush

The structural fix for cliffs: define a terrain once (a mapping from *which neighbors share
the terrain* → the correct joint metatile, e.g. the 16-edge or 47-piece "blob" set). The
user then paints the terrain freely and the editor auto-selects the joint and re-resolves
neighbors on edit. Out of scope for the initial phases; recorded here as the long-term
direction. A background-aware variant would also auto-pick rock-on-grass vs rock-on-sand at
boundaries.

---

## Persistence (Phases 1b–3)

Favorites, stamps, and the scratch canvas are tileset-scoped, so they live in a sidecar
file next to the tileset's other data. Add a path helper in `src/config.{h,cpp}` mirroring
`metatileset_path` / `attributes_path`, e.g. `editmeta_path()` → `<tileset>.polishedmap`.
Use a simple line-based text format read/written with the same `fprintf`/`fscanf` style
already used for collisions and attributes (no new JSON dependency). Load it where the
tileset loads (`src/main-window.cpp:1803-1846`) and save it alongside the tileset save
path. Favorites = list of ids; stamps = name + dims + ids (+ variant group); scratch =
reference to its own grid file.

---

## Verification

Build and run the macOS app, open a Prism map + tileset (the `.ablk` flow from `9300c23`):

1. **Filter:** type `cliff` → only cliff-collision metatiles remain; clear → all return.
   Selection highlight and auto-scroll land on the right tile; `0`–`9` hotkeys still select
   (clearing the filter when needed).
2. **Recents:** paint several different metatiles → they appear newest-first; clicking one
   selects it for painting.
3. **Favorites:** pin a few metatiles → they appear in the favorites row; reload the
   tileset → favorites persist.
4. **Regression:** zoom, grid, ids/hex, drag-to-swap, and the GameBoy-screen overlay all
   render correctly under the new sidebar layout.
5. **(Phases 2–3)** Save a stamp / scratch structure, reload the tileset, confirm it
   reappears and stamps correctly with undo.
