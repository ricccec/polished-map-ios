# Edit Mode Specification

## 1. Overview

The map editor currently operates in **insert mode**: a metatile is selected in the sidebar,
and left-clicking or dragging on the map places that metatile. This document specifies a new
**edit mode** that lets the user select, move, delete, and spatially expand/contract tiles
already placed on the map.

Edit mode is the active state whenever the BLOCKS mode is in use and _no metatile is selected
in the sidebar_. The user enters it by pressing **Escape**, which deselects the sidebar.
Pressing Escape again (when no selection is active) is a no-op. Double-clicking any tile on the
map re-enters insert mode with that tile's metatile selected.

---

## 2. Terminology

| Term | Meaning |
|------|---------|
| **Metatile** | The 16×16-pixel (or 32×32-px at zoom) visual unit stored in each map cell |
| **Block** | The `Block : Fl_Box` FLTK widget that represents one map cell |
| **Selection** | A rectangular region of map cells highlighted by the user |
| **Anchor handle** | One of 8 small squares drawn at the corners and edge midpoints of a live selection |
| **Edit mode** | State where `_mode == Mode::BLOCKS && _selected == nullptr` |
| **Insert mode** | State where `_mode == Mode::BLOCKS && _selected != nullptr` (current default) |
| **`ms`** | `metatile_size()` — 32 px at normal zoom, 64 px at zoom×2 |
| **map origin** | Screen coordinates of the top-left of the map group: `sx = _map_scroll->x() - _map_scroll->xposition()`, `sy = _map_scroll->y() - _map_scroll->yposition()` |

---

## 3. Design Decisions

| Question | Decision |
|----------|----------|
| How does the user enter edit mode? | Press **Escape** |
| How does the user exit edit mode? | Click a metatile in the sidebar, or double-click a map tile |
| Selection shape | Always rectangular (axis-aligned bounding box) |
| What "delete" fills with | Metatile id `0` (same as new-map default) |
| Move source cells after commit | Filled with metatile id `0` |
| Live preview during move | Yes — source cells ghosted to id 0, destination cells show the moved pattern |
| Anchor handle count | 8 (4 corners + 4 edge midpoints) |
| Drag anchor outward | Tiles the captured pattern into the new cells |
| Drag anchor inward | Blanks (id 0) cells that fall outside the shrunken bounding box |
| Tiling origin | Always relative to the original selection's top-left corner |
FIXME | Clicking inside an existing selection | Starts a move (not a new selection) |
| Clicking outside an existing selection | Starts a new single-cell selection (replaces old one) |
| Selection after undo/redo | Cleared (tile IDs may have changed; stale selection is misleading) |
| Selection after map resize | Cleared (block row/col layout changes) |
| Selection after mode switch to EVENTS | Cleared |
| Selection after map close/open | Cleared (handled implicitly by `_selected = nullptr` in `close_cb`) |

---

## 4. User Interactions

### 4.1 Enter Edit Mode

- User presses **Escape**.
- If a map is not loaded (`!_map.size()`), nothing happens.
- The currently selected `Metatile_Button` is deselected: each button in `_metatile_buttons`
  that has `value() == 1` gets `value(0)`, and `_selected` is set to `nullptr`.
- Any active edit selection is cleared.
- The sidebar redraws.
- Map group redraws (selection highlights disappear if any were showing).

### 4.2 Click to Select (single cell)

While in edit mode, left-click on a map tile that is **not inside** the current selection:

1. `_map_editable` is set to `true` via `Block::handle` FL_PUSH.
2. `change_block_cb` detects edit mode and that the click is not inside the current
   selection nor near an anchor handle.
3. `_edit_action` is set to `SELECTING`.
4. `_edit_drag_start_row/col` is set to `{b->row(), b->col()}`.
5. `_edit_drag_{r1,c1,r2,c2}` is set to `{row, col, row, col}` (1×1 preview rect).
6. Map group redraws to show the one-cell highlight.
7. On FL_RELEASE → `commit_edit_action()` copies the drag rect into `_edit_sel_*`,
   sets `_has_edit_sel = true`, clears `_edit_action`.

### 4.3 Rubber-Band Drag to Select (multiple cells)

Left-click-and-drag starting on an unselected tile:

1. FL_PUSH: same as 4.2 steps 1–6.
2. As the mouse crosses into new Blocks, each Block fires its callback via the
   FL_ENTER-while-button-pressed mechanism.  
3. `change_block_cb` in SELECTING action: updates `_edit_drag_{r1,c1,r2,c2}` to
   `{min(start_row, cur_row), min(start_col, cur_col), max(start_row, cur_row), max(start_col, cur_col)}`.
4. `redraw_overlay()` is called to refresh the rubber-band outline drawn in `draw_overlay()`.
5. Map group redraws to update the per-block highlight.
6. FL_RELEASE → `commit_edit_action()`: finalise selection.

### 4.4 Delete the Selection

With a live selection (`_has_edit_sel == true`) in edit mode, press **Delete** or **Backspace**:

1. `_map.remember()` — snapshot for undo.
2. Every block in `_edit_sel_{r1..r2, c1..c2}` has `id(0)` written.
3. `_map.modified(true)`.
4. `update_active_controls()` (enables Undo button).
5. `edit_clear_selection()`.
6. `_map_group->redraw()`.

### 4.5 Move Selected Tiles

Left-click-and-drag starting **inside** the current selection:

1. FL_PUSH:
   - Detect that `b->row()/col()` is within `_edit_sel_*`.
   - `_map.remember()` — snapshot for undo.
   - Capture `_edit_drag_ids` from current live block IDs within `_edit_sel_*`.
   - Set `_edit_drag_ids_rows/cols` to the selection height/width.
   - Set `_edit_action = MOVING`.
   - Set `_edit_drag_start_row/col = {b->row(), b->col()}`.
   - Set `_edit_move_dr = 0`, `_edit_move_dc = 0`.

2. As mouse moves across new Blocks (FL_ENTER while button pressed):
   - Compute `_edit_move_dr = b->row() - _edit_drag_start_row`.
   - Compute `_edit_move_dc = b->col() - _edit_drag_start_col`.
   - Clamp destination so selection stays within map bounds:  
     `dr` is clamped so `_edit_sel_r1+dr >= 0` and `_edit_sel_r2+dr < _map.height()`.  
     `dc` is clamped so `_edit_sel_c1+dc >= 0` and `_edit_sel_c2+dc < _map.width()`.
   - `_map_group->redraw()` for live preview.

3. FL_RELEASE → `commit_edit_action()` → `edit_commit_move()`:
   - Write `0` to every cell in the original `_edit_sel_*` that is **not** in the
     destination rect.
   - Write `_edit_drag_ids` to every cell in the destination rect (dst_r1 = `_edit_sel_r1
     + _edit_move_dr`, etc.).  Cells in both src and dst are just overwritten correctly
     in one pass (write 0 to src-only cells first, then write ids to dst cells).
   - Update `_edit_sel_*` to the destination bounds.
   - `_edit_move_dr = 0`, `_edit_move_dc = 0`.
   - `_map.modified(true)`, `update_active_controls()`, `_map_group->redraw()`.

**During the preview** (`_edit_action == MOVING`), `Block::draw()` overrides rendering:
- Block is in source rect only → draw metatile id `0`.
- Block is in destination rect → draw `_edit_drag_ids[(row - dst_r1) * cols + (col - dst_c1)]`
  where `dst_r1 = _edit_sel_r1 + _edit_move_dr` etc.
- Block is in both src and dst → draw from `_edit_drag_ids` (destination wins).
- Block is in neither → draw `_id` normally.

### 4.6 Anchor Handles — Expand / Contract

Eight anchor handles are drawn at the corners and edge midpoints of the committed selection
bounding box. When the cursor is within `EDIT_ANCHOR_RADIUS` pixels of a handle, the cursor
changes to the appropriate resize cursor and, on left-click-drag, the selection is resized.

**Anchor positions** (in screen pixels), given:
```
px1 = sx + _edit_sel_c1 * ms
py1 = sy + _edit_sel_r1 * ms
px2 = sx + (_edit_sel_c2 + 1) * ms      // right edge (exclusive)
py2 = sy + (_edit_sel_r2 + 1) * ms      // bottom edge (exclusive)
pmx = (px1 + px2) / 2                   // horizontal midpoint
pmy = (py1 + py2) / 2                   // vertical midpoint
```

| Handle | Position | EditAction | Cursor |
|--------|----------|------------|--------|
| NW | `(px1, py1)` | `RESIZE_NW` | `FL_CURSOR_NW` |
| N  | `(pmx, py1)` | `RESIZE_N`  | `FL_CURSOR_NS` |
| NE | `(px2, py1)` | `RESIZE_NE` | `FL_CURSOR_NE` |
| E  | `(px2, pmy)` | `RESIZE_E`  | `FL_CURSOR_WE` |
| SE | `(px2, py2)` | `RESIZE_SE` | `FL_CURSOR_SE` |
| S  | `(pmx, py2)` | `RESIZE_S`  | `FL_CURSOR_NS` |
| SW | `(px1, py2)` | `RESIZE_SW` | `FL_CURSOR_SW` |
| W  | `(px1, pmy)` | `RESIZE_W`  | `FL_CURSOR_WE` |

On FL_PUSH near an anchor:
1. `_map.remember()`.
2. Capture `_edit_drag_ids` from current selection, set `_edit_drag_ids_rows/cols`.
3. Set `_edit_action` to the corresponding `RESIZE_*`.
4. Set `_edit_drag_start_row/col = {b->row(), b->col()}`.
5. Set `_edit_drag_{r1,c1,r2,c2} = _edit_sel_{r1,c1,r2,c2}` (start from current bounds).

As the mouse moves (FL_ENTER during drag):
- Update only the edges controlled by the anchor. E.g. RESIZE_E only changes `_edit_drag_c2`.
- Clamp to map bounds (`0 .. _map.width()-1` for cols, `0 .. _map.height()-1` for rows).
- Minimum size: 1×1 (never allow a dimension to collapse to zero).
- `redraw_overlay()` + `_map_group->redraw()` for live preview.

**Edge ownership per anchor:**

| Anchor | Edges moved |
|--------|------------|
| N | `_edit_drag_r1` |
| S | `_edit_drag_r2` |
| E | `_edit_drag_c2` |
| W | `_edit_drag_c1` |
| NW | `_edit_drag_r1`, `_edit_drag_c1` |
| NE | `_edit_drag_r1`, `_edit_drag_c2` |
| SE | `_edit_drag_r2`, `_edit_drag_c2` |
| SW | `_edit_drag_r2`, `_edit_drag_c1` |

FL_RELEASE → `commit_edit_action()` → `edit_commit_resize()`:
- Cells in the **new** rect: filled with the tiled pattern (see §4.6.1).
- Cells in the **old** (`_edit_sel_*`) rect but **not** in the new rect: filled with `0`.
- `_edit_sel_*` is updated to `_edit_drag_*`.
- `_map.modified(true)`, `update_active_controls()`, `_map_group->redraw()`.

#### 4.6.1 Tiling Formula

The captured IDs in `_edit_drag_ids` have dimensions `_edit_drag_ids_rows × _edit_drag_ids_cols`
and were captured from the original `_edit_sel_*` bounds. For a destination cell at `(row, col)`:

```
src_row = ((row - _edit_sel_r1) % _edit_drag_ids_rows + _edit_drag_ids_rows) % _edit_drag_ids_rows
src_col = ((col - _edit_sel_c1) % _edit_drag_ids_cols + _edit_drag_ids_cols) % _edit_drag_ids_cols
id = _edit_drag_ids[src_row * _edit_drag_ids_cols + src_col]
```

The modulo arithmetic handles both expansion (wraps around) and contraction (stays within
original pattern) correctly. The tiling always anchors to the original selection's top-left.

**During the preview** (`_edit_action == RESIZE_*`), `Block::draw()` overrides rendering:
- Block is in **new** rect (`_edit_drag_*`) → draw tiled id using the formula above.
- Block is in **old** rect (`_edit_sel_*`) but **not** in new rect → draw id `0` (will be blanked).
- Block is in neither → draw `_id` normally.

### 4.7 Double-Click to Insert Mode

In edit mode, double-clicking any map tile:

1. Retrieve `id = b->id()`.
2. If `id >= _metatileset.size()`, do nothing.
3. Call `select_metatile(_metatile_buttons[id])` — this sets `_selected`, calls `setonly()`,
   and auto-scrolls the sidebar to make the selected metatile visible.
4. Call `edit_clear_selection()`.
5. The app is now in insert mode (`_selected != nullptr`).

This is the primary way to quickly go from edit mode to inserting a specific tile already on the map.

---

## 5. Architecture

### 5.1 Mode Detection

```cpp
// In main-window.h (inline)
inline bool is_edit_mode() const {
    return _mode == Mode::BLOCKS && _selected == nullptr;
}
```

No new `Mode` enum value is needed. Edit mode is a sub-state of `Mode::BLOCKS`.

### 5.2 EditAction State Machine

```
                 ┌─────────────────────────────────────┐
                 ▼                                     │
            ┌──NONE──┐                                 │
            │        │                                 │
   push on  │        │ push inside      push on        │
  unselected│        │ selection        anchor handle   │
   area     │        │                                 │
            ▼        ▼                                 │
        SELECTING  MOVING         RESIZE_{N,S,E,W,     │
            │        │             NE,NW,SE,SW}         │
            │        │                  │               │
            └────────┴──────────────────┘               │
                         FL_RELEASE                     │
                    commit_edit_action() ───────────────┘
```

`_edit_action` is `NONE` when the mouse is not held. It is set on `FL_PUSH` and reset to
`NONE` in `commit_edit_action()`, which is called from `Block::handle()` on `FL_RELEASE`.

### 5.3 Selection State

Two separate rectangles are tracked:

| State | Fields | Meaning |
|-------|--------|---------|
| Committed | `_edit_sel_{r1,c1,r2,c2}`, `_has_edit_sel` | Finalised selection, shown with highlight |
| In-progress | `_edit_drag_{r1,c1,r2,c2}` | Live rubber-band / resize preview |

The committed rect is what persists between mouse operations. The in-progress rect only
exists while `_edit_action != NONE` and is committed to `_edit_sel_*` on `FL_RELEASE`.

---

## 6. New State Fields

Add the following to the `// Work properties` section of `Main_Window`'s private block
in `src/main-window.h`, after `_map_editable`:

```cpp
// Edit mode selection — committed state
int _edit_sel_r1 = 0, _edit_sel_c1 = 0;   // top-left block (inclusive)
int _edit_sel_r2 = 0, _edit_sel_c2 = 0;   // bottom-right block (inclusive)
bool _has_edit_sel = false;

// Edit mode selection — in-progress drag preview
int _edit_drag_r1 = 0, _edit_drag_c1 = 0;
int _edit_drag_r2 = 0, _edit_drag_c2 = 0;

// Edit action state machine
EditAction _edit_action = EditAction::NONE;
int _edit_drag_start_row = 0, _edit_drag_start_col = 0;

// Move offset (used when _edit_action == MOVING)
int _edit_move_dr = 0, _edit_move_dc = 0;

// Captured tile IDs frozen at drag start (used for MOVING and RESIZE_*)
std::vector<uint8_t> _edit_drag_ids;
int _edit_drag_ids_rows = 0, _edit_drag_ids_cols = 0;
```

---

## 7. New Enum

Add before the `Main_Window` class declaration in `src/main-window.h`, next to the
existing `enum class Mode`:

```cpp
enum class EditAction {
    NONE,
    SELECTING,
    MOVING,
    RESIZE_N, RESIZE_S, RESIZE_E, RESIZE_W,
    RESIZE_NE, RESIZE_NW, RESIZE_SE, RESIZE_SW
};
```

---

## 8. New Constants

Add to `src/main-window.h` (near the top defines):

```cpp
// Radius (in screen pixels) within which clicking counts as hitting an anchor handle
#define EDIT_ANCHOR_RADIUS 6
// Visual size (px) of each drawn anchor handle square
#define EDIT_ANCHOR_SIZE   7
```

These are independent of zoom because handles are UI chrome, not map content.

---

## 9. Coordinate Helpers

These inline helpers will be used in multiple places. Add them as `private` inlines in
`Main_Window` in `src/main-window.h`:

```cpp
// Screen origin of the map group (accounts for scroll position)
inline int map_origin_x() const { return _map_scroll->x() - _map_scroll->xposition(); }
inline int map_origin_y() const { return _map_scroll->y() - _map_scroll->yposition(); }

// Convert screen pixel → block column / row (unclamped)
inline int pixel_to_col(int px) const { return (px - map_origin_x()) / metatile_size(); }
inline int pixel_to_row(int py) const { return (py - map_origin_y()) / metatile_size(); }

// Convert block col/row → screen pixel (top-left corner of that block)
inline int col_to_pixel(int col) const { return map_origin_x() + col * metatile_size(); }
inline int row_to_pixel(int row) const { return map_origin_y() + row * metatile_size(); }

// True if block b is inside the committed selection
inline bool in_edit_sel(const Block *b) const {
    return _has_edit_sel
        && b->row() >= _edit_sel_r1 && b->row() <= _edit_sel_r2
        && b->col() >= _edit_sel_c1 && b->col() <= _edit_sel_c2;
}
// True if (row, col) is inside the in-progress drag rect
inline bool in_edit_drag(int row, int col) const {
    return row >= _edit_drag_r1 && row <= _edit_drag_r2
        && col >= _edit_drag_c1 && col <= _edit_drag_c2;
}
```

---

## 10. New Methods

All new methods are added to `Main_Window`. Declaration goes in the `private` section of
`src/main-window.h`. Implementation goes in `src/main-window.cpp`.

---

### 10.1 `is_edit_mode()` — inline in header

```cpp
inline bool is_edit_mode() const {
    return _mode == Mode::BLOCKS && _selected == nullptr;
}
```

---

### 10.2 `deselect_metatile()`

```cpp
void Main_Window::deselect_metatile() {
    if (!_map.size()) { return; }
    if (_selected) {
        _selected->value(0);   // clear the radio button highlight
        _selected = nullptr;
        _sidebar->redraw();
    }
    edit_clear_selection();
}
```

---

### 10.3 `edit_clear_selection()`

```cpp
void Main_Window::edit_clear_selection() {
    _has_edit_sel = false;
    _edit_action = EditAction::NONE;
    _edit_move_dr = 0;
    _edit_move_dc = 0;
    _edit_drag_ids.clear();
    _edit_drag_ids_rows = 0;
    _edit_drag_ids_cols = 0;
    _map_group->redraw();
    redraw_overlay();
}
```

---

### 10.4 `edit_set_committed_sel(int r1, int c1, int r2, int c2)`

Sets the committed selection bounding box (does not capture tile IDs).

```cpp
void Main_Window::edit_set_committed_sel(int r1, int c1, int r2, int c2) {
    _edit_sel_r1 = r1;  _edit_sel_c1 = c1;
    _edit_sel_r2 = r2;  _edit_sel_c2 = c2;
    _has_edit_sel = true;
}
```

---

### 10.5 `edit_capture_ids()`

Captures the current live block IDs from the committed selection into `_edit_drag_ids`.
Must be called while `_edit_sel_*` is already set correctly.

```cpp
void Main_Window::edit_capture_ids() {
    int rows = _edit_sel_r2 - _edit_sel_r1 + 1;
    int cols = _edit_sel_c2 - _edit_sel_c1 + 1;
    _edit_drag_ids_rows = rows;
    _edit_drag_ids_cols = cols;
    _edit_drag_ids.resize((size_t)rows * cols);
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            _edit_drag_ids[(size_t)r * cols + c] =
                _map.block((uint8_t)(_edit_sel_c1 + c),
                           (uint8_t)(_edit_sel_r1 + r))->id();
        }
    }
}
```

---

### 10.6 `edit_tiled_id(int row, int col)`

Returns the tile id for a destination cell at `(row, col)` by tiling the captured pattern.
`row` and `col` are absolute map coordinates; the origin is `_edit_sel_{r1,c1}`.

```cpp
uint8_t Main_Window::edit_tiled_id(int row, int col) const {
    int dr = ((row - _edit_sel_r1) % _edit_drag_ids_rows + _edit_drag_ids_rows)
             % _edit_drag_ids_rows;
    int dc = ((col - _edit_sel_c1) % _edit_drag_ids_cols + _edit_drag_ids_cols)
             % _edit_drag_ids_cols;
    return _edit_drag_ids[(size_t)dr * _edit_drag_ids_cols + dc];
}
```

---

### 10.7 `detect_anchor_at(int ex, int ey)`

Returns the `EditAction` corresponding to the nearest anchor handle if within
`EDIT_ANCHOR_RADIUS`, otherwise `EditAction::NONE`. `ex`, `ey` are screen pixel coordinates.

```cpp
EditAction Main_Window::detect_anchor_at(int ex, int ey) const {
    if (!_has_edit_sel) { return EditAction::NONE; }
    int ms = metatile_size();
    int px1 = col_to_pixel(_edit_sel_c1);
    int py1 = row_to_pixel(_edit_sel_r1);
    int px2 = col_to_pixel(_edit_sel_c2 + 1);   // right edge (exclusive pixel)
    int py2 = row_to_pixel(_edit_sel_r2 + 1);   // bottom edge (exclusive pixel)
    int pmx = (px1 + px2) / 2;
    int pmy = (py1 + py2) / 2;
    int r = EDIT_ANCHOR_RADIUS;

    struct AnchorDef { int ax, ay; EditAction action; };
    static const AnchorDef anchors[] = {
        // corners
        { 0, 0, EditAction::RESIZE_NW }, { 1, 0, EditAction::RESIZE_NE },
        { 0, 1, EditAction::RESIZE_SW }, { 1, 1, EditAction::RESIZE_SE },
        // edges — filled at runtime below
    };
    // Check corners
    if (std::abs(ex - px1) <= r && std::abs(ey - py1) <= r) return EditAction::RESIZE_NW;
    if (std::abs(ex - px2) <= r && std::abs(ey - py1) <= r) return EditAction::RESIZE_NE;
    if (std::abs(ex - px2) <= r && std::abs(ey - py2) <= r) return EditAction::RESIZE_SE;
    if (std::abs(ex - px1) <= r && std::abs(ey - py2) <= r) return EditAction::RESIZE_SW;
    // Check edges
    if (std::abs(ex - pmx) <= r && std::abs(ey - py1) <= r) return EditAction::RESIZE_N;
    if (std::abs(ex - px2) <= r && std::abs(ey - pmy) <= r) return EditAction::RESIZE_E;
    if (std::abs(ex - pmx) <= r && std::abs(ey - py2) <= r) return EditAction::RESIZE_S;
    if (std::abs(ex - px1) <= r && std::abs(ey - pmy) <= r) return EditAction::RESIZE_W;
    return EditAction::NONE;
}
```

---

### 10.8 `edit_delete_selection()`

```cpp
void Main_Window::edit_delete_selection() {
    if (!_has_edit_sel) { return; }
    _map.remember();
    for (int r = _edit_sel_r1; r <= _edit_sel_r2; r++) {
        for (int c = _edit_sel_c1; c <= _edit_sel_c2; c++) {
            _map.block((uint8_t)c, (uint8_t)r)->id(0);
        }
    }
    _map.modified(true);
    update_active_controls();
    edit_clear_selection();
}
```

---

### 10.9 `edit_commit_move()`

Writes the move to the map. The source bounding box is `_edit_sel_*`, the offset is
`_edit_move_{dr,dc}`. The captured IDs are in `_edit_drag_ids`.

```cpp
void Main_Window::edit_commit_move() {
    if (!_has_edit_sel || (_edit_move_dr == 0 && _edit_move_dc == 0)) {
        _edit_action = EditAction::NONE;
        return;
    }
    int dst_r1 = _edit_sel_r1 + _edit_move_dr;
    int dst_c1 = _edit_sel_c1 + _edit_move_dc;
    int dst_r2 = _edit_sel_r2 + _edit_move_dr;
    int dst_c2 = _edit_sel_c2 + _edit_move_dc;
    int cols = _edit_drag_ids_cols;

    // 1. Blank cells that are in src but not in dst
    for (int r = _edit_sel_r1; r <= _edit_sel_r2; r++) {
        for (int c = _edit_sel_c1; c <= _edit_sel_c2; c++) {
            bool in_dst = (r >= dst_r1 && r <= dst_r2 && c >= dst_c1 && c <= dst_c2);
            if (!in_dst) {
                _map.block((uint8_t)c, (uint8_t)r)->id(0);
            }
        }
    }
    // 2. Write captured ids into the destination rect
    for (int r = dst_r1; r <= dst_r2; r++) {
        for (int c = dst_c1; c <= dst_c2; c++) {
            if (r < 0 || r >= (int)_map.height() || c < 0 || c >= (int)_map.width()) {
                continue; // clipped by map boundary
            }
            int dr = r - dst_r1;
            int dc = c - dst_c1;
            _map.block((uint8_t)c, (uint8_t)r)->id(
                _edit_drag_ids[(size_t)dr * cols + dc]);
        }
    }
    // 3. Update committed selection to the destination bounds
    _edit_sel_r1 = dst_r1; _edit_sel_c1 = dst_c1;
    _edit_sel_r2 = dst_r2; _edit_sel_c2 = dst_c2;
    _edit_move_dr = 0;
    _edit_move_dc = 0;
    _edit_action = EditAction::NONE;
    _map.modified(true);
    update_active_controls();
    _map_group->redraw();
    redraw_overlay();
}
```

---

### 10.10 `edit_commit_resize()`

Writes the resize to the map. The original selection was `_edit_sel_*`; the new bounds
are `_edit_drag_*`. Tile IDs are in `_edit_drag_ids` (captured from `_edit_sel_*`).

```cpp
void Main_Window::edit_commit_resize() {
    int new_r1 = _edit_drag_r1, new_c1 = _edit_drag_c1;
    int new_r2 = _edit_drag_r2, new_c2 = _edit_drag_c2;

    // 1. Blank cells in old rect but outside the new rect
    for (int r = _edit_sel_r1; r <= _edit_sel_r2; r++) {
        for (int c = _edit_sel_c1; c <= _edit_sel_c2; c++) {
            bool in_new = (r >= new_r1 && r <= new_r2 && c >= new_c1 && c <= new_c2);
            if (!in_new) {
                _map.block((uint8_t)c, (uint8_t)r)->id(0);
            }
        }
    }
    // 2. Fill all cells in the new rect with the tiled pattern
    for (int r = new_r1; r <= new_r2; r++) {
        for (int c = new_c1; c <= new_c2; c++) {
            _map.block((uint8_t)c, (uint8_t)r)->id(edit_tiled_id(r, c));
        }
    }
    // 3. Commit new bounds
    _edit_sel_r1 = new_r1; _edit_sel_c1 = new_c1;
    _edit_sel_r2 = new_r2; _edit_sel_c2 = new_c2;
    _edit_action = EditAction::NONE;
    _map.modified(true);
    update_active_controls();
    _map_group->redraw();
    redraw_overlay();
}
```

---

### 10.11 `commit_edit_action()`

Dispatcher called from `Block::handle()` on `FL_RELEASE` when in edit mode.

```cpp
void Main_Window::commit_edit_action() {
    switch (_edit_action) {
    case EditAction::SELECTING:
        // Finalise rubber-band selection
        edit_set_committed_sel(_edit_drag_r1, _edit_drag_c1,
                               _edit_drag_r2, _edit_drag_c2);
        _edit_action = EditAction::NONE;
        _map_group->redraw();
        redraw_overlay();
        break;
    case EditAction::MOVING:
        edit_commit_move();
        break;
    case EditAction::RESIZE_N:  case EditAction::RESIZE_S:
    case EditAction::RESIZE_E:  case EditAction::RESIZE_W:
    case EditAction::RESIZE_NE: case EditAction::RESIZE_NW:
    case EditAction::RESIZE_SE: case EditAction::RESIZE_SW:
        edit_commit_resize();
        break;
    default:
        break;
    }
}
```

---

### 10.12 `edit_handle_push(Block *b)` — helper

Called from `change_block_cb` when in edit mode on the initial `FL_PUSH`. Determines and
starts the correct action.

```cpp
void Main_Window::edit_handle_push(Block *b) {
    int ex = Fl::event_x(), ey = Fl::event_y();

    // 1. Double-click → enter insert mode
    if (Fl::event_clicks() > 0) {
        uint8_t id = b->id();
        if (id < _metatileset.size()) {
            select_metatile(_metatile_buttons[id]);
            edit_clear_selection();
        }
        return;
    }

    // 2. Near an anchor handle → resize
    EditAction anchor = detect_anchor_at(ex, ey);
    if (anchor != EditAction::NONE) {
        _map.remember();
        edit_capture_ids();
        _edit_action = anchor;
        _edit_drag_start_row = b->row();
        _edit_drag_start_col = b->col();
        _edit_drag_r1 = _edit_sel_r1; _edit_drag_c1 = _edit_sel_c1;
        _edit_drag_r2 = _edit_sel_r2; _edit_drag_c2 = _edit_sel_c2;
        return;
    }

    // 3. Inside committed selection → move
    if (_has_edit_sel && in_edit_sel(b)) {
        _map.remember();
        edit_capture_ids();
        _edit_action = EditAction::MOVING;
        _edit_drag_start_row = b->row();
        _edit_drag_start_col = b->col();
        _edit_move_dr = 0;
        _edit_move_dc = 0;
        return;
    }

    // 4. Elsewhere → start a new rubber-band selection
    _edit_action = EditAction::SELECTING;
    _edit_drag_start_row = b->row();
    _edit_drag_start_col = b->col();
    _edit_drag_r1 = _edit_drag_r2 = b->row();
    _edit_drag_c1 = _edit_drag_c2 = b->col();
    _has_edit_sel = false;   // hide old highlight while dragging new one
    _map_group->redraw();
    redraw_overlay();
}
```

---

### 10.13 `edit_handle_enter(Block *b)` — helper

Called from `change_block_cb` when in edit mode on FL_ENTER while mouse button is pressed
(i.e., `_edit_action != NONE`).

```cpp
void Main_Window::edit_handle_enter(Block *b) {
    int row = (int)b->row(), col = (int)b->col();
    int map_w = (int)_map.width(), map_h = (int)_map.height();

    switch (_edit_action) {
    case EditAction::SELECTING: {
        int r1 = MIN(_edit_drag_start_row, row);
        int c1 = MIN(_edit_drag_start_col, col);
        int r2 = MAX(_edit_drag_start_row, row);
        int c2 = MAX(_edit_drag_start_col, col);
        _edit_drag_r1 = r1; _edit_drag_c1 = c1;
        _edit_drag_r2 = r2; _edit_drag_c2 = c2;
        _map_group->redraw();
        redraw_overlay();
        break;
    }
    case EditAction::MOVING: {
        int dr = row - _edit_drag_start_row;
        int dc = col - _edit_drag_start_col;
        // Clamp so destination stays within map bounds
        dr = std::clamp(dr, -_edit_sel_r1, map_h - 1 - _edit_sel_r2);
        dc = std::clamp(dc, -_edit_sel_c1, map_w - 1 - _edit_sel_c2);
        _edit_move_dr = dr;
        _edit_move_dc = dc;
        _map_group->redraw();
        break;
    }
    case EditAction::RESIZE_N:
    case EditAction::RESIZE_NW:
    case EditAction::RESIZE_NE:
        _edit_drag_r1 = std::clamp(row, 0, _edit_drag_r2);
        if (_edit_action == EditAction::RESIZE_NW ||
            _edit_action == EditAction::RESIZE_W)  goto resize_w;
        if (_edit_action == EditAction::RESIZE_NE ||
            _edit_action == EditAction::RESIZE_E)  goto resize_e;
        goto done_resize;
    case EditAction::RESIZE_S:
    case EditAction::RESIZE_SW:
    case EditAction::RESIZE_SE:
        _edit_drag_r2 = std::clamp(row, _edit_drag_r1, map_h - 1);
        if (_edit_action == EditAction::RESIZE_SW) goto resize_w;
        if (_edit_action == EditAction::RESIZE_SE) goto resize_e;
        goto done_resize;
    case EditAction::RESIZE_W:
    resize_w:
        _edit_drag_c1 = std::clamp(col, 0, _edit_drag_c2);
        goto done_resize;
    case EditAction::RESIZE_E:
    resize_e:
        _edit_drag_c2 = std::clamp(col, _edit_drag_c1, map_w - 1);
        goto done_resize;
    done_resize:
        _map_group->redraw();
        redraw_overlay();
        break;
    default:
        break;
    }
}
```

> **Note on resize logic:** The `goto` labels above are a concise way to share the
> W/E column-update code between corner and edge anchors. If preferred, this can be
> refactored into small inline helpers (`update_drag_r1`, `update_drag_c1`, etc.) to
> avoid gotos entirely — the behaviour is identical.

---

## 11. Modified Existing Code

### 11.1 `src/main-window.h`

1. Add `EditAction` enum (§7) before the `Main_Window` class declaration.
2. Add new state fields (§6) in the `// Work properties` section.
3. Add new method declarations in the `private` section:
   ```cpp
   bool is_edit_mode() const;          // inline — see §10.1
   void deselect_metatile();
   void edit_clear_selection();
   void edit_set_committed_sel(int r1, int c1, int r2, int c2);
   void edit_capture_ids();
   uint8_t edit_tiled_id(int row, int col) const;
   EditAction detect_anchor_at(int ex, int ey) const;
   void edit_delete_selection();
   void edit_commit_move();
   void edit_commit_resize();
   void commit_edit_action();
   void edit_handle_push(Block *b);
   void edit_handle_enter(Block *b);
   ```
4. Add coordinate helper inlines (§9) in the `public` section.
5. Add new constants `EDIT_ANCHOR_RADIUS` and `EDIT_ANCHOR_SIZE` (§8) at the top of the file.

### 11.2 `Main_Window::handle()` — `src/main-window.cpp` (around line 952)

Add Escape, Delete, and Backspace key handling inside the `FL_SHORTCUT` case, **before** the
`handle_hotkey` call:

```cpp
case FL_SHORTCUT:
    key = Fl::event_key();
    // NEW: edit mode keyboard shortcuts
    if (key == FL_Escape && _map.size()) {
        deselect_metatile();          // clears _selected, clears edit selection
        return 1;
    }
    if ((key == FL_Delete || key == FL_BackSpace) && is_edit_mode() && _has_edit_sel) {
        edit_delete_selection();
        return 1;
    }
    // existing hotkey handling
    if (handle_hotkey(key)) { return 1; }
    [[fallthrough]];
```

### 11.3 `Main_Window::change_block_cb()` — `src/main-window.cpp` (around line 3597)

Replace the existing guard at the top of the function with an early branch for edit mode:

```cpp
void Main_Window::change_block_cb(Block *b, Main_Window *mw) {
    if (!mw->_map_editable || mw->_mode != Mode::BLOCKS) { return; }

    // NEW: Edit mode branch
    if (mw->is_edit_mode()) {
        if (Fl::event_button() != FL_LEFT_MOUSE) { return; }
        if (Fl::event_is_click()) {
            // First contact on this drag gesture
            mw->edit_handle_push(b);
        }
        else if (mw->_edit_action != EditAction::NONE) {
            // Continuing drag (FL_ENTER while button held)
            mw->edit_handle_enter(b);
        }
        return;   // never falls through to insert-mode logic
    }

    // ── existing insert-mode logic below (unchanged) ──────────────────────
    if (Fl::event_button() == FL_LEFT_MOUSE) {
        if (!mw->_selected) { return; }
        // ... rest of existing code unchanged
```

> `Fl::event_is_click()` returns non-zero only on the initial FL_PUSH that starts a gesture,
> not during the drag-enter phase. This correctly distinguishes "new press" from "drag enters
> this block".

### 11.4 `Main_Window::draw_overlay()` — `src/main-window.cpp` (around line 942)

Extend to draw the rubber-band rect and anchor handles:

```cpp
void Main_Window::draw_overlay() {
    // existing: Game Boy screen overlay
    if (_gameboy_screen) {
        // ... existing code unchanged ...
    }

    // NEW: Edit mode overlay
    if (!is_edit_mode()) { return; }

    int ms = metatile_size();
    int sx = map_origin_x(), sy = map_origin_y();

    // Rubber-band: draw dashed outline of the in-progress SELECTING drag
    if (_edit_action == EditAction::SELECTING) {
        int x1 = sx + _edit_drag_c1 * ms;
        int y1 = sy + _edit_drag_r1 * ms;
        int w  = (_edit_drag_c2 - _edit_drag_c1 + 1) * ms;
        int h  = (_edit_drag_r2 - _edit_drag_r1 + 1) * ms;
        fl_line_style(FL_DASH, 1);
        fl_color(FL_WHITE);
        fl_rect(x1, y1, w, h);
        fl_color(FL_BLACK);
        fl_rect(x1 + 1, y1 + 1, w - 2, h - 2);
        fl_line_style(0);
    }

    // Anchor handles: draw only when there is a committed selection
    if (_has_edit_sel) {
        int px1 = col_to_pixel(_edit_sel_c1);
        int py1 = row_to_pixel(_edit_sel_r1);
        int px2 = col_to_pixel(_edit_sel_c2 + 1);
        int py2 = row_to_pixel(_edit_sel_r2 + 1);
        int pmx = (px1 + px2) / 2;
        int pmy = (py1 + py2) / 2;
        int hs = EDIT_ANCHOR_SIZE / 2;

        int handle_xs[] = { px1, pmx, px2, px2, px2, pmx, px1, px1 };
        int handle_ys[] = { py1, py1, py1, pmy, py2, py2, py2, pmy };

        for (int i = 0; i < 8; i++) {
            int hx = handle_xs[i] - hs;
            int hy = handle_ys[i] - hs;
            fl_color(FL_WHITE);
            fl_rectf(hx, hy, EDIT_ANCHOR_SIZE, EDIT_ANCHOR_SIZE);
            fl_color(FL_BLACK);
            fl_rect(hx, hy, EDIT_ANCHOR_SIZE, EDIT_ANCHOR_SIZE);
        }
    }
}
```

### 11.5 `Block::draw()` — `src/map-buttons.cpp` (around line 125)

Add edit-mode rendering after the normal `draw_map_button` call. The function currently ends with
an event-cursor draw; insert the new code between the normal draw and the event-cursor block:

```cpp
void Block::draw() {
    Main_Window *mw = (Main_Window *)user_data();
    bool below_mouse = Fl::belowmouse() == this;
    bool event_cursor = mw->mode() == Mode::EVENTS;

    // Determine the id to display (may be overridden in edit mode preview)
    uint8_t display_id = _id;
    bool show_sel_highlight = false;

    if (mw->is_edit_mode()) {
        int row = (int)_row, col = (int)_col;
        EditAction ea = mw->edit_action();   // new public accessor — see §11.6

        if (ea == EditAction::MOVING) {
            int dr = mw->edit_move_dr(), dc = mw->edit_move_dc();
            int dst_r1 = mw->edit_sel_r1() + dr, dst_c1 = mw->edit_sel_c1() + dc;
            int dst_r2 = mw->edit_sel_r2() + dr, dst_c2 = mw->edit_sel_c2() + dc;
            bool in_src = (row >= mw->edit_sel_r1() && row <= mw->edit_sel_r2()
                        && col >= mw->edit_sel_c1() && col <= mw->edit_sel_c2());
            bool in_dst = (row >= dst_r1 && row <= dst_r2
                        && col >= dst_c1 && col <= dst_c2);
            if (in_dst) {
                int dr2 = row - dst_r1, dc2 = col - dst_c1;
                display_id = mw->edit_drag_id(dr2, dc2);   // new accessor — see §11.6
                show_sel_highlight = true;
            } else if (in_src) {
                display_id = 0;
            }
        }
        else if (ea == EditAction::RESIZE_N  || ea == EditAction::RESIZE_S  ||
                 ea == EditAction::RESIZE_E  || ea == EditAction::RESIZE_W  ||
                 ea == EditAction::RESIZE_NE || ea == EditAction::RESIZE_NW ||
                 ea == EditAction::RESIZE_SE || ea == EditAction::RESIZE_SW) {
            bool in_new = mw->in_edit_drag(row, col);
            bool in_old = (row >= mw->edit_sel_r1() && row <= mw->edit_sel_r2()
                        && col >= mw->edit_sel_c1() && col <= mw->edit_sel_c2());
            if (in_new) {
                display_id = mw->edit_tiled_id(row, col);
                show_sel_highlight = true;
            } else if (in_old) {
                display_id = 0;
            }
        }
        else {
            // NONE or SELECTING: just show highlight on committed selection
            show_sel_highlight = mw->has_edit_sel() && mw->in_edit_sel(this);
        }
    }

    draw_map_button(this, display_id, below_mouse && !event_cursor);

    // Selection highlight (cyan inset border)
    if (show_sel_highlight) {
        int ms = mw->metatile_size();
        fl_color(fl_color_average(FL_CYAN, FL_WHITE, 0.6f));
        fl_rect(x(), y(), ms, ms);
        fl_rect(x() + 1, y() + 1, ms - 2, ms - 2);
    }

    // existing: events-mode cursor quad
    if (!below_mouse || !event_cursor) { return; }
    int hx = x() + right_half() * w() / 2, hy = y() + bottom_half() * h() / 2;
    int hs = mw->metatile_size() / 2;
    draw_selection_border(hx, hy, hs, mw->zoom());
}
```

### 11.6 New public accessors on `Main_Window` (for `Block::draw()`)

`Block::draw()` is in `map-buttons.cpp` and calls through to `Main_Window` via `user_data()`.
It needs read access to several new fields. Add these `public` inline accessors to
`src/main-window.h`:

```cpp
inline EditAction edit_action() const { return _edit_action; }
inline bool has_edit_sel() const { return _has_edit_sel; }
inline int edit_sel_r1() const { return _edit_sel_r1; }
inline int edit_sel_c1() const { return _edit_sel_c1; }
inline int edit_sel_r2() const { return _edit_sel_r2; }
inline int edit_sel_c2() const { return _edit_sel_c2; }
inline int edit_move_dr() const { return _edit_move_dr; }
inline int edit_move_dc() const { return _edit_move_dc; }
inline uint8_t edit_drag_id(int dr, int dc) const {
    if (_edit_drag_ids.empty()) { return 0; }
    return _edit_drag_ids[(size_t)dr * _edit_drag_ids_cols + (size_t)dc];
}
```

`in_edit_sel(const Block *)` and `in_edit_drag(int row, int col)` and `edit_tiled_id(int,int)`
are already declared as public (§9 / §10.6).

### 11.7 `Block::handle()` — `src/map-buttons.cpp` (around line 170)

Add two changes:

**a) FL_RELEASE — trigger `commit_edit_action()`:**

```cpp
case FL_RELEASE:
    mw->map_editable(false);
    if (mw->is_edit_mode()) {
        mw->commit_edit_action();
    }
    return 1;
```

**b) FL_MOVE — update cursor when over an anchor handle:**

```cpp
case FL_MOVE:
    if (mw->is_edit_mode() && mw->has_edit_sel()) {
        EditAction anchor = mw->detect_anchor_at(Fl::event_x(), Fl::event_y());
        switch (anchor) {
        case EditAction::RESIZE_NW: fl_cursor(FL_CURSOR_NW);  break;
        case EditAction::RESIZE_N:  fl_cursor(FL_CURSOR_NS);  break;
        case EditAction::RESIZE_NE: fl_cursor(FL_CURSOR_NE);  break;
        case EditAction::RESIZE_E:  fl_cursor(FL_CURSOR_WE);  break;
        case EditAction::RESIZE_SE: fl_cursor(FL_CURSOR_SE);  break;
        case EditAction::RESIZE_S:  fl_cursor(FL_CURSOR_NS);  break;
        case EditAction::RESIZE_SW: fl_cursor(FL_CURSOR_SW);  break;
        case EditAction::RESIZE_W:  fl_cursor(FL_CURSOR_WE);  break;
        default:                    fl_cursor(FL_CURSOR_DEFAULT); break;
        }
    }
    mw->update_event_cursor(this);
    if (mw->mode() == Mode::EVENTS && mw->gameboy_screen()) {
        mw->update_gameboy_screen(this);
    }
    redraw();
    return 1;
```

> `FL/Fl.H` must be included in `map-buttons.cpp` for `fl_cursor()`. Currently only
> `FL/fl_draw.H` is included there. Add `#include <FL/Fl.H>` to the includes.

### 11.8 Lifecycle hooks — clear edit selection in these existing functions

Add `edit_clear_selection()` (or the equivalent inlined reset) at the indicated locations:

| Location | Line (approx) | Change |
|----------|---------------|--------|
| `close_cb` | ~2565 (after `_selected = NULL`) | Call `edit_clear_selection()` |
| `undo_cb` | ~2983 (before `redraw()`) | Call `edit_clear_selection()` |
| `redo_cb` | ~2988 (before `redraw()`) | Call `edit_clear_selection()` |
| `resize_map` | ~1982 (before final `redraw()`) | Call `edit_clear_selection()` |
| `events_mode_cb` | ~3291 (before `redraw()`) | Call `edit_clear_selection()` |
| `switch_mode_cb` | ~3298 (before `redraw()`, when switching to EVENTS) | Call `edit_clear_selection()` |

`close_cb` already sets `_selected = NULL`; ours is purely the selection reset. After adding
`edit_clear_selection()` in `close_cb`, its guard `if (!_map.size())` means calling it
after `_map.clear()` is safe because `edit_clear_selection()` does not touch map blocks.

---

## 12. Rendering Summary

### 12.1 Selection Highlight (`Block::draw()`)

- Drawn as a 2-pixel wide cyan inset border over the Block's area.
- Shown on all blocks inside the committed selection (`_edit_sel_*`) when
  `_edit_action == NONE` or `SELECTING`.
- Shown on destination blocks during MOVING preview.
- Shown on blocks in the new (`_edit_drag_*`) rect during RESIZE preview.

### 12.2 Rubber-Band Outline (`draw_overlay()`)

- Drawn only during `_edit_action == SELECTING`.
- Two concentric `fl_rect` calls: outer white, inner black, both dashed.
- Covers the `_edit_drag_*` rect (updates on every FL_ENTER during drag).

### 12.3 Anchor Handles (`draw_overlay()`)

- Drawn whenever `_has_edit_sel` is true (regardless of `_edit_action`).
- Eight `EDIT_ANCHOR_SIZE × EDIT_ANCHOR_SIZE` squares: white fill, black border.
- Centred on the 8 anchor pixel positions (see §4.6 anchor position table).

### 12.4 Move Preview (`Block::draw()`)

- Source-only blocks render as metatile id `0`.
- Destination blocks render from `_edit_drag_ids` at the offset `(row-dst_r1, col-dst_c1)`.
- Blocks in both src and dst render from `_edit_drag_ids` (destination wins).

### 12.5 Resize Preview (`Block::draw()`)

- Cells in the new (`_edit_drag_*`) rect render via `edit_tiled_id()`.
- Cells in the old (`_edit_sel_*`) rect but outside the new rect render as id `0`.

---

## 13. Undo/Redo Integration

`_map.remember()` is called **once at the start of each mutating gesture** (push):

| Action | When `remember()` is called |
|--------|----------------------------|
| Delete | At the start of `edit_delete_selection()` |
| Move | In `edit_handle_push()` when action is MOVING |
| Resize | In `edit_handle_push()` when action is RESIZE_* |

Selection itself (SELECTING action) is not a map mutation and does not call `remember()`.

After undo/redo, `edit_clear_selection()` is called to avoid showing a stale highlight.

---

## 14. Selection Lifecycle

| Event | Selection cleared? |
|-------|--------------------|
| Escape pressed (enter edit mode) | Yes — via `deselect_metatile()` |
| Click outside selection | Yes — replaced with new 1-cell selection |
| Double-click (exit to insert mode) | Yes — via `edit_clear_selection()` |
| Delete key | Yes — via `edit_delete_selection()` |
| Undo / redo | Yes — `edit_clear_selection()` added to those callbacks |
| Map resize | Yes — `edit_clear_selection()` added to `resize_map` |
| Switch to EVENTS mode | Yes — `edit_clear_selection()` added to mode callbacks |
| Map close / open | Yes — `close_cb` sets `_selected = NULL` (triggers edit clear too) |
| Move commit | No — selection follows the tiles to the new position |
| Resize commit | No — selection updates to the new bounds |

---

## 15. Implementation Phases

Implement in this order to allow building and testing after each phase.

---

### Phase 1 — Mode Infrastructure (no visible behaviour yet)

**Goal:** Escape key switches out of insert mode. All existing insert-mode behaviour is
completely unaffected.

Steps:
1. Add `EditAction` enum to `src/main-window.h` (§7).
2. Add all new state fields to `Main_Window` private section (§6).
3. Add `EDIT_ANCHOR_RADIUS`, `EDIT_ANCHOR_SIZE` constants (§8).
4. Add `is_edit_mode()` inline (§10.1).
5. Add all new method declarations to the `private` section of `Main_Window` (§11.1 step 3).
6. Add public accessors for `Block::draw()` (§11.6).
7. Add coordinate helpers (§9).
8. Implement `deselect_metatile()` (§10.2).
9. Implement `edit_clear_selection()` (§10.3).
10. In `Main_Window::handle()`, add the Escape case before `handle_hotkey` (§11.2 — Escape only; skip Delete/Backspace for now).
11. In `change_block_cb`, add the early-return edit-mode stub:
    ```cpp
    if (mw->is_edit_mode()) { return; }
    ```
    This prevents any tile placement while in edit mode.
12. Add `edit_clear_selection()` calls in `close_cb`, `undo_cb`, `redo_cb`, `resize_map`,
    `events_mode_cb`, `switch_mode_cb` (§11.8).

**Verify:**
- Build succeeds with no warnings.
- Load a map. Press Escape → sidebar button deselects, clicks no longer place tiles.
- Click a sidebar button → insert mode restored, tiles can be placed.
- Undo/redo still work.
- Mode switch (Tab key / Mode menu) still works.

---

### Phase 2 — Click-to-Select and Delete

**Goal:** Clicking a tile in edit mode selects it (cyan border). Delete key blanks it.

Steps:
1. Implement `edit_set_committed_sel()` (§10.4).
2. Implement `edit_handle_push()` (§10.12) — only the SELECTING branch is needed now
   (anchor and move branches will call not-yet-implemented methods; add `return` stubs
   or leave the full implementation — all referenced methods exist or are stubs).
3. Implement `edit_handle_enter()` (§10.13) — only SELECTING branch.
4. Implement `commit_edit_action()` (§10.11) — only SELECTING case.
5. Replace the edit-mode stub in `change_block_cb` with the full edit-mode branch (§11.3).
6. Modify `Block::draw()` to draw the cyan selection highlight for NONE/SELECTING states (§11.5 — only the `show_sel_highlight` path for those states; MOVING/RESIZE paths can be `else {}` stubs for now).
7. Modify `Block::handle()` FL_RELEASE to call `commit_edit_action()` (§11.7a).
8. Add Delete/Backspace handler in `Main_Window::handle()` (§11.2).
9. Implement `edit_delete_selection()` (§10.8).

**Verify:**
- Escape to edit mode. Click a tile → single cell cyan highlight appears.
- Drag across multiple tiles → rectangle highlighted on release.
- Clicking elsewhere → new selection, old highlight gone.
- Delete key → selected cells become metatile 0. Ctrl+Z undoes.
- Undo clears the selection.

---

### Phase 3 — Rubber-Band Overlay

**Goal:** During a drag-select, show a live dashed outline.

Steps:
1. Extend `draw_overlay()` with the SELECTING rubber-band drawing (§11.4).
2. Ensure `redraw_overlay()` is called in `edit_handle_enter()` for SELECTING.

**Verify:**
- Drag across tiles: dashed white/black outline tracks the drag rectangle live.
- On release, outline disappears; selection highlight takes its place.

---

### Phase 4 — Double-Click to Insert Mode

**Goal:** Double-clicking a tile in edit mode selects it in the sidebar and enters insert mode.

Steps:
1. The double-click case is already in `edit_handle_push()` (§10.12 step 1). Confirm it is
   reached and that `select_metatile(_metatile_buttons[id])` is called correctly.

**Verify:**
- In edit mode, double-click any tile → that metatile is highlighted in the sidebar.
- Tile placement works immediately (insert mode active).
- Sidebar auto-scrolls to show the selected metatile.

---

### Phase 5 — Move

**Goal:** Dragging a selected tile or rectangle moves it to a new location with live preview.

Steps:
1. Implement `edit_capture_ids()` (§10.5).
2. Implement `edit_tiled_id()` (§10.6) — needed by commit and draw.
3. Complete the MOVING branch in `edit_handle_push()` (§10.12 — already written).
4. Complete the MOVING branch in `edit_handle_enter()` (§10.13).
5. Implement `edit_commit_move()` (§10.9).
6. Add MOVING case to `commit_edit_action()` (§10.11).
7. Implement the MOVING preview in `Block::draw()` (§11.5 — MOVING branch).

**Verify:**
- Select a 1×1 tile. Click-drag → source shows id 0 (blank), destination shows the tile live.
- Release → tile at new position, source blanked. Ctrl+Z restores both.
- Select a 3×2 rect. Drag → entire rect moves. Release → committed.
- Drag to the map edge → selection is clamped, no crash.

---

### Phase 6 — Anchor Handles and Resize

**Goal:** Drawn handles on selection edges allow expanding/contracting the selection with tiling.

Steps:
1. Implement `detect_anchor_at()` (§10.7).
2. Extend `draw_overlay()` with anchor handle drawing (§11.4).
3. Modify `Block::handle()` FL_MOVE to change cursor over anchors (§11.7b).
   - Add `#include <FL/Fl.H>` to `map-buttons.cpp` if not already present via the existing includes.
4. Complete RESIZE branches in `edit_handle_push()` (§10.12 — already written).
5. Complete RESIZE branches in `edit_handle_enter()` (§10.13).
6. Implement `edit_commit_resize()` (§10.10).
7. Add RESIZE cases to `commit_edit_action()` (§10.11).
8. Implement the RESIZE preview in `Block::draw()` (§11.5 — RESIZE branch).

**Verify:**
- With a selection, hover over corner handle → cursor changes to diagonal resize.
- Drag corner outward → preview shows tiled expansion live.
- Release → new larger selection with pattern tiled. Ctrl+Z undoes.
- Drag anchor inward past original boundary → cells outside new rect show id 0. Release → blanked.
- Drag edge (N/S/E/W) handle → only one dimension changes.

---

## 16. Edge Cases

| Scenario | Expected behaviour |
|----------|--------------------|
| Press Escape with no map loaded | No-op (`deselect_metatile` guards on `_map.size()`) |
| Double-click tile with `id >= metatileset size` | No-op |
| Move selection to a position that partly clips the map edge | Clamped by the clamp in `edit_handle_enter` MOVING; commit skips out-of-bounds cells |
| Resize anchor drag creates 1×1 selection | Allowed; clamps prevent zero or negative size |
| Undo while mid-drag | `_map.remember()` was called at push; undo reverts to pre-drag state. `edit_clear_selection()` in undo clears the partial drag. |
| Map is resized while a selection exists | `edit_clear_selection()` is called in `resize_map`. |
| Switch to EVENTS mode with selection | Selection cleared by `edit_clear_selection()` in mode callbacks. |
| Right-click in edit mode | `change_block_cb` returns early (only left-mouse is handled). |
| Metatile 0 as a non-blank metatile | "Blank" is always id 0; if the user has placed it intentionally, deletes still write 0. |
| Zoom toggle with selection visible | `metatile_size()` changes; anchor hit positions update automatically on next `detect_anchor_at` call because positions are computed live. Selection highlight redraws correctly since Block sizes change. |
| `_edit_drag_ids` empty when `edit_tiled_id` called | `edit_tiled_id` would divide by zero. Guard: only call `edit_tiled_id` when `_edit_drag_ids_rows > 0 && _edit_drag_ids_cols > 0`. |

---

## 17. Files Changed Summary

| File | Changes |
|------|---------|
| `src/main-window.h` | `EditAction` enum; new state fields; new method declarations; new public accessors; coordinate helpers; constants |
| `src/main-window.cpp` | New methods §10.2–10.13; modified `handle()`, `change_block_cb()`, `draw_overlay()`; lifecycle hooks in §11.8 |
| `src/map-buttons.cpp` | Modified `Block::draw()` (§11.5); modified `Block::handle()` (§11.7); add `#include <FL/Fl.H>` |
| `src/map-buttons.h` | No changes required (Block class interface unchanged) |

---

## 18. Verification Checklist

- [ ] Build with no warnings
- [ ] All existing insert-mode operations work unchanged (paint, flood fill Shift+click, substitute Ctrl+click, swap Alt+click, right-click to pick)
- [ ] Escape with no map: no-op
- [ ] Escape deselects sidebar; cursor on map no longer places tiles
- [ ] Click → 1-cell cyan selection; click elsewhere → selection replaced
- [ ] Drag → rectangular selection; rubber-band outline visible during drag
- [ ] Delete key blanks selection; Ctrl+Z undoes
- [ ] Double-click tile → enters insert mode with that tile; sidebar scrolls to it
- [ ] Drag selected tile → live ghost preview (source blank, dest shows tile); release commits
- [ ] Drag selection near map edge → clamped, no crash or blank out-of-range write
- [ ] Ctrl+Z after move restores both source and destination
- [ ] Hover over anchor handle → cursor changes
- [ ] Drag corner anchor outward → tiled expansion preview; release commits
- [ ] Drag anchor inward → shrink preview (exposed cells show id 0); release blanks them
- [ ] Ctrl+Z after resize restores
- [ ] Undo/redo clears selection
- [ ] Map resize clears selection
- [ ] Switching to EVENTS mode clears selection
- [ ] Closing and reopening map starts in insert mode (metatile 0 selected)
