#include <cstdlib>
#include <cstring>
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

void Main_Window::store_recent_map() {
	std::string last(_blk_file);
	for (int i = 0; i < NUM_RECENT; i++) {
		if (_recent[i] == _blk_file) {
			_recent[i] = last;
			break;
		}
		std::swap(last, _recent[i]);
	}
	update_recent_maps();
}

void Main_Window::update_recent_maps() {
	int last = -1;
	for (int i = 0; i < NUM_RECENT; i++) {
#ifndef __APPLE__
		Fl_Multi_Label *ml = (Fl_Multi_Label *)_recent_mis[i]->label();
		if (ml->labelb[0]) {
			delete [] ml->labelb;
			ml->labelb = "";
		}
#endif
		if (_recent[i].empty()) {
#ifdef __APPLE__
			_recent_mis[i]->label("");
#endif
			_recent_mis[i]->hide();
		}
		else {
			const char *basename = fl_filename_name(_recent[i].c_str());
#ifndef __APPLE__
			char *label = new char[FL_PATH_MAX]();
			strcpy(label, OS_MENU_ITEM_PREFIX);
			strcat(label, basename);
			strcat(label, OS_MENU_ITEM_SUFFIX);
			ml->labelb = label;
#else
			_recent_mis[i]->label(basename);
#endif
			_recent_mis[i]->show();
			last = i;
		}
		_recent_mis[i]->flags &= ~FL_MENU_DIVIDER;
	}
	if (last > -1) {
		_recent_mis[last]->flags |= FL_MENU_DIVIDER;
	}
}

void Main_Window::save_editmeta() {
	if (_directory.empty() || !_metatileset.size()) { return; }
	char filename[FL_PATH_MAX] = {};
	Config::editmeta_path(filename, _directory.c_str(), _metatileset.tileset().name());
	FILE *file = fl_fopen(filename, "wb");
	if (!file) { return; }
	for (uint8_t id : _favorite_metatiles) {
		fprintf(file, "favorite %u\n", (unsigned)id);
	}
	fclose(file);
}

void Main_Window::load_editmeta() {
	_favorite_metatiles.clear();
	if (_directory.empty()) { return; }
	char filename[FL_PATH_MAX] = {};
	Config::editmeta_path(filename, _directory.c_str(), _metatileset.tileset().name());
	FILE *file = fl_fopen(filename, "rb");
	if (!file) { return; }
	char tag[32];
	unsigned id;
	while (fscanf(file, "%31s %u", tag, &id) == 2) {
		if (strcmp(tag, "favorite") == 0 && id < _metatileset.size() && !is_favorite((uint8_t)id)) {
			_favorite_metatiles.push_back((uint8_t)id);
		}
	}
	fclose(file);
}

// Builds _scratch_map (data + Block widgets) from the tileset's scratch sidecar, or a default
// empty grid if none exists. The Blocks are added to the scratch window's group by build().
void Main_Window::load_scratch() {
	int w = SCRATCH_DEFAULT_W, h = SCRATCH_DEFAULT_H;
	std::vector<uint8_t> cells;
	if (!_directory.empty()) {
		char filename[FL_PATH_MAX] = {};
		Config::scratch_path(filename, _directory.c_str(), _metatileset.tileset().name());
		FILE *file = fl_fopen(filename, "rb");
		if (file) {
			unsigned fw = 0, fh = 0;
			if (fscanf(file, "%u %u", &fw, &fh) == 2 && fw > 0 && fh > 0 && fw <= 255 && fh <= 255) {
				w = (int)fw;
				h = (int)fh;
				cells.resize((size_t)w * h, 0);
				for (size_t i = 0; i < cells.size(); i++) {
					unsigned id = 0;
					if (fscanf(file, "%u", &id) != 1) { break; }
					cells[i] = (uint8_t)id;
				}
			}
			fclose(file);
		}
	}
	_scratch_map.size((uint8_t)w, (uint8_t)h);
	for (uint8_t y = 0; y < (uint8_t)h; y++) {
		for (uint8_t x = 0; x < (uint8_t)w; x++) {
			uint8_t id = cells.empty() ? 0 : cells[(size_t)y * w + x];
			_scratch_map.block(x, y, new Block(y, x, id));
		}
	}
}

void Main_Window::save_scratch() {
	if (_directory.empty() || !_scratch_map.size()) { return; }
	char filename[FL_PATH_MAX] = {};
	Config::scratch_path(filename, _directory.c_str(), _metatileset.tileset().name());
	FILE *file = fl_fopen(filename, "wb");
	if (!file) { return; }
	uint8_t w = _scratch_map.width(), h = _scratch_map.height();
	fprintf(file, "%u %u\n", (unsigned)w, (unsigned)h);
	for (uint8_t y = 0; y < h; y++) {
		for (uint8_t x = 0; x < w; x++) {
			fprintf(file, "%u ", (unsigned)_scratch_map.block(x, y)->id());
		}
		fprintf(file, "\n");
	}
	fclose(file);
}

void Main_Window::open_map(const char *filename) {
	const char *basename = fl_filename_name(filename);

	char directory[FL_PATH_MAX] = {};
	if (!Config::project_path_from_blk_path(filename, directory)) {
		std::string msg = "Could not find the project directory for\n";
		msg = msg + basename + "!\nMake sure it contains a Makefile.";
		_error_dialog->message(msg);
		_error_dialog->show(this);
		return;
	}

	open_map(directory, filename);
}

void Main_Window::open_map(const char *directory, const char *filename) {
	// get map options
	Map_Attributes attrs;
	printf("limit_blk_options(%s, %s, %p)\n", filename, directory, &attrs);
	if (!_map_options_dialog->limit_blk_options(filename, directory, attrs)) {
		std::string msg = "This is not a valid project!\n\n"
			"Make sure the Options are correct.";
		_error_dialog->message(msg);
		_error_dialog->show(this);
		return;
	}

	_map_options_dialog->show(this);
	bool canceled = _map_options_dialog->canceled();
	if (canceled) { return; }

	if (!_map_options_dialog->map_width() || !_map_options_dialog->map_height()) {
		std::string msg = "Dimensions must be nonzero!";
		_error_dialog->message(msg);
		_error_dialog->show(this);
		return;
	}

	_map.modified(false);
	_map_events.modified(false);
	_metatileset.modified(false);
	close_cb(NULL, this);

	_directory = directory;
	if (filename) {
		_blk_file = filename;
	}
	else {
		_blk_file = "";
		_blk_save_chooser->directory(directory);
	}
	_asm_file = "";

	// read data
	const char *tileset_name = _map_options_dialog->tileset();
	const char *roof_name = _map_options_dialog->roof();
	if (!read_metatile_data(tileset_name, roof_name)) { return; }

	uint8_t w = _map_options_dialog->map_width(), h = _map_options_dialog->map_height();
	_map.size(w, h);
	int ms = metatile_size();

	_map.attributes(attrs);

	const char *basename;

	// populate map with blocks
	if (filename) {
		basename = fl_filename_name(_blk_file.c_str());
		Map::Result r = _map.read_blocks(filename);
		if (r == Map::Result::MAP_TOO_LONG) {
			std::string msg = "Warning: ";
			msg = msg + basename + ":\n\n" + Map::error_message(r);
			_warning_dialog->message(msg);
			_warning_dialog->show(this);
		}
		else if (r != Map::Result::MAP_OK) {
			_map.clear();
			_metatileset.clear();
			std::string msg = "Error reading ";
			msg = msg + basename + "!\n\n" + Map::error_message(r);
			_error_dialog->message(msg);
			_error_dialog->show(this);
			return;
		}
	}
	else {
		basename = NEW_MAP_NAME;
		_map.modified(true);
		for (uint8_t row = 0; row < h; row++) {
			for (uint8_t col = 0; col < w; col++) {
				_map.block(col, row, new Block(row, col, 0x00));
			}
		}
	}

	_map_group->size(ms * ((int)w + EVENT_MARGIN), ms * ((int)h + EVENT_MARGIN));
	for (uint8_t y = 0; y < h; y++) {
		for (uint8_t x = 0; x < w; x++) {
			Block *block = _map.block(x, y);
			block->callback((Fl_Callback *)change_block_cb, this);
			_map_group->add(block);
		}
	}
	_map.resize_blocks(_map_group->x(), _map_group->y(), ms);
	_map_scroll->init_sizes();
	int kx = _map_group->w() - _map_scroll->w() + Fl::scrollbar_size();
	int ky = _map_group->h() - _map_scroll->h() + Fl::scrollbar_size();
	int tx = kx > MAP_MARGIN * ms ? MAP_MARGIN * ms : kx < 0 ? 0 : kx / 2;
	int ty = ky > MAP_MARGIN * ms ? MAP_MARGIN * ms : ky < 0 ? 0 : ky / 2;
	_map_scroll->scroll_to(tx, ty);
	_map_scroll->contents(_map_group->w(), _map_group->h());

	// set filenames
	char buffer[FL_PATH_MAX] = {};
	sprintf(buffer, PROGRAM_NAME " - %s", basename);
	copy_label(buffer);
	if (ends_with_ignore_case(basename, ".blk")) {
		strcpy(buffer, basename);
		size_t n = strlen(buffer);
		buffer[n - strlen(".blk")] = '\0';
		strcat(buffer, ".png");
	}
	else {
		sprintf(buffer, "%s.png", basename);
	}
	_png_chooser->preset_file(buffer);

	// populate sidebar with metatile buttons
	_sidebar->scroll_to(0, 0);
	size_t n = _metatileset.size();
	for (size_t i = 0; i < n; i++) {
		int x = ms * (i % METATILES_PER_ROW), y = ms * (i / METATILES_PER_ROW);
		Metatile_Button *mtb = new Metatile_Button(_sidebar->x() + x, _sidebar->y() + y, ms, (uint8_t)i);
		mtb->callback((Fl_Callback *)select_metatile_cb, this);
		_sidebar->add(mtb);
		_metatile_buttons[i] = mtb;
	}
	_sidebar->init_sizes();
	_sidebar->contents(ms * METATILES_PER_ROW, ms * (((int)n + METATILES_PER_ROW - 1) / METATILES_PER_ROW));

	if (n) {
		_metatile_buttons[0]->setonly();
		_selected = _metatile_buttons[0];
	}
	_copied = false;

	// Metatile picker: reset to the All tab, clear filter/recents, and load this tileset's favorites
	_picker_tab = Picker_Tab::ALL;
	_tab_all->setonly();
	_metatile_filter->value("");
	_recent_metatiles.clear();
	load_editmeta();
	rebuild_visible_metatiles();
	update_layout();

	Tileset &tileset = _metatileset.tileset();
	_block_window->tileset(&tileset);
	_tileset_window->tileset(&tileset);
	_roof_window->tileset(&tileset);

	// load default palettes
	Config::bg_tiles_pal_path(buffer, directory);
	load_palettes(buffer);

	// load special palettes if applicable and they exist
	if (auto_load_special_palettes()) {
		Config::special_pal_path(buffer, directory, filename, _map.landmark().c_str(), tileset_name);
		if (file_exists(buffer)) {
			load_palettes(buffer);
		}
	}

	// use palettes coresponding to palette
	Palettes new_palettes = palettes();
	if (new_palettes != Palettes::CUSTOM) {
		if (_map.palette() == "PALETTE_NITE" || _map.palette() == "PALETTE_DARK" ||
			_map.palette() == "2" || _map.palette() == "4") {
			new_palettes = Palettes::NITE;
		}
		else if (_map.environment() == "INDOOR" || _map.environment() == "GATE" ||
			_map.environment() == "3" || _map.environment() == "6") {
			new_palettes = Palettes::INDOOR;
		}
		else if (_map.palette() == "PALETTE_MORN" || _map.palette() == "3") {
			new_palettes = Palettes::MORN;
		}
		else if (OS::is_dark_theme(OS::current_theme()) && !Config::monochrome()) {
			new_palettes = Palettes::NITE;
		}
		else {
			new_palettes = Palettes::DAY;
		}
	}
	if (new_palettes != palettes()) {
		_palettes->value((int)new_palettes);
		palettes_cb(NULL, this);
		update_palettes();
	}
	else if (Config::prism()) {
		// resolve Prism palettes now that the map's environment is known
		_metatileset.resolve_prism_palettes(_map.environment(), palettes());
	}

	// load roof colors if applicable
	if (auto_load_roof_colors() && _map.group() && _map.is_outside() && !Config::monochrome()) {
		load_roof_colors(true);
	}

	// load map events if applicable and they exist
	if (auto_load_events() && filename) {
		char map_name[FL_PATH_MAX] = {};
		remove_dot_ext(filename, map_name);
		Config::event_script_path(buffer, directory, map_name);
		if (!file_exists(buffer)) {
			strcpy(buffer, filename);
			fl_filename_setext(buffer, ".asm");
		}
		if (file_exists(buffer)) {
			load_events(buffer);
		}
	}

	update_active_controls();
	update_labels();
	update_status((Block *)NULL);

	if (filename) {
		store_recent_map();
	}

	redraw();
}

void Main_Window::open_recent(int n) {
	if (n < 0 || n >= NUM_RECENT || _recent[n].empty()) {
		return;
	}

	if (unsaved()) {
		std::string msg = modified_filename();
		msg = msg + " has unsaved changes!\n\n"
			"Open another map anyway?";
		_unsaved_dialog->message(msg);
		_unsaved_dialog->show(this);
		if (_unsaved_dialog->canceled()) { return; }
	}

	const char *filename = _recent[n].c_str();
	open_map(filename);
}

void Main_Window::warp_to_map(Event *e) {
	std::string destination = e->warp_destination().first;
	if (destination.empty()) { return; }

	char filename[FL_PATH_MAX] = {};
	strcpy(filename, _blk_file.c_str());
	strcpy(const_cast<char *>(fl_filename_name(filename)), destination.c_str());
	strcat(filename, fl_filename_ext(_blk_file.c_str()));

	const char *basename = fl_filename_name(filename);
	if (!strcmp(filename, _blk_file.c_str())) {
		std::string msg = basename;
		msg = msg + " is already open!";
		_error_dialog->message(msg);
		_error_dialog->show(this);
	}
	else if (file_exists(filename)) {
		if (unsaved()) {
			std::string msg = modified_filename();
			msg = msg + " has unsaved changes!\n\n"
				"Warp to " + basename + " anyway?";
			_unsaved_dialog->message(msg);
			_unsaved_dialog->show(this);
			if (_unsaved_dialog->canceled()) { return; }
		}
		open_map(filename);
	}
	else {
		std::string msg = "Could not warp to ";
		msg = msg + basename + "!";
		_error_dialog->message(msg);
		_error_dialog->show(this);
	}
}

void Main_Window::load_events(const char *filename) {
	Map_Events::Result et = _map_events.read_events(filename);
	if (et != Map_Events::Result::MAP_EVENTS_OK) {
		const char *basename = fl_filename_name(filename);
		std::string msg = "Error reading ";
		msg = msg + basename + "!\n\n" + Map_Events::error_message(et);
		_error_dialog->message(msg);
		_error_dialog->show(this);
		return;
	}

	_map_events.resize_events(_map_group->x(), _map_group->y(), metatile_size() / 2);
	size_t n = _map_events.size();
	for (size_t i = 0; i < n; i++) {
		Event *event = _map_events.event(i);
		event->callback((Fl_Callback *)change_event_cb, this);
		_map_group->add(event);
	}

	_asm_file = filename;
}

void Main_Window::unload_events() {
	size_t n = _map_events.size();
	for (size_t i = 0; i < n; i++) {
		Event *event = _map_events.event(i);
		_map_group->remove(event);
		delete event;
	}
	_map_events.clear();
	_asm_file = "";
}

void Main_Window::view_event_script(Event *e) {
#ifdef _WIN32
	HWND hwnd = fl_xid(this);
	wchar_t filename[FL_PATH_MAX] = {};
	fl_utf8towc(_asm_file.c_str(), _asm_file.length(), filename, FL_PATH_MAX);

	if (e) {
		wchar_t directory[FL_PATH_MAX] = {}, program[FL_PATH_MAX] = {};
		GetCurrentDirectory(FL_PATH_MAX, directory);
		FindExecutable(filename, directory, program);

		std::wstringstream wss;
		// Notepad2 or Notepad3: /g <#> <filename>
		if (ends_with_ignore_case(program, L"notepad2.exe") || ends_with_ignore_case(program, L"notepad3.exe")) {
			wss << L"/g " << e->line() << L" \"" << filename << L"\"";
		}
		// Notepad++: -n<#> <filename>
		else if (ends_with_ignore_case(program, L"notepad++.exe")) {
			wss << L"-n" << e->line() << L" \"" << filename << L"\"";
		}
		// VS Code: -g <filename>:<#>
		else if (ends_with_ignore_case(program, L"code.exe")) {
			wss << L"-g \"" << filename << L"\":" << e->line();
		}
		// Sublime Text: <filename>:<#>
		else if (ends_with_ignore_case(program, L"subl.exe") || ends_with_ignore_case(program, L"sublime_text.exe")) {
			wss << L"\"" << filename << L"\":" << e->line();
		}

		std::wstring params = wss.str();
		if (!params.empty()) {
			ShellExecute(hwnd, L"open", program, params.c_str(), NULL, SW_SHOW);
			return;
		}
	}

	ShellExecute(hwnd, L"edit", filename, NULL, NULL, SW_SHOW);
#elif defined(__APPLE__)
	if (fork() == 0) {
		execl("/usr/bin/open", "open", _asm_file.c_str(), NULL);
		exit(EXIT_SUCCESS);
	}
#else
	if (fork() == 0) {
		execl("/usr/bin/xdg-open", "xdg-open", _asm_file.c_str(), NULL);
		exit(EXIT_SUCCESS);
	}
#endif
}

void Main_Window::load_palettes(const char *filename) {
	if (_edited_palettes) {
		const char *basename = fl_filename_name(filename);
		std::string msg = "The palettes have been edited!\n\n";
		msg = msg + "Load " + basename + " anyway?";
		_unsaved_dialog->message(msg);
		_unsaved_dialog->show(this);
		if (_unsaved_dialog->canceled()) { return; }
	}

	Palettes new_palettes = Color::read_palettes(filename, palettes());
	_edited_palettes = false;
	if (new_palettes != palettes()) {
		_palettes->value((int)new_palettes);
		palettes_cb(NULL, this);
	}
	update_palettes();
}

void Main_Window::load_roof_colors(bool quiet) {
	char buffer[FL_PATH_MAX] = {};
	Config::roofs_pal_path(buffer, _directory.c_str());

	if (_edited_palettes) {
		const char *basename = fl_filename_name(buffer);
		std::string msg = "The palettes have been edited!\n\n";
		msg = msg + "Load " + basename + " anyway?";
		_unsaved_dialog->message(msg);
		_unsaved_dialog->show(this);
		if (_unsaved_dialog->canceled()) { return; }
	}

	if (!Color::read_roof_colors(buffer, _map.group(), _roof_palettes)) {
		Config::roofs_pal_path(buffer, "");
		if (quiet) {
			std::string msg = "Warning: Could not read ";
			msg = msg + buffer + "!";
			_warning_dialog->message(msg);
			_warning_dialog->show(this);
		}
		else {
			std::string msg = "Could not read ";
			msg = msg + buffer + "!";
			_error_dialog->message(msg);
			_error_dialog->show(this);
		}
	}
	else {
		update_palettes();
		if (!quiet) {
			std::string msg = "Loaded roof colors for map group ";
			msg = msg + std::to_string(_map.group()) + "!";
			_success_dialog->message(msg);
			_success_dialog->show(this);
		}
	}
}

bool Main_Window::read_metatile_data(const char *tileset_name, const char *roof_name) {
	char buffer[FL_PATH_MAX] = {};

	Tileset &tileset = _metatileset.tileset();
	tileset.name(tileset_name);
	tileset.roof_name(roof_name);

	const char *directory = _directory.c_str();

	Config::palette_map_path(buffer, directory, tileset_name);
	Palette_Map::Result rp = tileset.read_palette_map(buffer);
	// 'monochrome' becomes true if the palette map could not be read
	update_monochrome_controls();
	// 'allow_priority' become true if a PRIORITY_* color was used
	if (Config::allow_priority() && !_allow_priority_mi->value()) {
		update_priority_controls();
		redraw();
	}
	if (rp == Palette_Map::Result::PALETTE_TOO_LONG) {
		Config::palette_map_path(buffer, "", tileset_name);
		std::string msg = "Warning: ";
		msg = msg + buffer + ":\n\n" + Palette_Map::error_message(rp);
		_warning_dialog->message(msg);
		_warning_dialog->show(this);
	}
	else if (rp != Palette_Map::Result::PALETTE_OK) {
		Config::palette_map_path(buffer, "", tileset_name);
		std::string msg = "Error reading ";
		msg = msg + buffer + "!\n\n" + Palette_Map::error_message(rp);
		_error_dialog->message(msg);
		_error_dialog->show(this);
		return false;
	}

	Config::tileset_path(buffer, directory, tileset_name);
	char b_buffer[FL_PATH_MAX] = {}, a_buffer[FL_PATH_MAX] = {};
	bool has_before = Config::tileset_before_path(b_buffer, directory, tileset_name);
	bool has_after = Config::tileset_after_path(a_buffer, directory, tileset_name);
	Tileset::Result rt = tileset.read_graphics(buffer, has_before ? b_buffer : NULL, has_after ? a_buffer : NULL, palettes());
	if (rt != Tileset::Result::GFX_OK) {
		Config::tileset_path(buffer, "", tileset_name);
		std::string msg = "Error reading ";
		msg = msg + buffer + "!\n\n" + Tileset::error_message(rt);
		_error_dialog->message(msg);
		_error_dialog->show(this);
		return false;
	}

	Config::metatileset_path(buffer, directory, tileset_name);
	Metatileset::Result rm = _metatileset.read_metatiles(buffer);
	if (rm == Metatileset::Result::META_TOO_SHORT || rm == Metatileset::Result::META_TOO_LONG) {
		Config::metatileset_path(buffer, "", tileset_name);
		std::string msg = "Warning: ";
		msg = msg + buffer + ":\n\n" + Metatileset::error_message(rm);
		_warning_dialog->message(msg);
		_warning_dialog->show(this);
	}
	else if (rm != Metatileset::Result::META_OK) {
		_metatileset.clear();
		Config::metatileset_path(buffer, "", tileset_name);
		std::string msg = "Error reading ";
		msg = msg + buffer + "!\n\n" + Metatileset::error_message(rm);
		_error_dialog->message(msg);
		_error_dialog->show(this);
		return false;
	}

	if (Config::prism()) {
		// Prism: per-tile palette/flip attributes (binary) + master bg.pal palette set
		Config::attributes_path(buffer, directory, tileset_name);
		Metatileset::Result ra = _metatileset.read_attributes(buffer);
		if (ra != Metatileset::Result::META_OK) {
			Config::attributes_path(buffer, "", tileset_name);
			std::string msg = "Warning: ";
			msg = msg + buffer + ":\n\n" + Metatileset::error_message(ra);
			_warning_dialog->message(msg);
			_warning_dialog->show(this);
		}
		char pal_buffer[FL_PATH_MAX] = {};
		Config::bg_tiles_pal_path(pal_buffer, directory);
		if (!_metatileset.load_prism_palettes(pal_buffer)) {
			Config::bg_tiles_pal_path(pal_buffer, "");
			std::string msg = "Warning: could not read ";
			msg = msg + pal_buffer + " for Prism palettes.";
			_warning_dialog->message(msg);
			_warning_dialog->show(this);
		}
	}

	bool bin_collisions = Config::collisions_path(buffer, directory, tileset_name);
	_metatileset.bin_collisions(bin_collisions);
	rm = _metatileset.read_collisions(buffer);
	_has_collisions = (rm == Metatileset::Result::META_OK);

	if (tileset.has_roof()) {
		Config::roof_path(buffer, directory, roof_name);
		rt = tileset.read_roof_graphics(buffer);
		if (rt != Tileset::Result::GFX_OK) {
			Config::roof_path(buffer, "", roof_name);
			std::string msg = "Error reading ";
			msg = msg + buffer + "!\n\n" + Tileset::error_message(rt);
			_warning_dialog->message(msg);
			_warning_dialog->show(this);
		}
	}

	return true;
}

bool Main_Window::save_map(bool force) {
	const char *filename = _blk_file.c_str();
	const char *basename = fl_filename_name(filename);

	if (_map.modified() && _map.other_modified(filename)) {
		std::string msg = basename;
		msg = msg + " was modified by another program!\n\n"
			"Save the map and overwrite it anyway?";
		_unsaved_dialog->message(msg);
		_unsaved_dialog->show(this);
		if (_unsaved_dialog->canceled()) { return true; }
	}

	if (_map.modified() || force) {
		if (!_map.write_blocks(filename)) {
			std::string msg = "Could not write to ";
			msg = msg + basename + "!";
			_error_dialog->message(msg);
			_error_dialog->show(this);
			return false;
		}

		size_t w = _map.width(), h = _map.height();
		size_t b = (w + MAP_MARGIN * 2) * (h + MAP_MARGIN * 2);
		size_t s = Config::overworld_map_size();
		if (b > s) {
			std::ostringstream ss;
			ss << "A " << w << " x " << h << " map will overflow a "
				<< s << "-byte buffer!\n\n"
				<< "Make sure the overworld map block buffer in WRAM\n"
				"(wOverworldMapBlocks, wOverworldMap, or OverworldMap)\n"
				"has at least " << b << " bytes.";
			_warning_dialog->message(ss.str());
			_warning_dialog->show(this);
		}

		_map.modified(false);
	}

	if (force) {
		store_recent_map();
	}

	std::string msg = "Saved ";
	msg = msg + basename + "!";
	_success_dialog->message(msg);
	_success_dialog->show(this);

	return true;
}

bool Main_Window::save_metatileset() {
	const char *directory = _directory.c_str();
	const char *tileset_name = _metatileset.tileset().name();

	char filename[FL_PATH_MAX] = {};
	Config::metatileset_path(filename, directory, tileset_name);
	const char *basename = fl_filename_name(filename);

	char filename_coll[FL_PATH_MAX] = {};
	Config::collisions_path(filename_coll, directory, tileset_name);
	const char *basename_coll = fl_filename_name(filename_coll);

	if (_metatileset.modified()) {
		if (_metatileset.other_modified(filename)) {
			std::string msg = basename;
			msg = msg + " was modified by another program!\n\n"
				"Save the blockset and overwrite it anyway?";
			_unsaved_dialog->message(msg);
			_unsaved_dialog->show(this);
			if (_unsaved_dialog->canceled()) { return true; }
		}

		if (!_metatileset.write_metatiles(filename)) {
			std::string msg = "Could not write to ";
			msg = msg + basename + "!";
			_error_dialog->message(msg);
			_error_dialog->show(this);
			return false;
		}

		if (_has_collisions) {
			if (_metatileset.other_modified_collisions(filename_coll)) {
				std::string msg = basename_coll;
				msg = msg + " was modified by another program!\n\n"
					"Save the collisions and overwrite it anyway?";
				_unsaved_dialog->message(msg);
				_unsaved_dialog->show(this);
				if (_unsaved_dialog->canceled()) { return true; }
			}

			if (!_metatileset.write_collisions(filename_coll)) {
				std::string msg = "Could not write to ";
				msg = msg + basename_coll + "!";
				_error_dialog->message(msg);
				_error_dialog->show(this);
			}
		}

		_metatileset.modified(false);
	}

	std::string msg = "Saved ";
	msg = msg + basename;
	if (_has_collisions) {
		msg = msg + "\nand " + basename_coll;
	}
	msg = msg + "!";
	_success_dialog->message(msg);
	_success_dialog->show(this);

	size_t n = _metatileset.size();
	if (_has_collisions && n > 128 && !Preferences::get("meta128", 0)) {
		msg = "Warning: ";
		msg = msg + basename + " has " + std::to_string(n) + " blocks.\n\n"
			"Be sure to fix the 128-block limit:\n"
			"https://github.com/pret/pokecrystal/blob/master/docs/bugs_and_glitches.md\n"
			"(\"LoadMetatiles wraps around past 128 blocks\")";
		_warning_dialog->message(msg);
		_warning_dialog->show(this);
		Preferences::set("meta128", 1);
	}

	return true;
}

bool Main_Window::save_tileset() {
	_metatileset.trim_tileset();

	Tileset &tileset = _metatileset.tileset();

	char filename[FL_PATH_MAX] = {}, b_filename[FL_PATH_MAX] = {}, a_filename[FL_PATH_MAX] = {};
	const char *directory = _directory.c_str();
	const char *tileset_name = tileset.name();

	if (!tileset.modified()) {
		std::string msg = "Saved ";
		msg = msg + tileset_name + "!";
		_success_dialog->message(msg);
		_success_dialog->show(this);
		return true;
	}

	Config::tileset_png_paths(filename, b_filename, a_filename, directory, tileset_name);
	const char *basename = fl_filename_name(filename);

	const char *mod_basename = NULL;
	if (tileset.other_modified(filename)) {
		mod_basename = basename;
	}
	else if (tileset.other_modified_before(b_filename)) {
		mod_basename = fl_filename_name(b_filename);
	}
	else if (tileset.other_modified_before(a_filename)) {
		mod_basename = fl_filename_name(a_filename);
	}
	if (mod_basename) {
		std::string msg = mod_basename;
		msg = msg + " was modified by another program!\n\n"
			"Save the tileset and overwrite it anyway?";
		_unsaved_dialog->message(msg);
		_unsaved_dialog->show(this);
		if (_unsaved_dialog->canceled()) { return true; }
	}

	if (const char *ff = tileset.write_graphics(filename, b_filename, a_filename); ff) {
		const char *bff = fl_filename_name(ff);
		std::string msg = "Could not write to ";
		msg = msg + bff + "!";
		_error_dialog->message(msg);
		_error_dialog->show(this);
		return false;
	}

	std::string msg = "Saved ";
	msg = msg + basename + "!";
	_success_dialog->message(msg);
	_success_dialog->show(this);

	if (!Config::monochrome()) {
		Config::palette_map_path(filename, directory, tileset_name);
		basename = fl_filename_name(filename);

		if (tileset.palette_map().other_modified(filename)) {
			msg = basename;
			msg = msg + " was modified by another program!\n\n"
				"Save the palette map and overwrite it anyway?";
			_unsaved_dialog->message(msg);
			_unsaved_dialog->show(this);
			if (_unsaved_dialog->canceled()) { return true; }
		}

		if (!tileset.palette_map().write_palette_map(filename)) {
			msg = "Could not write to ";
			msg = msg + basename + "!";
			_error_dialog->message(msg);
			_error_dialog->show(this);
			return false;
		}

		msg = "Saved ";
		msg = msg + basename + "!";
		_success_dialog->message(msg);
		_success_dialog->show(this);
	}

	tileset.modified(false);
	return true;
}

bool Main_Window::save_roof() {
	Tileset &tileset = _metatileset.tileset();

	char filename[FL_PATH_MAX] = {};
	const char *directory = _directory.c_str();
	const char *roof_name = tileset.roof_name();

	if (!tileset.modified_roof()) {
		std::string msg = "Saved ";
		msg = msg + roof_name + "!";
		_success_dialog->message(msg);
		_success_dialog->show(this);
		return true;
	}

	Config::roof_png_path(filename, directory, roof_name);
	const char *basename = fl_filename_name(filename);

	if (tileset.other_modified_roof(filename)) {
		std::string msg = basename;
		msg = msg + " was modified by another program!\n\n"
			"Save the roof and overwrite it anyway?";
		_unsaved_dialog->message(msg);
		_unsaved_dialog->show(this);
		if (_unsaved_dialog->canceled()) { return true; }
	}

	if (!tileset.write_roof_graphics(filename)) {
		std::string msg = "Could not write to ";
		msg = msg + basename + "!";
		_error_dialog->message(msg);
		_error_dialog->show(this);
		return false;
	}

	std::string msg = "Saved ";
	msg = msg + basename + "!";
	_success_dialog->message(msg);
	_success_dialog->show(this);

	tileset.modified_roof(false);
	return true;
}

bool Main_Window::save_event_script() {
	const char *filename = _asm_file.c_str();
	const char *basename = fl_filename_name(filename);

	if (!_map_events.modified()) {
		std::string msg = "Saved ";
		msg = msg + basename + "!";
		_success_dialog->message(msg);
		_success_dialog->show(this);
		return true;
	}

	if (_map_events.other_modified(filename)) {
		std::string msg = basename;
		msg = msg + " was modified by another program!\n\n"
			"Save the events and overwrite it anyway?";
		_unsaved_dialog->message(msg);
		_unsaved_dialog->show(this);
		if (_unsaved_dialog->canceled()) { return true; }
	}

	if (!_map_events.write_event_script(filename)) {
		std::string msg = "Could not write to ";
		msg = msg + basename + "!";
		_error_dialog->message(msg);
		_error_dialog->show(this);
		return false;
	}

	std::string msg = "Saved ";
	msg = msg + basename + "!";
	_success_dialog->message(msg);
	_success_dialog->show(this);

	_map_events.modified(false);
	return true;
}

bool Main_Window::export_palettes(const char *filename) {
	const char *basename = fl_filename_name(filename);

	if (!Color::write_palettes(filename)) {
		std::string msg = "Could not write to ";
		msg = msg + basename + "!";
		_error_dialog->message(msg);
		_error_dialog->show(this);
		return false;
	}

	std::string msg = "Exported ";
	msg = msg + basename + "!";
	_success_dialog->message(msg);
	_success_dialog->show(this);

	_edited_palettes = false;
	return true;
}

void Main_Window::print_map() {
	size_t nb = _map.size();
	for (size_t i = 0; i < nb; i++) {
		Block *b = _map.block(i);
		b->print();
	}
	if (Config::print_events()) {
		size_t ne = _map_events.size();
		for (size_t i = 0; i < ne; i++) {
			Event *e = _map_events.event(i);
			e->print(Config::print_warp_ids());
		}
	}
}
