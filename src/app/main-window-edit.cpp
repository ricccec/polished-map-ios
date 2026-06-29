#include <cstdlib>
#include <queue>
#include <utility>

#pragma warning(push, 0)
#include <FL/x.H>
#include <FL/Fl.H>
#include <FL/Fl_Overlay_Window.H>
#include <FL/Fl_Sys_Menu_Bar.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Toggle_Button.H>
#include <FL/Fl_Multi_Label.H>
#include <FL/Fl_Copy_Surface.H>
#include <FL/Fl_Image_Surface.H>
#pragma warning(pop)

#include "version.h"
#include "utils.h"
#include "themes.h"
#include "image.h"
#include "widgets.h"
#include "modal-dialog.h"
#include "option-dialogs.h"
#include "tileset.h"
#include "metatileset.h"
#include "preferences.h"
#include "config.h"
#include "main-window.h"
#include "colors.h"
#include "icons.h"

#ifdef _WIN32
#include "resource.h"
#endif

#ifdef __APPLE__
#include <unistd.h>
#endif

#ifdef __LINUX__
#include <unistd.h>
#include <X11/xpm.h>
#include "app-icon.xpm"
#endif

void Main_Window::flood_fill(Map &map, Block *b, uint8_t f, uint8_t t) {
	if (f == t) { return; }
	std::queue<size_t> queue;
	uint8_t w = map.width(), h = map.height();
	uint8_t row = b->row(), col = b->col();
	size_t i = row * w + col;
	queue.push(i);
	while (!queue.empty()) {
		size_t j = queue.front();
		queue.pop();
		Block *bi = map.block(j);
		if (bi->id() != f) { continue; }
		bi->id(t); // fill
		uint8_t r = bi->row(), c = bi->col();
		if (c > 0) { queue.push(j-1); } // left
		if (c < w - 1) { queue.push(j+1); } // right
		if (r > 0) { queue.push(j-w); } // up
		if (r < h - 1) { queue.push(j+w); } // down
	}
}

void Main_Window::substitute_block(Map &map, uint8_t f, uint8_t t) {
	size_t n = map.size();
	for (size_t i = 0; i < n; i++) {
		Block *b = map.block(i);
		if (b->id() == f) {
			b->id(t);
		}
	}
}

void Main_Window::swap_blocks(Map &map, uint8_t f, uint8_t t) {
	if (f == t) { return; }
	size_t n = map.size();
	for (size_t i = 0; i < n; i++) {
		Block *b = map.block(i);
		if (b->id() == f) {
			b->id(t);
		}
		else if (b->id() == t) {
			b->id(f);
		}
	}
}

void Main_Window::resize_scratch(int w, int h) {
	// Snapshot the existing grid so overlapping cells survive the resize (top-left anchored).
	uint8_t ow = _scratch_map.width(), oh = _scratch_map.height();
	std::vector<uint8_t> old(_scratch_map.size());
	for (size_t i = 0; i < _scratch_map.size(); i++) {
		old[i] = _scratch_map.block(i)->id();
	}
	_scratch_window->clear_canvas(); // delete old Block widgets before reallocating the grid
	_scratch_map.size((uint8_t)w, (uint8_t)h);
	for (uint8_t y = 0; y < (uint8_t)h; y++) {
		for (uint8_t x = 0; x < (uint8_t)w; x++) {
			uint8_t id = (x < ow && y < oh) ? old[(size_t)y * ow + x] : 0;
			_scratch_map.block(x, y, new Block(y, x, id));
		}
	}
	_scratch_window->build(this);
	_scratch_map.modified(true);
}

void Main_Window::add_sub_metatiles(size_t n) {
	size_t s = _metatileset.size();
	if (n == s) { return; }
	_metatileset.size(n);
	force_add_sub_metatiles(s, n);
}

void Main_Window::force_add_sub_metatiles(size_t s, size_t n) {
	int ms = metatile_size();

	if (n > s) {
		// add metatiles
		for (size_t i = (int)s; i < n; i++) {
			int x = ms * (i % METATILES_PER_ROW), y = ms * (i / METATILES_PER_ROW);
			Metatile_Button *mtb = new Metatile_Button(_sidebar->x() + x, _sidebar->y() - _sidebar->yposition() + y, ms, (uint8_t)i);
			mtb->callback((Fl_Callback *)select_metatile_cb, this);
			_sidebar->add(mtb);
			_metatile_buttons[i] = mtb;
		}
	}
	else if (n < s) {
		// remove metatiles
		if (_clipboard.id() >= n) {
			_copied = false;
		}
		for (auto it = _metatile_hotkeys.begin(); it != _metatile_hotkeys.end();) {
			uint8_t id = it->first;
			if (id >= n) {
				int key = it->second;
				_hotkey_metatiles.erase(key);
				_metatile_hotkeys.erase(it++);
			}
			else {
				++it;
			}
		}
		if (_selected->id() >= n) {
			_selected = _metatile_buttons[0];
			_selected->setonly();
			_sidebar->scroll_to(0, 0);
		}
		for (size_t i = n; i < s; i++) {
			_sidebar->remove((int)n);
			_metatile_buttons[i] = NULL;
		}
		int k = ms * ((int)(n - 1) / METATILES_PER_ROW + 1);
		if (_sidebar->yposition() + _sidebar->h() > k) {
			_sidebar->scroll_to(0, std::max(k - _sidebar->h(), 0));
		}
	}

	_sidebar->size(ms * METATILES_PER_ROW + Fl::scrollbar_size(), _sidebar->h());
	_sidebar->init_sizes();
	_sidebar->contents(ms * METATILES_PER_ROW, ms * (((int)_metatileset.size() + METATILES_PER_ROW - 1) / METATILES_PER_ROW));

	Tileset &tileset = _metatileset.tileset();
	_block_window->tileset(&tileset);
	_tileset_window->tileset(&tileset);
	_roof_window->tileset(&tileset);

	update_labels();
	update_status((Block *)NULL);

	// Prune trays for the new metatile count, then refresh the filtered list + layout
	size_t cnt = _metatileset.size();
	for (auto it = _recent_metatiles.begin(); it != _recent_metatiles.end();) {
		if (*it >= cnt) { it = _recent_metatiles.erase(it); } else { ++it; }
	}
	bool favs_changed = false;
	for (auto it = _favorite_metatiles.begin(); it != _favorite_metatiles.end();) {
		if (*it >= cnt) { it = _favorite_metatiles.erase(it); favs_changed = true; } else { ++it; }
	}
	if (favs_changed) { save_editmeta(); }
	rebuild_visible_metatiles();
	update_layout();

	redraw();
}

void Main_Window::resize_map(int w, int h) {
	int dw = w - _map.width(), dh = h - _map.height();

	int px, py;
	switch (_resize_dialog->horizontal_anchor()) {
	case Resize_Dialog::Hor_Align::LEFT:
		px = 0;
		break;
	case Resize_Dialog::Hor_Align::RIGHT:
		px = dw;
		break;
	case Resize_Dialog::Hor_Align::CENTER:
	default:
		px = dw / 2;
	}
	switch (_resize_dialog->vertical_anchor()) {
	case Resize_Dialog::Vert_Align::TOP:
		py = 0;
		break;
	case Resize_Dialog::Vert_Align::BOTTOM:
		py = dh;
		break;
	case Resize_Dialog::Vert_Align::MIDDLE:
	default:
		py = dh / 2;
	}

	while (_map_group->children()) {
		_map_group->remove(0);
	}
	int mx = std::max(px, 0), my = std::max(py, 0), mw = std::min(w, _map.width() + px), mh = std::min(h, _map.height() + py);
	for (int y = 0; y < py; y++) {
		for (int x = 0; x < w; x++) {
			_map_group->add(new Block());
		}
	}
	for (int y = my; y < mh; y++) {
		for (int x = 0; x < px; x++) {
			_map_group->add(new Block());
		}
		for (int x = mx; x < mw; x++) {
			_map_group->add(_map.block((uint8_t)(x - px), (uint8_t)(y - py)));
		}
		for (int x = mw; x < w; x++) {
			_map_group->add(new Block());
		}
	}
	for (int y = mh; y < h; y++) {
		for (int x = 0; x < w; x++) {
			_map_group->add(new Block());
		}
	}
	size_t n = _map_events.size();
	int sx = _map_scroll->x() - _map_scroll->xposition();
	int sy = _map_scroll->y() - _map_scroll->yposition();
	for (size_t i = 0; i < n; i++) {
		Event *e = _map_events.event(i);
		if (px || py) {
			int rx = (int)e->event_x() + px * 2;
			int ry = (int)e->event_y() + py * 2;
			int16_t ex = std::clamp((int16_t)rx, MIN_EVENT_COORD, MAX_EVENT_COORD);
			int16_t ey = std::clamp((int16_t)ry, MIN_EVENT_COORD, MAX_EVENT_COORD);
			e->coords(ex, ey);
			e->reposition(sx, sy);
			e->update_tooltip();
			_map_events.modified(true);
		}
		_map_group->add(e);
	}

	int ms = metatile_size();
	_map_group->size(ms * ((int)w + EVENT_MARGIN), ms * ((int)h + EVENT_MARGIN));
	_map.size((uint8_t)w, (uint8_t)h);
	int i = 0;
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			Block *block = (Block *)_map_group->child(i++);
			block->coords((uint8_t)y, (uint8_t)x);
			block->callback((Fl_Callback *)change_block_cb, this);
			_map.block((uint8_t)x, (uint8_t)y, block);
		}
	}
	_map.resize_blocks(_map_group->x(), _map_group->y(), ms);

	_map_scroll->scroll_to(0, 0);
	_map_scroll->init_sizes();
	_map_events.resize_events(_map_group->x(), _map_group->y(), ms / 2);
	_map_scroll->contents(_map_group->w(), _map_group->h());

	_map.modified(true);
	redraw();
}

void Main_Window::edit_metatile(Metatile *mt) {
	for (int y = 0; y < METATILE_SIZE; y++) {
		for (int x = 0; x < METATILE_SIZE; x++) {
			uint8_t id = _block_window->tile_id(x, y);
			mt->tile_id(x, y, id);
			for (int i = 0; i < NUM_QUADRANTS; i++) {
				Quadrant q = (Quadrant)i;
				const char *c = _block_window->collision(q);
				mt->collision(q, c);
				uint8_t b = _block_window->bin_collision(q);
				mt->bin_collision(q, b);
			}
		}
	}
	_metatileset.modified(true);
	redraw();
}
