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

void Main_Window::drag_and_drop_cb(DnD_Receiver *dndr, Main_Window *mw) {
	Fl_Window *top = Fl::modal();
	if (top && top != mw) { return; }
	if (mw->unsaved()) {
		std::string msg = mw->modified_filename();
		msg = msg + " has unsaved changes!\n\n"
			"Open another map anyway?";
		mw->_unsaved_dialog->message(msg);
		mw->_unsaved_dialog->show(mw);
		if (mw->_unsaved_dialog->canceled()) { return; }
	}
	std::string filename = dndr->text().substr(0, dndr->text().find('\n'));
	mw->open_map(filename.c_str());
}

void Main_Window::new_cb(Fl_Widget *, Main_Window *mw) {
	if (mw->unsaved()) {
		std::string msg = mw->modified_filename();
		msg = msg + " has unsaved changes!\n\n"
			"Create a new map anyway?";
		mw->_unsaved_dialog->message(msg);
		mw->_unsaved_dialog->show(mw);
		if (mw->_unsaved_dialog->canceled()) { return; }
	}

	char directory[FL_PATH_MAX] = {};

	if (!mw->_map.size()) {
		int status = mw->_new_dir_chooser->show();
		if (status == 1) { return; }
		if (status == -1) {
			std::string msg = "Could not get project directory!";
			mw->_error_dialog->message(msg);
			mw->_error_dialog->show(mw);
			return;
		}

		const char *project_dir = mw->_new_dir_chooser->filename();
		strcpy(directory, project_dir);
		strcat(directory, DIR_SEP);
	}
	else {
		strcpy(directory, mw->_directory.c_str());
	}

	Config::project_path_from_blk_path(directory, directory);
	mw->open_map(directory, NULL);
}

void Main_Window::open_cb(Fl_Widget *, Main_Window *mw) {
	if (mw->unsaved()) {
		std::string msg = mw->modified_filename();
		msg = msg + " has unsaved changes!\n\n"
			"Open another map anyway?";
		mw->_unsaved_dialog->message(msg);
		mw->_unsaved_dialog->show(mw);
		if (mw->_unsaved_dialog->canceled()) { return; }
	}

	int status = mw->_blk_open_chooser->show();
	if (status == 1) { return; }

	const char *filename = mw->_blk_open_chooser->filename();
	const char *basename = fl_filename_name(filename);
	if (status == -1) {
		std::string msg = "Could not open ";
		msg = msg + basename + "!\n\n" + mw->_blk_open_chooser->errmsg();
		mw->_error_dialog->message(msg);
		mw->_error_dialog->show(mw);
		return;
	}

	mw->open_map(filename);
}

void Main_Window::open_recent_cb(Fl_Menu_ *m, Main_Window *mw) {
	int first_recent_i = m->find_index((Fl_Callback *)open_recent_cb);
	int i = m->find_index(m->mvalue()) - first_recent_i;
	mw->open_recent(i);
}

void Main_Window::clear_recent_cb(Fl_Menu_ *, Main_Window *mw) {
	for (int i = 0; i < NUM_RECENT; i++) {
		mw->_recent[i].clear();
		mw->_recent_mis[i]->hide();
	}
}

void Main_Window::close_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_map.size()) { return; }

	if (mw->unsaved()) {
		std::string msg = mw->modified_filename();
		msg = msg + " has unsaved changes!\n\n"
			"Close it anyway?";
		mw->_unsaved_dialog->message(msg);
		mw->_unsaved_dialog->show(mw);
		if (mw->_unsaved_dialog->canceled()) { return; }
	}

	mw->label(PROGRAM_NAME);
	mw->_sidebar->clear();
	mw->_sidebar->scroll_to(0, 0);
	mw->_sidebar->contents(0, 0);
	std::fill_n(mw->_metatile_buttons, MAX_NUM_METATILES, (Metatile_Button *)NULL);
	mw->_selected = NULL;
	mw->_copied = false;
	mw->_hotkey_metatiles.clear();
	mw->_metatile_hotkeys.clear();
	mw->_map_group->clear();
	mw->_map_group->size(0, 0);
	mw->_map.clear();
	mw->_map_events.clear();
	mw->_map_scroll->scroll_to(0, 0);
	mw->_map_scroll->contents(0, 0);
	mw->init_sizes();
	mw->update_status((Block *)NULL);
	mw->_directory.clear();
	mw->_blk_file.clear();
	mw->_metatileset.clear();
	mw->_block_window->tileset(NULL);
	mw->_tileset_window->tileset(NULL);
	mw->_roof_window->tileset(NULL);

	mw->update_active_controls();
	mw->redraw();
}

void Main_Window::save_cb(Fl_Widget *w, Main_Window *mw) {
	if (!mw->_map.size()) { return; }
	bool other_modified = false;
	if (mw->_metatileset.const_tileset().modified()) {
		save_tileset_cb(w, mw);
		other_modified = true;
	}
	if (mw->_metatileset.const_tileset().modified_roof()) {
		save_roof_cb(w, mw);
		other_modified = true;
	}
	if (mw->_metatileset.modified()) {
		save_metatiles_cb(w, mw);
		other_modified = true;
	}
	if (mw->_map_events.modified()) {
		save_event_script_cb(w, mw);
		other_modified = true;
	}
	if (other_modified && !mw->_map.modified()) { return; }
	save_map_cb(w, mw);
}

void Main_Window::save_as_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_map.size()) { return; }

	int status = mw->_blk_save_chooser->show();
	if (status == 1) { return; }

	char filename[FL_PATH_MAX] = {};
	add_dot_ext(mw->_blk_save_chooser->filename(), ".blk", filename);
	const char *basename = fl_filename_name(filename);

	if (status == -1) {
		std::string msg = "Could not open ";
		msg = msg + basename + "!\n\n" + mw->_blk_save_chooser->errmsg();
		mw->_error_dialog->message(msg);
		mw->_error_dialog->show(mw);
		return;
	}

	char directory[FL_PATH_MAX] = {};
	if (!Config::project_path_from_blk_path(filename, directory)) {
		std::string msg = "Could not get project directory for ";
		msg = msg + basename + "!";
		mw->_error_dialog->message(msg);
		mw->_error_dialog->show(mw);
		return;
	}

	mw->_directory.assign(directory);
	mw->_blk_file.assign(filename);

	char buffer[FL_PATH_MAX] = {};
	sprintf(buffer, PROGRAM_NAME " - %s", basename);
	mw->copy_label(buffer);

	strcpy(buffer, basename);
	fl_filename_setext(buffer, FL_PATH_MAX, ".png");
	mw->_png_chooser->preset_file(buffer);

	mw->save_map(true);
}

void Main_Window::save_map_cb(Fl_Widget *w, Main_Window *mw) {
	if (mw->_blk_file.empty()) {
		save_as_cb(w, mw);
	}
	else {
		mw->save_map(false);
	}
}

void Main_Window::save_metatiles_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_map.size()) { return; }
	mw->save_metatileset();
}

void Main_Window::save_tileset_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_map.size()) { return; }
	mw->save_tileset();
}

void Main_Window::save_roof_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_map.size() || !mw->_metatileset.const_tileset().num_roof_tiles()) { return; }
	mw->save_roof();
}

void Main_Window::load_event_script_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_map.size()) { return; }

	int status = mw->_asm_chooser->show();
	if (status == 1) { return; }

	const char *filename = mw->_asm_chooser->filename();
	const char *basename = fl_filename_name(filename);
	if (status == -1) {
		std::string msg = "Could not open ";
		msg = msg + basename + "!\n\n" + mw->_asm_chooser->errmsg();
		mw->_error_dialog->message(msg);
		mw->_error_dialog->show(mw);
		return;
	}

	if (mw->_map_events.modified()) {
		std::string msg = "The events have been edited!\n\n";
		msg = msg + "Load " + basename + " anyway?";
		mw->_unsaved_dialog->message(msg);
		mw->_unsaved_dialog->show(mw);
		if (mw->_unsaved_dialog->canceled()) { return; }
	}

	mw->unload_events();
	mw->load_events(filename);

	mw->update_active_controls();
	mw->redraw();
}

void Main_Window::view_event_script_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_map_events.loaded()) { return; }
	mw->view_event_script(NULL);
}

void Main_Window::save_event_script_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_map_events.loaded()) { return; }
	mw->save_event_script();
}

void Main_Window::reload_event_script_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_map_events.loaded()) { return; }

	if (mw->_map_events.modified()) {
		const char *basename = fl_filename_name(mw->_asm_file.c_str());
		std::string msg = "The events have been edited!\n\n";
		msg = msg + "Reload " + basename + " anyway?";
		mw->_unsaved_dialog->message(msg);
		mw->_unsaved_dialog->show(mw);
		if (mw->_unsaved_dialog->canceled()) { return; }
	}

	std::string filename(mw->_asm_file);
	mw->unload_events();
	mw->load_events(filename.c_str());

	mw->update_active_controls();
	mw->redraw();
}

void Main_Window::unload_event_script_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_map_events.loaded()) { return; }

	if (mw->_map_events.modified()) {
		const char *basename = fl_filename_name(mw->_asm_file.c_str());
		std::string msg = "The events have been edited!\n\n";
		msg = msg + "Unload " + basename + " anyway?";
		mw->_unsaved_dialog->message(msg);
		mw->_unsaved_dialog->show(mw);
		if (mw->_unsaved_dialog->canceled()) { return; }
	}

	mw->unload_events();

	mw->update_active_controls();
	mw->redraw();
}

void Main_Window::load_roof_colors_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_map.size()) { return; }

	if (!mw->_map.group()) {
		std::string msg = "The map's group is unknown!";
		mw->_error_dialog->message(msg);
		mw->_error_dialog->show(mw);
		return;
	}

	mw->load_roof_colors(false);
}

void Main_Window::load_palettes_cb(Fl_Widget *, Main_Window *mw) {
	int status = mw->_pal_load_chooser->show();
	if (status == 1) { return; }

	const char *filename = mw->_pal_load_chooser->filename();
	const char *basename = fl_filename_name(filename);
	if (status == -1) {
		std::string msg = "Could not open ";
		msg = msg + basename + "!\n\n" + mw->_pal_load_chooser->errmsg();
		mw->_error_dialog->message(msg);
		mw->_error_dialog->show(mw);
		return;
	}

	mw->load_palettes(filename);

	mw->update_active_controls();
	mw->redraw();
}

void Main_Window::export_palettes_cb(Fl_Widget *, Main_Window *mw) {
	int status = mw->_pal_save_chooser->show();
	if (status == 1) { return; }

	char filename[FL_PATH_MAX] = {};
	add_dot_ext(mw->_pal_save_chooser->filename(), ".pal", filename);
	const char *basename = fl_filename_name(filename);

	if (status == -1) {
		std::string msg = "Could not open ";
		msg = msg + basename + "!\n\n" + mw->_pal_save_chooser->errmsg();
		mw->_error_dialog->message(msg);
		mw->_error_dialog->show(mw);
		return;
	}

	mw->export_palettes(filename);
}

void Main_Window::print_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_map.size()) { return; }

	mw->_print_options_dialog->show(mw);
	Config::print_grid(mw->_print_options_dialog->grid());
	Config::print_ids(mw->_print_options_dialog->ids());
	Config::print_priority(mw->_print_options_dialog->priority());
	Config::print_events(mw->_print_options_dialog->events());
	Config::print_warp_ids(mw->_print_options_dialog->warp_ids());
	if (mw->_print_options_dialog->canceled()) { return; }

	int w = (int)mw->_map.width() * METATILE_PX_SIZE, h = (int)mw->_map.height() * METATILE_PX_SIZE;
	if (mw->_print_options_dialog->copied()) {
		Fl_Copy_Surface *surface = new Fl_Copy_Surface(w, h);
		surface->set_current();
		mw->print_map();
		delete surface;
		Fl_Display_Device::display_device()->set_current();

		std::string msg = "Copied to clipboard!";
		mw->_success_dialog->message(msg);
		mw->_success_dialog->show(mw);
	}
	else {
		int status = mw->_png_chooser->show();
		if (status == 1) { return; }

		char filename[FL_PATH_MAX] = {};
		add_dot_ext(mw->_png_chooser->filename(), ".png", filename);
		const char *basename = fl_filename_name(filename);

		if (status == -1) {
			std::string msg = "Could not print to ";
			msg = msg + basename + "!\n\n" + mw->_png_chooser->errmsg();
			mw->_error_dialog->message(msg);
			mw->_error_dialog->show(mw);
			return;
		}

		Fl_Image_Surface *surface = new Fl_Image_Surface(w, h);
		surface->set_current();
		mw->print_map();
		Fl_RGB_Image *image = surface->image();
		delete surface;
		Fl_Display_Device::display_device()->set_current();

		Image::Result result = Image::write_rgb_image(filename, image);
		if (result != Image::Result::IMAGE_OK) {
			std::string msg = "Could not print to ";
			msg = msg + basename + "!\n\n" + Image::error_message(result);
			mw->_error_dialog->message(msg);
			mw->_error_dialog->show(mw);
		}
		else {
			std::string msg = "Printed ";
			msg = msg + basename + "!";
			mw->_success_dialog->message(msg);
			mw->_success_dialog->show(mw);
		}
	}
}

void Main_Window::exit_cb(Fl_Widget *, Main_Window *mw) {
	// Override default behavior of Esc to close main window
	if (Fl::event() == FL_SHORTCUT && Fl::event_key() == FL_Escape) { return; }

	if (mw->unsaved()) {
		std::string msg = mw->modified_filename();
		msg = msg + " has unsaved changes!\n\n"
			"Exit anyway?";
		mw->_unsaved_dialog->message(msg);
		mw->_unsaved_dialog->show(mw);
		if (mw->_unsaved_dialog->canceled()) { return; }
	}

	/*if (mw->_edited_palettes) {
		std::string msg = "The palettes have been edited!\n\n"
			"Exit anyway?";
		mw->_unsaved_dialog->message(msg);
		mw->_unsaved_dialog->show(mw);
		if (mw->_unsaved_dialog->canceled()) { return; }
	}*/

	// Save global config
	Preferences::set("theme", (int)OS::current_theme());
	if (mw->full_screen()) {
		Preferences::set("x", mw->_wx);
		Preferences::set("y", mw->_wy);
		Preferences::set("w", mw->_ww);
		Preferences::set("h", mw->_wh);
		Preferences::set("fullscreen", 1);
	}
	else if (mw->maximized()) {
#ifdef _WIN32
		HWND hwnd = fl_xid(mw);
		WINDOWPLACEMENT wp;
		wp.length = sizeof(wp);
		if (GetWindowPlacement(hwnd, &wp)) {
			// Get the window border size
			RECT br;
			SetRectEmpty(&br);
			DWORD styleEx = GetWindowLong(hwnd, GWL_EXSTYLE);
			AdjustWindowRectEx(&br, WS_OVERLAPPEDWINDOW, FALSE, styleEx);
			// Subtract the border size from the normal window position
			RECT wr = wp.rcNormalPosition;
			wr.left -= br.left;
			wr.right -= br.right;
			wr.top -= br.top;
			wr.bottom -= br.bottom;
			Preferences::set("x", wr.left);
			Preferences::set("y", wr.top);
			Preferences::set("w", wr.right - wr.left);
			Preferences::set("h", wr.bottom - wr.top);
		}
		else {
			Preferences::set("x", mw->x());
			Preferences::set("y", mw->y());
			Preferences::set("w", mw->w());
			Preferences::set("h", mw->h());
		}
#else
		Preferences::set("x", mw->_wx);
		Preferences::set("y", mw->_wy);
		Preferences::set("w", mw->_ww);
		Preferences::set("h", mw->_wh);
#endif
		Preferences::set("fullscreen", 0);
	}
	else {
		Preferences::set("x", mw->x());
		Preferences::set("y", mw->y());
		Preferences::set("w", mw->w());
		Preferences::set("h", mw->h());
		Preferences::set("fullscreen", 0);
	}
	Preferences::set("maximized", mw->maximized());
	Preferences::set("mode", (int)mw->mode());
	Preferences::set("grid", mw->grid());
	Preferences::set("rulers", mw->rulers());
	Preferences::set("zoom", mw->zoom());
	Preferences::set("ids", mw->ids());
	Preferences::set("hex", mw->hex());
	Preferences::set("priority", mw->show_priority());
	Preferences::set("gameboy", mw->gameboy_screen());
	Preferences::set("event", mw->show_events());
	Preferences::set("warp-ids", mw->show_warp_ids());
	Preferences::set("transparent", mw->transparent());
	Preferences::set("palettes", (int)mw->palettes());
	Preferences::set("monochrome", mw->monochrome());
	Preferences::set("prism", Config::prism());
	Preferences::set("prioritize", mw->allow_priority());
	Preferences::set("all256", mw->allow_256_tiles());
	Preferences::set("roof-palettes", (int)mw->_roof_palettes);
	Preferences::set("events", mw->auto_load_events());
	Preferences::set("special", mw->auto_load_special_palettes());
	Preferences::set("roofs", mw->auto_load_roof_colors());
	Preferences::set("drag", mw->drag_and_drop());
	Preferences::set("overworld-map", (int)Config::overworld_map_size());
	Preferences::set("print-grid", Config::print_grid());
	Preferences::set("print-ids", Config::print_ids());
	Preferences::set("print-priority", Config::print_priority());
	Preferences::set("print-events", Config::print_events());
	Preferences::set("print-warp-ids", Config::print_warp_ids());
	for (int i = 0; i < NUM_RECENT; i++) {
		Preferences::set_string(Fl_Preferences::Name("recent%d", i), mw->_recent[i]);
	}
	if (mw->_resize_dialog->initialized()) {
		Preferences::set("resize-anchor", mw->_resize_dialog->anchor());
	}

	Preferences::close();

	exit(EXIT_SUCCESS);
}

void Main_Window::undo_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_map.size()) { return; }
	mw->_map.undo();
	mw->update_active_controls();
	mw->redraw();
}

void Main_Window::redo_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_map.size()) { return; }
	mw->_map.redo();
	mw->update_active_controls();
	mw->redraw();
}

void Main_Window::copy_metatile_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_selected) { return; }
	uint8_t id = mw->_selected->id();
	Metatile *src = mw->_metatileset.metatile(id);
	mw->_clipboard = *src;
	mw->_copied = true;
	mw->update_active_controls();
	mw->redraw();
}

void Main_Window::paste_metatile_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_copied || !mw->_selected) { return; }
	uint8_t id = mw->_selected->id();
	Metatile *dest = mw->_metatileset.metatile(id);
	dest->copy(&mw->_clipboard);
	mw->_metatileset.modified(true);
	mw->redraw();
}

void Main_Window::swap_metatiles_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_copied || !mw->_selected) { return; }
	uint8_t id1 = mw->_clipboard.id(), id2 = mw->_selected->id();
	Metatile *mt1 = mw->_metatileset.metatile(id1), *mt2 = mw->_metatileset.metatile(id2);
	mt1->swap(mt2);
	mw->_metatileset.modified(true);
	mw->redraw();
}

void Main_Window::classic_theme_cb(Fl_Menu_ *, Main_Window *mw) {
	OS::use_classic_theme();
	mw->_classic_theme_mi->setonly();
	mw->update_icons();
	mw->redraw();
}

void Main_Window::aero_theme_cb(Fl_Menu_ *, Main_Window *mw) {
	OS::use_aero_theme();
	mw->_aero_theme_mi->setonly();
	mw->update_icons();
	mw->redraw();
}

void Main_Window::metro_theme_cb(Fl_Menu_ *, Main_Window *mw) {
	OS::use_metro_theme();
	mw->_metro_theme_mi->setonly();
	mw->update_icons();
	mw->redraw();
}

void Main_Window::aqua_theme_cb(Fl_Menu_ *, Main_Window *mw) {
	OS::use_aqua_theme();
	mw->_aqua_theme_mi->setonly();
	mw->update_icons();
	mw->redraw();
}

void Main_Window::greybird_theme_cb(Fl_Menu_ *, Main_Window *mw) {
	OS::use_greybird_theme();
	mw->_greybird_theme_mi->setonly();
	mw->update_icons();
	mw->redraw();
}

void Main_Window::ocean_theme_cb(Fl_Menu_ *, Main_Window *mw) {
	OS::use_ocean_theme();
	mw->_ocean_theme_mi->setonly();
	mw->update_icons();
	mw->redraw();
}

void Main_Window::blue_theme_cb(Fl_Menu_ *, Main_Window *mw) {
	OS::use_blue_theme();
	mw->_blue_theme_mi->setonly();
	mw->update_icons();
	mw->redraw();
}

void Main_Window::olive_theme_cb(Fl_Menu_ *, Main_Window *mw) {
	OS::use_olive_theme();
	mw->_olive_theme_mi->setonly();
	mw->update_icons();
	mw->redraw();
}

void Main_Window::rose_gold_theme_cb(Fl_Menu_ *, Main_Window *mw) {
	OS::use_rose_gold_theme();
	mw->_rose_gold_theme_mi->setonly();
	mw->update_icons();
	mw->redraw();
}

void Main_Window::dark_theme_cb(Fl_Menu_ *, Main_Window *mw) {
	OS::use_dark_theme();
	mw->_dark_theme_mi->setonly();
	mw->update_icons();
	mw->redraw();
}

void Main_Window::brushed_metal_theme_cb(Fl_Menu_ *, Main_Window *mw) {
	OS::use_brushed_metal_theme();
	mw->_brushed_metal_theme_mi->setonly();
	mw->update_icons();
	mw->redraw();
}

void Main_Window::high_contrast_theme_cb(Fl_Menu_ *, Main_Window *mw) {
	OS::use_high_contrast_theme();
	mw->_high_contrast_theme_mi->setonly();
	mw->update_icons();
	mw->redraw();
}

void Main_Window::transparent_cb(Fl_Menu_ *, Main_Window *mw) {
	mw->apply_transparency();
}

void Main_Window::full_screen_cb(Fl_Menu_ *m, Main_Window *mw) {
	if (m->mvalue()->value()) {
		mw->_wx = mw->x(); mw->_wy = mw->y();
		mw->_ww = mw->w(); mw->_wh = mw->h();
		mw->fullscreen();
	}
	else {
		mw->fullscreen_off(mw->_wx, mw->_wy, mw->_ww, mw->_wh);
	}
}

#define SYNC_TB_WITH_M(tb, m) tb->value(m->mvalue()->value())

void Main_Window::grid_cb(Fl_Menu_ *m, Main_Window *mw) {
	SYNC_TB_WITH_M(mw->_grid_tb, m);
	mw->redraw();
}

void Main_Window::rulers_cb(Fl_Menu_ *m, Main_Window *mw) {
	SYNC_TB_WITH_M(mw->_rulers_tb, m);
	mw->update_rulers();
	mw->redraw();
}

void Main_Window::zoom_cb(Fl_Menu_ *m, Main_Window *mw) {
	SYNC_TB_WITH_M(mw->_zoom_tb, m);
	mw->update_zoom();
	mw->redraw();
}

void Main_Window::ids_cb(Fl_Menu_ *m, Main_Window *mw) {
	SYNC_TB_WITH_M(mw->_ids_tb, m);
	mw->redraw();
}

void Main_Window::hex_cb(Fl_Menu_ *m, Main_Window *mw) {
	SYNC_TB_WITH_M(mw->_hex_tb, m);
	mw->update_labels();
	mw->redraw();
}

void Main_Window::show_priority_cb(Fl_Menu_ *m, Main_Window *mw) {
	SYNC_TB_WITH_M(mw->_show_priority_tb, m);
	mw->update_labels();
	mw->redraw();
}

void Main_Window::gameboy_screen_cb(Fl_Menu_ *m, Main_Window *mw) {
	SYNC_TB_WITH_M(mw->_gameboy_screen_tb, m);
	mw->update_gameboy_screen();
	mw->redraw();
}

void Main_Window::show_events_cb(Fl_Menu_ *m, Main_Window *mw) {
	SYNC_TB_WITH_M(mw->_show_events_tb, m);
	mw->update_labels();
	mw->redraw();
}

void Main_Window::show_warp_ids_cb(Fl_Menu_ *m, Main_Window *mw) {
	SYNC_TB_WITH_M(mw->_show_warp_ids_tb, m);
	mw->update_labels();
	mw->redraw();
}

#undef SYNC_TB_WITH_M

#define SYNC_MI_WITH_TB(tb, mi) if (tb->value()) mi->set(); else mi->clear()

void Main_Window::grid_tb_cb(Toolbar_Toggle_Button *, Main_Window *mw) {
	SYNC_MI_WITH_TB(mw->_grid_tb, mw->_grid_mi);
	mw->redraw();
}

void Main_Window::rulers_tb_cb(Toolbar_Toggle_Button *, Main_Window *mw) {
	SYNC_MI_WITH_TB(mw->_rulers_tb, mw->_rulers_mi);
	mw->update_rulers();
	mw->redraw();
}

void Main_Window::zoom_tb_cb(Toolbar_Toggle_Button *, Main_Window *mw) {
	SYNC_MI_WITH_TB(mw->_zoom_tb, mw->_zoom_mi);
	mw->update_zoom();
	mw->redraw();
}

void Main_Window::ids_tb_cb(Toolbar_Toggle_Button *, Main_Window *mw) {
	SYNC_MI_WITH_TB(mw->_ids_tb, mw->_ids_mi);
	mw->redraw();
}

void Main_Window::hex_tb_cb(Toolbar_Toggle_Button *, Main_Window *mw) {
	SYNC_MI_WITH_TB(mw->_hex_tb, mw->_hex_mi);
	mw->update_labels();
	mw->redraw();
}

void Main_Window::show_priority_tb_cb(Toolbar_Toggle_Button *, Main_Window *mw) {
	SYNC_MI_WITH_TB(mw->_show_priority_tb, mw->_show_priority_mi);
	mw->update_labels();
	mw->redraw();
}

void Main_Window::gameboy_screen_tb_cb(Toolbar_Toggle_Button *, Main_Window *mw) {
	SYNC_MI_WITH_TB(mw->_gameboy_screen_tb, mw->_gameboy_screen_mi);
	mw->update_gameboy_screen();
	mw->redraw();
}

void Main_Window::show_events_tb_cb(Toolbar_Toggle_Button *, Main_Window *mw) {
	SYNC_MI_WITH_TB(mw->_show_events_tb, mw->_show_events_mi);
	mw->update_labels();
	mw->redraw();
}

void Main_Window::show_warp_ids_tb_cb(Toolbar_Toggle_Button *, Main_Window *mw) {
	SYNC_MI_WITH_TB(mw->_show_warp_ids_tb, mw->_show_warp_ids_mi);
	mw->update_labels();
	mw->redraw();
}

#undef SYNC_MI_WITH_TB

void Main_Window::morn_palettes_cb(Fl_Menu_ *, Main_Window *mw) {
	mw->_palettes->value((int)Palettes::MORN);
	mw->update_palettes();
	mw->redraw();
}

void Main_Window::day_palettes_cb(Fl_Menu_ *, Main_Window *mw) {
	mw->_palettes->value((int)Palettes::DAY);
	mw->update_palettes();
	mw->redraw();
}

void Main_Window::night_palettes_cb(Fl_Menu_ *, Main_Window *mw) {
	mw->_palettes->value((int)Palettes::NITE);
	mw->update_palettes();
	mw->redraw();
}

void Main_Window::darkness_palettes_cb(Fl_Menu_ *, Main_Window *mw) {
	mw->_palettes->value((int)Palettes::DARKNESS);
	mw->update_palettes();
	mw->redraw();
}

void Main_Window::indoor_palettes_cb(Fl_Menu_ *, Main_Window *mw) {
	mw->_palettes->value((int)Palettes::INDOOR);
	mw->update_palettes();
	mw->redraw();
}

void Main_Window::custom_palettes_cb(Fl_Menu_ *, Main_Window *mw) {
	mw->_palettes->value((int)Palettes::CUSTOM);
	mw->update_palettes();
	mw->redraw();
}

void Main_Window::palettes_cb(Dropdown *, Main_Window *mw) {
	Palettes palettes = (Palettes)mw->_palettes->value();
	switch (palettes) {
	case Palettes::MORN:     mw->_morn_mi->setonly(); break;
	case Palettes::DAY:      mw->_day_mi->setonly(); break;
	case Palettes::NITE:     mw->_night_mi->setonly(); break;
	case Palettes::DARKNESS: mw->_darkness_mi->setonly(); break;
	case Palettes::INDOOR:   mw->_indoor_mi->setonly(); break;
	case Palettes::CUSTOM:   mw->_custom_mi->setonly(); break;
	}
	mw->update_palettes();
	mw->redraw();
}

void Main_Window::blocks_mode_cb(Fl_Menu_ *, Main_Window *mw) {
	mw->_blocks_mode_tb->setonly();
	mw->mode(Mode::BLOCKS);
	mw->redraw();
}

void Main_Window::events_mode_cb(Fl_Menu_ *, Main_Window *mw) {
	mw->_events_mode_tb->setonly();
	mw->mode(Mode::EVENTS);
	mw->redraw();
}

void Main_Window::switch_mode_cb(Fl_Menu_ *, Main_Window *mw) {
	if (mw->mode() == Mode::BLOCKS) {
		mw->_events_mode_mi->setonly();
		mw->_events_mode_tb->setonly();
		mw->mode(Mode::EVENTS);
	}
	else {
		mw->_blocks_mode_mi->setonly();
		mw->_blocks_mode_tb->setonly();
		mw->mode(Mode::BLOCKS);
	}
	mw->update_gameboy_screen();
	mw->redraw();
}

void Main_Window::blocks_mode_tb_cb(Toolbar_Radio_Button *, Main_Window *mw) {
	mw->_blocks_mode_mi->setonly();
	mw->mode(Mode::BLOCKS);
	mw->redraw();
}

void Main_Window::events_mode_tb_cb(Toolbar_Radio_Button *, Main_Window *mw) {
	mw->_events_mode_mi->setonly();
	mw->mode(Mode::EVENTS);
	mw->redraw();
}

void Main_Window::add_sub_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_map.size()) { return; }
	mw->_add_sub_dialog->num_metatiles(mw->_metatileset.size());
	mw->_add_sub_dialog->show(mw);
	if (mw->_add_sub_dialog->canceled()) { return; }
	if (mw->_add_sub_dialog->num_metatiles() != mw->_metatileset.size()) {
		mw->add_sub_metatiles((int)mw->_add_sub_dialog->num_metatiles());
	}
}

void Main_Window::resize_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_map.size()) { return; }
	mw->_resize_dialog->map_size(mw->_map.width(), mw->_map.height());
	mw->_resize_dialog->show(mw);
	if (mw->_resize_dialog->canceled()) { return; }
	if (mw->_resize_dialog->map_width() != mw->_map.width() || mw->_resize_dialog->map_height() != mw->_map.height()) {
		mw->resize_map(mw->_resize_dialog->map_width(), mw->_resize_dialog->map_height());
	}
}

void Main_Window::change_tileset_cb(Fl_Widget *w, Main_Window *mw) {
	if (!mw->_map.size()) { return; }

	const Tileset &tileset = mw->_metatileset.tileset();

	if (mw->_metatileset.modified() || tileset.modified() || tileset.modified_roof()) {
		std::string msg = mw->modified_filename();
		msg = msg + " has unsaved changes!\n\n"
			"Change the tileset anyway?";
		mw->_unsaved_dialog->message(msg);
		mw->_unsaved_dialog->show(mw);
		if (mw->_unsaved_dialog->canceled()) { return; }
	}

	char tileset_name[FL_PATH_MAX] = {};
	strcpy(tileset_name, tileset.name());

	if (w) {
		if (!mw->_tileset_options_dialog->limit_tileset_options(tileset_name)) {
			const char *basename = fl_filename_name(mw->_blk_file.c_str());
			std::string msg = "This is not a valid project!\n\n";
			msg = msg + "Make sure the Options match\n" + basename + ".";
			mw->_error_dialog->message(msg);
			mw->_error_dialog->show(mw);
			return;
		}

		mw->_tileset_options_dialog->show(mw);
		bool canceled = mw->_tileset_options_dialog->canceled();
		if (canceled) { return; }

		strcpy(tileset_name, mw->_tileset_options_dialog->tileset());
	}

	size_t old_size = mw->_metatileset.size();
	mw->_metatileset.clear();

	const char *roof_name = tileset.roof_name();
	if (!mw->read_metatile_data(tileset_name, roof_name)) {
		mw->_map.modified(false);
		mw->_metatileset.modified(false);
		close_cb(NULL, mw);
		return;
	}

	mw->force_add_sub_metatiles(old_size, mw->_metatileset.size());
	mw->_metatileset.modified(false);
	mw->redraw();
}

void Main_Window::edit_tileset_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_map.size()) { return; }

	mw->_tileset_window->tileset(&mw->_metatileset.tileset());
	mw->_tileset_window->show(mw, mw->show_priority());
	bool canceled = mw->_tileset_window->canceled();
	if (canceled) { return; }

	mw->_tileset_window->apply_modifications();
	mw->redraw();
}

void Main_Window::change_roof_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_map.size()) { return; }

	Tileset &tileset = mw->_metatileset.tileset();

	if (tileset.modified_roof()) {
		char basename[FL_PATH_MAX] = {};
		Config::roof_png_path(basename, "", tileset.roof_name());
		std::string msg = fl_filename_name(basename);
		msg = msg + " has unsaved changes!\n\n"
			"Change the roof anyway?";
		mw->_unsaved_dialog->message(msg);
		mw->_unsaved_dialog->show(mw);
		if (mw->_unsaved_dialog->canceled()) { return; }
	}

	char old_name[FL_PATH_MAX] = {};
	strcpy(old_name, tileset.roof_name());

	if (!mw->_roof_options_dialog->limit_roof_options(old_name)) {
		const char *basename = fl_filename_name(mw->_blk_file.c_str());
		std::string msg = "This is not a valid project!\n\n";
		msg = msg + "Make sure the Options match\n" + basename + ".";
		mw->_error_dialog->message(msg);
		mw->_error_dialog->show(mw);
		return;
	}

	mw->_roof_options_dialog->show(mw);
	bool canceled = mw->_roof_options_dialog->canceled();
	if (canceled) { return; }

	const char *roof_name = mw->_roof_options_dialog->roof();
	tileset.roof_name(roof_name);
	if (tileset.has_roof()) {
		char filename[FL_PATH_MAX] = {};
		Config::roof_path(filename, mw->_directory.c_str(), roof_name);
		Tileset::Result rt = tileset.read_roof_graphics(filename);
		if (rt != Tileset::Result::GFX_OK) {
			Config::roof_path(filename, "", roof_name);
			std::string msg = "Error reading ";
			msg = msg + filename + "!\n\n" + Tileset::error_message(rt);
			mw->_error_dialog->message(msg);
			mw->_error_dialog->show(mw);
		}
	}
	else {
		tileset.clear_roof_graphics();
	}

	mw->update_active_controls();
	mw->redraw();
}

void Main_Window::edit_roof_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_map.size()) { return; }

	mw->_roof_window->tileset(&mw->_metatileset.tileset());
	mw->_roof_window->show(mw);
	bool canceled = mw->_roof_window->canceled();
	if (canceled) { return; }

	mw->_roof_window->apply_modifications();
	mw->redraw();
}

void Main_Window::edit_current_palettes_cb(Fl_Widget *, Main_Window *mw) {
	Abstract_Palette_Window *alw = mw->monochrome() ? (Abstract_Palette_Window *)mw->_monochrome_palette_window
		                                             : (Abstract_Palette_Window *)mw->_palette_window;
	alw->current_palettes(mw->palettes());
	alw->show(mw);
	bool canceled = alw->canceled();
	if (canceled) { return; }

	mw->_edited_palettes = true;
	alw->apply_modifications();
	mw->update_palettes();
	mw->redraw();
}

void Main_Window::default_palettes_cb(Fl_Menu_ *, Main_Window *mw) {
	Config::monochrome(false);
	Config::prism(false);
	change_tileset_cb(NULL, mw);
}

void Main_Window::monochrome_cb(Fl_Menu_ *, Main_Window *mw) {
	Config::monochrome(true);
	Config::prism(false);
	change_tileset_cb(NULL, mw);
}

void Main_Window::prism_cb(Fl_Menu_ *, Main_Window *mw) {
	Config::prism(true);
	Config::monochrome(false);
	change_tileset_cb(NULL, mw);
}

void Main_Window::allow_priority_cb(Fl_Menu_ *m, Main_Window *mw) {
	Config::allow_priority(!!m->mvalue()->value());
	mw->update_priority_controls();
	mw->redraw();
}

void Main_Window::allow_256_tiles_cb(Fl_Menu_ *m, Main_Window *mw) {
	Config::allow_256_tiles(!!m->mvalue()->value());
	change_tileset_cb(NULL, mw);
}

void Main_Window::roof_custom_cb(Fl_Menu_ *, Main_Window *mw) {
	mw->_roof_custom_mi->setonly();
	mw->_roof_palettes = Roof_Palettes::ROOF_CUSTOM;
}

void Main_Window::roof_day_nite_cb(Fl_Menu_ *, Main_Window *mw) {
	mw->_roof_day_nite_mi->setonly();
	mw->_roof_palettes = Roof_Palettes::ROOF_DAY_NITE;
}

void Main_Window::roof_morn_day_nite_cb(Fl_Menu_ *, Main_Window *mw) {
	mw->_roof_morn_day_nite_mi->setonly();
	mw->_roof_palettes = Roof_Palettes::ROOF_MORN_DAY_NITE;
}

void Main_Window::roof_day_nite_custom_cb(Fl_Menu_ *, Main_Window *mw) {
	mw->_roof_day_nite_custom_mi->setonly();
	mw->_roof_palettes = Roof_Palettes::ROOF_DAY_NITE_CUSTOM;
}

void Main_Window::roof_morn_day_nite_custom_cb(Fl_Menu_ *, Main_Window *mw) {
	mw->_roof_morn_day_nite_custom_mi->setonly();
	mw->_roof_palettes = Roof_Palettes::ROOF_MORN_DAY_NITE_CUSTOM;
}

void Main_Window::auto_load_events_cb(Fl_Menu_ *m, Main_Window *mw) {
	if (mw->auto_load_events() == !m->mvalue()->value()) {
		mw->redraw();
	}
}

void Main_Window::auto_load_special_palettes_cb(Fl_Menu_ *m, Main_Window *mw) {
	if (mw->auto_load_special_palettes() == !m->mvalue()->value()) {
		mw->redraw();
	}
}

void Main_Window::auto_load_roof_colors_cb(Fl_Menu_ *m, Main_Window *mw) {
	if (mw->auto_load_roof_colors() == !m->mvalue()->value()) {
		mw->redraw();
	}
}

void Main_Window::drag_and_drop_option_cb(Fl_Menu_ *m, Main_Window *) {
	Config::drag_and_drop(!!m->mvalue()->value());
}

void Main_Window::overworld_map_size_cb(Fl_Menu_ *, Main_Window *mw) {
	mw->_overworld_map_size_dialog->overworld_map_size(Config::overworld_map_size());
	mw->_overworld_map_size_dialog->show(mw);
	if (mw->_overworld_map_size_dialog->canceled()) { return; }
	Config::overworld_map_size(mw->_overworld_map_size_dialog->overworld_map_size());
}

void Main_Window::help_cb(Fl_Widget *, Main_Window *mw) {
	mw->_help_window->show(mw);
}

void Main_Window::about_cb(Fl_Widget *, Main_Window *mw) {
	mw->_about_dialog->show(mw);
}

void Main_Window::select_metatile_cb(Metatile_Button *mb, Main_Window *mw) {
	if ((Fl::event() == FL_PASTE || Fl::event() == FL_DND_RELEASE) && mb->dragging
		&& mb->dragging != mb && mb->dragging->active() && mb->active()) {
		Metatile *mt1 = mw->_metatileset.metatile(mb->id());
		Metatile *mt2 = mw->_metatileset.metatile(mb->dragging->id());
		if (Fl::event_ctrl()) {
			// Ctrl+drag to copy
			mt1->copy(mt2);
		}
		else {
			// Drag to swap
			mt1->swap(mt2);
		}
		mw->_metatileset.modified(true);
		mw->redraw();
		mw->_selected = mb;
		mw->_selected->setonly();
	}
	else if (Fl::belowmouse() == mb) {
		// Click to select
		mw->_selected = mb;
		mw->_selected->setonly();
		if (Fl::event_button() == FL_RIGHT_MOUSE) {
			// Right-click to edit
			Metatile *mt = mw->_metatileset.metatile(mb->id());
			mw->_block_window->metatile(mt, mw->_has_collisions, mw->_metatileset.bin_collisions());
			mw->_block_window->show(mw, mw->show_priority());
			if (!mw->_block_window->canceled()) {
				mw->edit_metatile(mt);
			}
		}
	}
}

void Main_Window::change_block_cb(Block *b, Main_Window *mw) {
	if (!mw->_map_editable || mw->_mode != Mode::BLOCKS) { return; }
	if (Fl::event_button() == FL_LEFT_MOUSE) {
		if (!mw->_selected) { return; }
		if (Fl::event_is_click()) {
			mw->_map.remember();
			mw->update_active_controls();
		}
		if (Fl::event_shift()) {
			// Shift+left-click to flood fill
			mw->flood_fill(b, b->id(), mw->_selected->id());
			mw->_map_group->redraw();
		}
		else if (Fl::event_ctrl()) {
			// Ctrl+left-click to replace
			mw->substitute_block(b->id(), mw->_selected->id());
			mw->_map_group->redraw();
		}
		else if (Fl::event_alt()) {
			// Alt+left-click to replace
			mw->swap_blocks(b->id(), mw->_selected->id());
			mw->_map_group->redraw();
		}
		else {
			// Left-click/drag to edit
			uint8_t id = mw->_selected->id();
			b->id(id);
			b->damage(1);
		}
		mw->_map.modified(true);
		mw->update_status(b);
	}
	else if (Fl::event_button() == FL_RIGHT_MOUSE) {
		// Right-click to select
		uint8_t id = b->id();
		if (id >= mw->_metatileset.size()) { return; }
		mw->select_metatile(mw->_metatile_buttons[id]);
	}
}

void Main_Window::change_event_cb(Event *e, Main_Window *mw) {
	if (mw->_mode != Mode::EVENTS) { return; }
	int sx = mw->_map_scroll->x() - mw->_map_scroll->xposition();
	int sy = mw->_map_scroll->y() - mw->_map_scroll->yposition();
	if (Fl::event_button() == FL_LEFT_MOUSE) {
		if (!Fl::event_is_click()) {
			// Left-drag to move
			int ox = Fl::event_x() - EVENT_MARGIN * e->w(), oy = Fl::event_y() - EVENT_MARGIN * e->h();
			int rx = (ox - sx) / e->w() - (ox < sx);
			int ry = (oy - sy) / e->h() - (oy < sy);
			int16_t ex = std::clamp((int16_t)rx, MIN_EVENT_COORD, mw->_map.max_event_x());
			int16_t ey = std::clamp((int16_t)ry, MIN_EVENT_COORD, mw->_map.max_event_y());
			e->coords(ex, ey);
			e->reposition(sx, sy);
			mw->_map_events.modified(true);
			mw->redraw_map();
		}
		else if (Fl::event_shift()) {
			// Shift+click to open a warp's map
			mw->warp_to_map(e);
		}
		else if (Fl::event_clicks()) {
			// Double-click to view .asm file
			mw->view_event_script(e);
		}
	}
	else if (Fl::event_button() == FL_RIGHT_MOUSE && Fl::event_is_click()) {
		// Right-click to edit
		mw->_event_options_dialog->use_event(e);
		mw->_event_options_dialog->show(mw);
		if (!mw->_event_options_dialog->canceled()) {
			mw->_event_options_dialog->update_event(e);
			e->reposition(sx, sy);
			mw->_map_events.refresh_warps();
			mw->_map_events.modified(true);
			mw->redraw();
		}
	}
}
