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

void Main_Window::update_status(Block *b) {
	if (!_map.size()) {
		_metatile_count->label("");
		_map_dimensions->label("");
		_hover_id->label("");
		_hover_xy->label("");
		_status_event_x = _status_event_y = INT_MIN;
		_hover_event->label("");
		_status_bar->redraw();
		_hor_ruler->redraw();
		_ver_ruler->redraw();
		return;
	}
	char buffer[64] = {};
	if (!b) {
		sprintf(buffer, "Blocks: %zu", _metatileset.size());
		_metatile_count->copy_label(buffer);
		sprintf(buffer, "Map: %u x %u", _map.width(), _map.height());
		_map_dimensions->copy_label(buffer);
		_hover_id->label("");
		_hover_xy->label("");
		_status_event_x = _status_event_y = INT_MIN;
		_hover_event->label("");
		_status_bar->redraw();
		_hor_ruler->redraw();
		_ver_ruler->redraw();
		return;
	}
	uint8_t row = b->row(), col = b->col(), id = b->id();
	bool hex_ = hex();
	sprintf(buffer, (hex_ ? "ID: $%02X" : "ID: %u"), id);
	_hover_id->copy_label(buffer);
	sprintf(buffer, (hex_ ? "X/Y ($%X, $%X)" : "X/Y (%u, %u)"), col, row);
	_hover_xy->copy_label(buffer);
	update_event_cursor(b);
}

void Main_Window::update_event_cursor(Block *b) {
	if (b) {
		_status_event_x = (int)b->col() * 2 + b->right_half();
		_status_event_y = (int)b->row() * 2 + b->bottom_half();
	}
	else {
		_status_event_x = _status_event_y = INT_MIN;
	}
	if (_mode == Mode::EVENTS && b) {
		char buffer[64] = {};
		sprintf(buffer, (hex() ? "Event: X/Y ($%X, $%X)" : "Event: X/Y (%u, %u)"), _status_event_x, _status_event_y);
		_hover_event->copy_label(buffer);
	}
	else {
		_hover_event->label("");
	}
	_status_bar->redraw();
	_hor_ruler->redraw();
	_ver_ruler->redraw();
}

void Main_Window::update_gameboy_screen(Block *b) {
	if (_mode == Mode::EVENTS && b && gameboy_screen()) {
		int hx = b->x() + b->right_half() * b->w() / 2, hy = b->y() + b->bottom_half() * b->h() / 2;
		int hs = metatile_size() / 2;
		Game_Boy_Screen::resize(hx-hs*4, hy-hs*4, hs*10, hs*9);
		_gameboy_screen = true;
		redraw_overlay();
	}
	else if (_gameboy_screen) {
		_gameboy_screen = false;
		redraw_map();
	}
	else if (b) {
		b->redraw();
	}
}

void Main_Window::update_active_controls() {
	update_priority_controls();
	if (_map.size()) {
		_load_event_script_mi->activate();
		_load_event_script_tb->activate();
		_close_mi->activate();
		_save_mi->activate();
		_save_tb->activate();
		_save_as_mi->activate();
		_save_map_mi->activate();
		_save_blockset_mi->activate();
		_save_tileset_mi->activate();
		_print_mi->activate();
		_print_tb->activate();
		if (_map_events.loaded()) {
			_view_event_script_mi->activate();
			_unload_event_script_mi->activate();
			_reload_event_script_mi->activate();
			_save_event_script_mi->activate();
			_reload_event_script_tb->activate();
		}
		else {
			_view_event_script_mi->deactivate();
			_unload_event_script_mi->deactivate();
			_reload_event_script_mi->deactivate();
			_save_event_script_mi->deactivate();
			_reload_event_script_tb->deactivate();
		}
		if (_map.group()) {
			_load_roof_colors_mi->activate();
		}
		else {
			_load_roof_colors_mi->deactivate();
		}
		if (_map.can_undo()) {
			_undo_mi->activate();
			_undo_tb->activate();
		}
		else {
			_undo_mi->deactivate();
			_undo_tb->deactivate();
		}
		if (_map.can_redo()) {
			_redo_mi->activate();
			_redo_tb->activate();
		}
		else {
			_redo_mi->deactivate();
			_redo_tb->deactivate();
		}
		_copy_block_mi->activate();
		if (_copied && _selected) {
			_paste_block_mi->activate();
			_swap_block_mi->activate();
		}
		else {
			_paste_block_mi->deactivate();
			_swap_block_mi->deactivate();
		}
		_resize_blockset_mi->activate();
		_add_sub_tb->activate();
		_resize_map_mi->activate();
		_resize_tb->activate();
		_change_tileset_mi->activate();
		_change_tileset_tb->activate();
		_edit_tileset_mi->activate();
		_edit_tileset_tb->activate();
		if (_map_options_dialog->num_roofs() > 0) {
			_change_roof_mi->activate();
			_change_roof_tb->activate();
		}
		else {
			_change_roof_mi->deactivate();
			_change_roof_tb->deactivate();
		}
		if (_metatileset.const_tileset().num_roof_tiles() > 0) {
			_save_roof_mi->activate();
			_edit_roof_mi->activate();
			_edit_roof_tb->activate();
		}
		else {
			_save_roof_mi->deactivate();
			_edit_roof_mi->deactivate();
			_edit_roof_tb->deactivate();
		}
	}
	else {
		_load_event_script_mi->deactivate();
		_load_event_script_tb->deactivate();
		_load_roof_colors_mi->deactivate();
		_reload_event_script_mi->deactivate();
		_reload_event_script_tb->deactivate();
		_close_mi->deactivate();
		_view_event_script_mi->deactivate();
		_unload_event_script_mi->deactivate();
		_save_mi->deactivate();
		_save_tb->deactivate();
		_save_as_mi->deactivate();
		_save_map_mi->deactivate();
		_save_blockset_mi->deactivate();
		_save_tileset_mi->deactivate();
		_save_roof_mi->deactivate();
		_save_event_script_mi->deactivate();
		_print_mi->deactivate();
		_print_tb->deactivate();
		_undo_mi->deactivate();
		_undo_tb->deactivate();
		_redo_mi->deactivate();
		_redo_tb->deactivate();
		_copy_block_mi->deactivate();
		_paste_block_mi->deactivate();
		_swap_block_mi->deactivate();
		_resize_blockset_mi->deactivate();
		_add_sub_tb->deactivate();
		_resize_map_mi->deactivate();
		_resize_tb->deactivate();
		_change_tileset_mi->deactivate();
		_edit_tileset_mi->deactivate();
		_change_tileset_tb->deactivate();
		_edit_tileset_tb->deactivate();
		_change_roof_mi->deactivate();
		_change_roof_tb->deactivate();
		_edit_roof_mi->deactivate();
		_edit_roof_tb->deactivate();
	}
}

void Main_Window::update_priority_controls() {
	if (Config::allow_priority()) {
		_allow_priority_mi->set();
		_show_priority_mi->activate();
		_show_priority_tb->activate();
	}
	else {
		_allow_priority_mi->clear();
		_show_priority_mi->deactivate();
		_show_priority_tb->deactivate();
	}
}

void Main_Window::update_monochrome_controls() {
	// keep the Crystal/Monochrome/Prism radio group in sync with Config
	// (monochrome may have been forced on if the palette map could not be read)
	if (Config::monochrome()) {
		_monochrome_mi->setonly();
	}
	else if (Config::prism()) {
		_prism_mi->setonly();
	}
	else {
		_crystal_palettes_mi->setonly();
	}
}

void Main_Window::update_icons() {
	bool dark = OS::is_dark_theme(OS::current_theme());
	_grid_tb->image(dark ? GRID_DARK_ICON : GRID_ICON);
	_ids_tb->image(dark ? IDS_DARK_ICON : IDS_ICON);
	_hex_tb->image(dark ? HEX_DARK_ICON : HEX_ICON);
	_blocks_mode_tb->image(dark ? BLOCKS_DARK_ICON : BLOCKS_ICON);
	Image::make_deimage(_new_tb);
	Image::make_deimage(_open_tb);
	Image::make_deimage(_load_event_script_tb);
	Image::make_deimage(_reload_event_script_tb);
	Image::make_deimage(_save_tb);
	Image::make_deimage(_print_tb);
	Image::make_deimage(_undo_tb);
	Image::make_deimage(_redo_tb);
	Image::make_deimage(_grid_tb);
	Image::make_deimage(_rulers_tb);
	Image::make_deimage(_zoom_tb);
	Image::make_deimage(_ids_tb);
	Image::make_deimage(_hex_tb);
	Image::make_deimage(_show_priority_tb);
	Image::make_deimage(_gameboy_screen_tb);
	Image::make_deimage(_show_events_tb);
	Image::make_deimage(_show_warp_ids_tb);
	Image::make_deimage(_blocks_mode_tb);
	Image::make_deimage(_events_mode_tb);
	Image::make_deimage(_add_sub_tb);
	Image::make_deimage(_resize_tb);
	Image::make_deimage(_change_tileset_tb);
	Image::make_deimage(_edit_tileset_tb);
	Image::make_deimage(_change_roof_tb);
	Image::make_deimage(_edit_roof_tb);
	Image::make_deimage(_load_palettes_tb);
	Image::make_deimage(_edit_current_palettes_tb);
	_block_window->update_icons();
	_tileset_window->update_icons();
	_roof_window->update_icons();
}

void Main_Window::update_rulers() {
	if (rulers()) {
		_hor_ruler->show();
		_ver_ruler->show();
		_corner_ruler->show();
	}
	else {
		_hor_ruler->hide();
		_ver_ruler->hide();
		_corner_ruler->hide();
	}
	update_layout();
}

void Main_Window::update_zoom() {
	int sx = _map_scroll->xposition(), sy = _map_scroll->yposition();
	if (zoom()) {
		_map_scroll->scroll_to(sx * 2, sy * 2);
	}
	else {
		_map_scroll->scroll_to(sx / 2, sy / 2);
	}
	update_layout();
}

void Main_Window::update_layout() {
	init_sizes();
	int ms = metatile_size();
	size_t n = _metatileset.size();
	int rs = Fl::scrollbar_size();
	_sidebar->size(ms * METATILES_PER_ROW + rs, _sidebar->h());
	int ox = _sidebar->w() + (rulers() ? rs : 0);
	int oy = rulers() ? rs : 0;
	_hor_ruler->resize(ox, _sidebar->y(), w() - ox, rs);
	_ver_ruler->resize(_sidebar->w(), _sidebar->y() + oy, rs, _sidebar->h() - oy);
	_corner_ruler->resize(_sidebar->w(), _sidebar->y(), rs, rs);
	_map_scroll->resize(ox, _sidebar->y() + oy, w() - ox, _sidebar->h() - oy);
	int gw = ((int)_map.width() + EVENT_MARGIN) * ms, gh = ((int)_map.height() + EVENT_MARGIN) * ms;
	_map_group->resize(ox - _map_scroll->xposition(), _sidebar->y() + oy - _map_scroll->yposition(), gw, gh);
	_sidebar->contents(ms * METATILES_PER_ROW, ms * (((int)n + METATILES_PER_ROW - 1) / METATILES_PER_ROW));
	_map_scroll->contents(_map_group->w(), _map_group->h());
	int sx = _sidebar->x(), sy = _sidebar->y();
	for (size_t i = 0; i < n; i++) {
		Metatile_Button *mt = _metatile_buttons[i];
		int dx = ms * (i % METATILES_PER_ROW), dy = ms * (i / METATILES_PER_ROW);
		mt->resize(sx + dx, sy + dy, ms + 1, ms + 1);
	}
	int mx = _map_group->x(), my = _map_group->y();
	_map.resize_blocks(mx, my, ms);
	_map_events.resize_events(mx, my, ms / 2);
}

void Main_Window::update_labels() {
	size_t n = _metatileset.size();
	for (size_t i = 0; i < n; i++) {
		_metatile_buttons[i]->id(_metatile_buttons[i]->id());
	}
	n = _map.size();
	for (size_t i = 0; i < n; i++) {
		_map.block(i)->update_label();
	}
	redraw();
}

void Main_Window::update_palettes() {
	Tileset &tileset = _metatileset.tileset();
	tileset.update_palettes(palettes());
	if (Config::prism()) {
		_metatileset.resolve_prism_palettes(_map.environment(), palettes());
	}
	redraw();
}

void Main_Window::select_metatile(Metatile_Button *mb) {
	_selected = mb;
	_selected->setonly();
	uint8_t id = mb->id();
	int ms = metatile_size();
	if (ms * (id / METATILES_PER_ROW) >= _sidebar->yposition() + _sidebar->h() - ms / 2) {
		_sidebar->scroll_to(0, ms * (id / METATILES_PER_ROW + 1) - _sidebar->h());
		_sidebar->redraw();
	}
	else if (ms * (id / METATILES_PER_ROW + 1) <= _sidebar->yposition() + ms / 2) {
		_sidebar->scroll_to(0, ms * (id / METATILES_PER_ROW));
		_sidebar->redraw();
	}
}
