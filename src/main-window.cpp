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
#include "image.h"
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

// Avoid "warning C4458: declaration of 'i' hides class member"
// due to Fl_Window's Fl_X *i
#pragma warning(push)
#pragma warning(disable : 4458)

Main_Window::Main_Window(int x, int y, int w, int h, const char *) : Fl_Overlay_Window(x, y, w, h, PROGRAM_NAME),
	_directory(), _blk_file(), _asm_file(), _recent(), _metatileset(), _map(), _map_events(), _status_event_x(INT_MIN),
	_status_event_y(INT_MIN), _metatile_buttons(), _clipboard(0), _wx(x), _wy(y), _ww(w), _wh(h) {
	// Get global configs
	Mode mode_config = (Mode)Preferences::get("mode", (int)Mode::BLOCKS);
	mode(mode_config);

	Roof_Palettes roof_palettes_config = (Roof_Palettes)Preferences::get("roof-palettes", (int)Roof_Palettes::ROOF_DAY_NITE);
	_roof_palettes = roof_palettes_config;

	int grid_config = Preferences::get("grid", 1);
	int rulers_config = Preferences::get("rulers", 0);
	int zoom_config = Preferences::get("zoom", 0);
	int ids_config = Preferences::get("ids", 0);
	int hex_config = Preferences::get("hex", 0);
	int show_priority_config = Preferences::get("priority", 1);
	int gameboy_screen_config = Preferences::get("gameboy", 0);
	int show_events_config = Preferences::get("event", 1);
	int show_warp_ids_config = Preferences::get("warp-ids", 1);
	Palettes palettes_config = (Palettes)Preferences::get("palettes", (int)Palettes::DAY);

	int monochrome_config = Preferences::get("monochrome", 0);
	int prism_config = Preferences::get("prism", 0);
	if (monochrome_config && prism_config) { prism_config = 0; } // exclusive render modes
	int allow_priority_config = Preferences::get("prioritize", 0);
	int allow_256_tiles_config = Preferences::get("all256", 0);
	int drag_and_drop_config = Preferences::get("drag", 1);
	Config::monochrome(!!monochrome_config);
	Config::prism(!!prism_config);
	Config::allow_priority(!!allow_priority_config);
	Config::allow_256_tiles(!!allow_256_tiles_config);
	Config::drag_and_drop(!!drag_and_drop_config);

	int print_grid_config = Preferences::get("print-grid", 0);
	int print_ids_config = Preferences::get("print-ids", 0);
	int print_priority_config = Preferences::get("print-priority", 0);
	int print_events_config = Preferences::get("print-events", 0);
	int print_warp_ids_config = Preferences::get("print-warp-ids", 0);
	Config::print_grid(!!print_grid_config);
	Config::print_ids(!!print_ids_config);
	Config::print_priority(!!print_priority_config);
	Config::print_events(!!print_events_config);
	Config::print_warp_ids(!!print_warp_ids_config);

	int auto_events_config = Preferences::get("events", 1);
	int special_palettes_config = Preferences::get("special", 1);
	int roof_colors_config = Preferences::get("roofs", 1);

	size_t overworld_map_size_config = Preferences::get("overworld-map", 1300);
	Config::overworld_map_size(overworld_map_size_config);

	for (int i = 0; i < NUM_RECENT; i++) {
		_recent[i] = Preferences::get_string(Fl_Preferences::Name("recent%d", i));
	}

	int transparent = Preferences::get("transparent", 0);
	int fullscreen = Preferences::get("fullscreen", 0);

	// Populate window

	int wx = 0, wy = 0, ww = w, wh = h;

	// Initialize menu bar
#ifdef __APPLE__
	Fl_Mac_App_Menu::about = "About " PROGRAM_NAME;
	Fl_Mac_App_Menu::hide = "Hide " PROGRAM_NAME;
	Fl_Mac_App_Menu::quit = "Quit " PROGRAM_NAME;
	_menu_bar = new Fl_Sys_Menu_Bar(wx, wy, w, 0);
#else
	_menu_bar = new Fl_Sys_Menu_Bar(wx, wy, w, 21);
#endif
	wy += _menu_bar->h();
	wh -= _menu_bar->h();

	// Toolbar
#ifdef __APPLE__
	_toolbar = new Toolbar(wx, wy, w, 38);
#define SEPARATE_TOOLBAR_BUTTONS new Fl_Box(0, 0, 12, 36)
#else
	_toolbar = new Toolbar(wx, wy, w, 26);
#define SEPARATE_TOOLBAR_BUTTONS new Fl_Box(0, 0, 2, 24); new Spacer(0, 0, 2, 24); new Fl_Box(0, 0, 2, 24)
#endif
	wy += _toolbar->h();
	wh -= _toolbar->h();
#ifdef __APPLE__
	new Fl_Box(0, 0, 6, 36);
#endif
	_new_tb = new Toolbar_Button(0, 0, 24, 24);
	_open_tb = new Toolbar_Button(0, 0, 24, 24);
	_load_event_script_tb = new Toolbar_Button(0, 0, 24, 24);
	_reload_event_script_tb = new Toolbar_Button(0, 0, 24, 24);
	_save_tb = new Toolbar_Button(0, 0, 24, 24);
	SEPARATE_TOOLBAR_BUTTONS;
	_print_tb = new Toolbar_Button(0, 0, 24, 24);
	SEPARATE_TOOLBAR_BUTTONS;
	_undo_tb = new Toolbar_Button(0, 0, 24, 24);
	_redo_tb = new Toolbar_Button(0, 0, 24, 24);
	SEPARATE_TOOLBAR_BUTTONS;
	_grid_tb = new Toolbar_Toggle_Button(0, 0, 24, 24);
	_rulers_tb = new Toolbar_Toggle_Button(0, 0, 24, 24);
	_zoom_tb = new Toolbar_Toggle_Button(0, 0, 24, 24);
	_ids_tb = new Toolbar_Toggle_Button(0, 0, 24, 24);
	_hex_tb = new Toolbar_Toggle_Button(0, 0, 24, 24);
	_show_priority_tb = new Toolbar_Toggle_Button(0, 0, 24, 24);
	_gameboy_screen_tb = new Toolbar_Toggle_Button(0, 0, 24, 24);
	_show_events_tb = new Toolbar_Toggle_Button(0, 0, 24, 24);
	_show_warp_ids_tb = new Toolbar_Toggle_Button(0, 0, 24, 24);
	SEPARATE_TOOLBAR_BUTTONS;
	_blocks_mode_tb = new Toolbar_Radio_Button(0, 0, 24, 24);
	_events_mode_tb = new Toolbar_Radio_Button(0, 0, 24, 24);
	SEPARATE_TOOLBAR_BUTTONS;
	new Label(0, 0, text_width("Palettes:", 3), 24, "Palettes:");
	_palettes = new Dropdown(0, 0, text_width("Custom", 3) + 24, 22);
	new Fl_Box(0, 0, 4, 24);
	_load_palettes_tb = new Toolbar_Button(0, 0, 24, 24);
	_edit_current_palettes_tb = new Toolbar_Button(0, 0, 24, 24);
	SEPARATE_TOOLBAR_BUTTONS;
	_add_sub_tb = new Toolbar_Button(0, 0, 24, 24);
	_resize_tb = new Toolbar_Button(0, 0, 24, 24);
	SEPARATE_TOOLBAR_BUTTONS;
	_change_tileset_tb = new Toolbar_Button(0, 0, 24, 24);
	_edit_tileset_tb = new Toolbar_Button(0, 0, 24, 24);
	SEPARATE_TOOLBAR_BUTTONS;
	_change_roof_tb = new Toolbar_Button(0, 0, 24, 24);
	_edit_roof_tb = new Toolbar_Button(0, 0, 24, 24);
	_toolbar->end();
#undef SEPARATE_TOOLBAR_BUTTONS
	begin();

	// Status bar
	_status_bar = new Toolbar(wx, h-23, w, 23);
	wh -= _status_bar->h();
	_metatile_count = new Label(0, 0, text_width("Blocks: 999", 8), 21, "");
	new Spacer(0, 0, 2, 21);
	_map_dimensions = new Label(0, 0, text_width("Map: 999 x 999", 8), 21, "");
	new Spacer(0, 0, 2, 21);
	_hover_id = new Label(0, 0, text_width("ID: $99", 8), 21, "");
	new Spacer(0, 0, 2, 21);
	_hover_xy = new Label(0, 0, text_width("X/Y ($99, $99)", 8), 21, "");
	new Spacer(0, 0, 2, 21);
	_hover_event = new Label(0, 0, text_width("Event: X/Y ($999, $999)", 8), 21, "");
	_status_bar->end();
	begin();

	// Sidebar
	int sw = METATILE_PX_SIZE * (zoom_config ? ZOOM_FACTOR : 1) * METATILES_PER_ROW + Fl::scrollbar_size();
	_sidebar = new Workspace(wx, wy, sw, wh);
	wx += _sidebar->w();
	ww -= _sidebar->w();
	_sidebar->type(Fl_Scroll::VERTICAL_ALWAYS);
	_sidebar->end();
	begin();

	// Rulers
	int rs = Fl::scrollbar_size();
	_hor_ruler = new Ruler(wx+rs, wy, ww-rs, rs);
	_ver_ruler = new Ruler(wx, wy+rs, rs, wh-rs);
	_corner_ruler = new Ruler(wx, wy, rs, rs);
	if (rulers_config) {
		wx += _ver_ruler->w();
		ww -= _ver_ruler->w();
		wy += _hor_ruler->h();
		wh -= _hor_ruler->h();
	}

	// Map
	_map_scroll = new Workspace(wx, wy, ww, wh);
	_map_scroll->type(Fl_Scroll::BOTH);
	_map_group = new Fl_Group(wx, wy, 0, 0);
	_map_group->end();
	begin();

	// Dialogs
	_new_dir_chooser = new Directory_Chooser(Fl_Native_File_Chooser::BROWSE_DIRECTORY);
	_blk_open_chooser = new Fl_Native_File_Chooser(Fl_Native_File_Chooser::BROWSE_FILE);
	_blk_save_chooser = new Fl_Native_File_Chooser(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
	_pal_load_chooser = new Fl_Native_File_Chooser(Fl_Native_File_Chooser::BROWSE_FILE);
	_pal_save_chooser = new Fl_Native_File_Chooser(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
	_roof_chooser = new Fl_Native_File_Chooser(Fl_Native_File_Chooser::BROWSE_FILE);
	_asm_chooser = new Fl_Native_File_Chooser(Fl_Native_File_Chooser::BROWSE_FILE);
	_png_chooser = new Fl_Native_File_Chooser(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
	_error_dialog = new Modal_Dialog(this, "Error", Modal_Dialog::Icon::ERROR_ICON);
	_warning_dialog = new Modal_Dialog(this, "Warning", Modal_Dialog::Icon::WARNING_ICON);
	_success_dialog = new Modal_Dialog(this, "Success", Modal_Dialog::Icon::SUCCESS_ICON);
	_unsaved_dialog = new Modal_Dialog(this, "Warning", Modal_Dialog::Icon::WARNING_ICON, true);
	_about_dialog = new Modal_Dialog(this, "About " PROGRAM_NAME, Modal_Dialog::Icon::APP_ICON);
	_map_options_dialog = new Map_Options_Dialog("Map Options");
	_tileset_options_dialog = new Tileset_Options_Dialog("Change Tileset", _map_options_dialog);
	_roof_options_dialog = new Roof_Options_Dialog("Change Roof", _map_options_dialog);
	_event_options_dialog = new Event_Options_Dialog("Edit Event");
	_print_options_dialog = new Print_Options_Dialog("Print Options");
	_resize_dialog = new Resize_Dialog("Resize Map");
	_add_sub_dialog = new Add_Sub_Dialog("Resize Blockset");
	_overworld_map_size_dialog = new Overworld_Map_Size_Dialog("Overworld Map Size");
	_help_window = new Help_Window(48, 48, 700, 500, PROGRAM_NAME " Help");
	_block_window = new Block_Window(48, 48);
	_tileset_window = new Tileset_Window(48, 48);
	_roof_window = new Roof_Window(48, 48);
	_palette_window = new Palette_Window(48, 48);
	_monochrome_palette_window = new Monochrome_Palette_Window(48, 48);

	// Drag-and-drop receiver
	_dnd_receiver = new DnD_Receiver(0, 0, 0, 0);
	_dnd_receiver->callback((Fl_Callback *)drag_and_drop_cb);
	_dnd_receiver->user_data(this);

	// Configure window
	box(OS_BG_BOX);
	size_range(335, 262);
	resizable(_map_scroll);
	callback((Fl_Callback *)exit_cb, this);
	xclass(PROGRAM_NAME);

	// Configure window icon
#ifdef _WIN32
	icon((const void *)LoadIcon(fl_display, MAKEINTRESOURCE(IDI_ICON1)));
#elif defined(__LINUX__)
	fl_open_display();
	XpmCreatePixmapFromData(fl_display, DefaultRootWindow(fl_display), (char **)&APP_ICON_XPM, &_icon_pixmap, &_icon_mask, NULL);
	icon((const void *)_icon_pixmap);
#endif

	// Configure rulers
	_hor_ruler->direction(Ruler::Direction::HORIZONTAL);
	_ver_ruler->direction(Ruler::Direction::VERTICAL);
	_corner_ruler->direction(Ruler::Direction::CORNER);
	_hor_ruler->user_data(this);
	_ver_ruler->user_data(this);
	_corner_ruler->user_data(this);
	if (!rulers_config) {
		_hor_ruler->hide();
		_ver_ruler->hide();
		_corner_ruler->hide();
	}

	// Configure workspaces
	_map_scroll->dnd_receiver(_dnd_receiver);
	_map_scroll->add_correlate(_hor_ruler);
	_map_scroll->add_correlate(_ver_ruler);
	_map_scroll->add_correlate(_corner_ruler);
	_map_scroll->resizable(NULL);
	_map_group->resizable(NULL);
	_map_group->clip_children(1);

	// Configure menu bar
	_menu_bar->box(OS_PANEL_THIN_UP_BOX);
	_menu_bar->down_box(FL_FLAT_BOX);

	// Configure menu bar items
	Fl_Menu_Item menu_items[] = {
		// label, shortcut, callback, data, flags
		OS_SUBMENU("&File"),
		OS_MENU_ITEM("&New...", FL_COMMAND + 'n', (Fl_Callback *)new_cb, this, 0),
		OS_MENU_ITEM("&Open...", FL_COMMAND + 'o', (Fl_Callback *)open_cb, this, 0),
		OS_MENU_ITEM("Open Recent", 0, NULL, NULL, FL_SUBMENU | FL_MENU_DIVIDER),
		// NUM_RECENT items with callback open_recent_cb
		OS_NULL_MENU_ITEM(FL_ALT + '1', (Fl_Callback *)open_recent_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + '2', (Fl_Callback *)open_recent_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + '3', (Fl_Callback *)open_recent_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + '4', (Fl_Callback *)open_recent_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + '5', (Fl_Callback *)open_recent_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + '6', (Fl_Callback *)open_recent_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + '7', (Fl_Callback *)open_recent_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + '8', (Fl_Callback *)open_recent_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + '9', (Fl_Callback *)open_recent_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + '0', (Fl_Callback *)open_recent_cb, this, 0),
		OS_MENU_ITEM("Clear &Recent", 0, (Fl_Callback *)clear_recent_cb, this, 0),
		{},
		OS_MENU_ITEM("&Close", FL_COMMAND + 'w', (Fl_Callback *)close_cb, this, FL_MENU_DIVIDER),
		OS_MENU_ITEM("&Save", FL_COMMAND + 's', (Fl_Callback *)save_cb, this, 0),
		OS_MENU_ITEM("Save &As...", FL_COMMAND + 'S', (Fl_Callback *)save_as_cb, this, 0),
		OS_MENU_ITEM("Save &Map", 0, (Fl_Callback *)save_map_cb, this, 0),
		OS_MENU_ITEM("Save &Blockset", 0, (Fl_Callback *)save_metatiles_cb, this, 0),
		OS_MENU_ITEM("Save &Tileset", 0, (Fl_Callback *)save_tileset_cb, this, 0),
		OS_MENU_ITEM("Save &Roof", 0, (Fl_Callback *)save_roof_cb, this, FL_MENU_DIVIDER),
#ifdef __APPLE__
		OS_MENU_ITEM("&Print...", FL_COMMAND + 'p', (Fl_Callback *)print_cb, this, 0),
#else
		OS_MENU_ITEM("&Print...", FL_COMMAND + 'p', (Fl_Callback *)print_cb, this, FL_MENU_DIVIDER),
		OS_MENU_ITEM("E&xit", FL_ALT + FL_F + 4, (Fl_Callback *)exit_cb, this, 0),
#endif
		{},
		OS_SUBMENU("&Data"),
		OS_MENU_ITEM("Load &Event Script...", FL_COMMAND + 'a', (Fl_Callback *)load_event_script_cb, this, 0),
		OS_MENU_ITEM("Save E&vent Script", FL_COMMAND + 'A', (Fl_Callback *)save_event_script_cb, this, 0),
		OS_MENU_ITEM("V&iew Event Script", FL_COMMAND + 'u', (Fl_Callback *)view_event_script_cb, this, 0),
		OS_MENU_ITEM("Reloa&d Event Script", FL_COMMAND + 'r', (Fl_Callback *)reload_event_script_cb, this, 0),
		OS_MENU_ITEM("&Unload Event Script", FL_COMMAND + 'W', (Fl_Callback *)unload_event_script_cb, this, FL_MENU_DIVIDER),
		OS_MENU_ITEM("Load &Palettes...", FL_COMMAND + 'l', (Fl_Callback *)load_palettes_cb, this, 0),
		OS_MENU_ITEM("Export Pa&lettes...", 0, (Fl_Callback *)export_palettes_cb, this, FL_MENU_DIVIDER),
		OS_MENU_ITEM("Load Roo&f Colors", 0, (Fl_Callback *)load_roof_colors_cb, this, 0),
		{},
		OS_SUBMENU("&Edit"),
		OS_MENU_ITEM("&Undo", FL_COMMAND + 'z', (Fl_Callback *)undo_cb, this, 0),
		OS_MENU_ITEM("&Redo", FL_COMMAND + 'y', (Fl_Callback *)redo_cb, this, FL_MENU_DIVIDER),
		OS_MENU_ITEM("&Copy Block", FL_COMMAND + 'c', (Fl_Callback *)copy_metatile_cb, this, 0),
		OS_MENU_ITEM("&Paste Block", FL_COMMAND + 'v', (Fl_Callback *)paste_metatile_cb, this, 0),
		OS_MENU_ITEM("S&wap Block", FL_COMMAND + 'x', (Fl_Callback *)swap_metatiles_cb, this, 0),
		{},
		OS_SUBMENU("&View"),
		OS_MENU_ITEM("&Theme", 0, NULL, NULL, FL_SUBMENU | FL_MENU_DIVIDER),
		OS_MENU_ITEM("&Classic", 0, (Fl_Callback *)classic_theme_cb, this,
			FL_MENU_RADIO | (OS::current_theme() == OS::Theme::CLASSIC ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&Aero", 0, (Fl_Callback *)aero_theme_cb, this,
			FL_MENU_RADIO | (OS::current_theme() == OS::Theme::AERO ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&Metro", 0, (Fl_Callback *)metro_theme_cb, this,
			FL_MENU_RADIO | (OS::current_theme() == OS::Theme::METRO ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("A&qua", 0, (Fl_Callback *)aqua_theme_cb, this,
			FL_MENU_RADIO | (OS::current_theme() == OS::Theme::AQUA ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&Greybird", 0, (Fl_Callback *)greybird_theme_cb, this,
			FL_MENU_RADIO | (OS::current_theme() == OS::Theme::GREYBIRD ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&Ocean", 0, (Fl_Callback *)ocean_theme_cb, this,
			FL_MENU_RADIO | (OS::current_theme() == OS::Theme::OCEAN ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&Blue", 0, (Fl_Callback *)blue_theme_cb, this,
			FL_MENU_RADIO | (OS::current_theme() == OS::Theme::BLUE ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("Oli&ve", 0, (Fl_Callback *)olive_theme_cb, this,
			FL_MENU_RADIO | (OS::current_theme() == OS::Theme::OLIVE ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&Rose Gold", 0, (Fl_Callback *)rose_gold_theme_cb, this,
			FL_MENU_RADIO | (OS::current_theme() == OS::Theme::ROSE_GOLD ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&Dark", 0, (Fl_Callback *)dark_theme_cb, this,
			FL_MENU_RADIO | (OS::current_theme() == OS::Theme::DARK ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("Brushed Me&tal", 0, (Fl_Callback *)brushed_metal_theme_cb, this,
			FL_MENU_RADIO | (OS::current_theme() == OS::Theme::BRUSHED_METAL ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&High Contrast", 0, (Fl_Callback *)high_contrast_theme_cb, this,
			FL_MENU_RADIO | (OS::current_theme() == OS::Theme::HIGH_CONTRAST ? FL_MENU_VALUE : 0)),
		{},
		OS_MENU_ITEM("&Grid", FL_COMMAND + 'g', (Fl_Callback *)grid_cb, this,
			FL_MENU_TOGGLE | (grid_config ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&Rulers", FL_COMMAND + 'R', (Fl_Callback *)rulers_cb, this,
			FL_MENU_TOGGLE | (rulers_config ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&Zoom", FL_COMMAND + '=', (Fl_Callback *)zoom_cb, this,
			FL_MENU_TOGGLE | (zoom_config ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("Block &IDs", FL_COMMAND + 'i', (Fl_Callback *)ids_cb, this,
			FL_MENU_TOGGLE | (ids_config ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&Hexadecimal", FL_COMMAND + FL_SHIFT + '4', (Fl_Callback *)hex_cb, this,
			FL_MENU_TOGGLE | (hex_config ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("Show &Priority", FL_COMMAND + 'P', (Fl_Callback *)show_priority_cb, this,
			FL_MENU_TOGGLE | (show_priority_config ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("Game &Boy Screen", FL_COMMAND + 'M', (Fl_Callback *)gameboy_screen_cb, this,
			FL_MENU_TOGGLE | (gameboy_screen_config ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("Show &Events", FL_COMMAND + 'V', (Fl_Callback *)show_events_cb, this,
			FL_MENU_TOGGLE | (show_events_config ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("Show &Warp IDs", FL_COMMAND + FL_SHIFT + '3', (Fl_Callback *)show_warp_ids_cb, this,
			FL_MENU_TOGGLE | (show_warp_ids_config ? FL_MENU_VALUE : 0) | FL_MENU_DIVIDER),
		OS_MENU_ITEM("Pa&lettes", 0, NULL, NULL, FL_SUBMENU | FL_MENU_DIVIDER),
		OS_MENU_ITEM("&Morn", 0, (Fl_Callback *)morn_palettes_cb, this,
			FL_MENU_RADIO | (palettes_config == Palettes::MORN ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&Day", 0, (Fl_Callback *)day_palettes_cb, this,
			FL_MENU_RADIO | (palettes_config == Palettes::DAY ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&Night", 0, (Fl_Callback *)night_palettes_cb, this,
			FL_MENU_RADIO | (palettes_config == Palettes::NITE ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("Dar&k", 0, (Fl_Callback *)darkness_palettes_cb, this,
			FL_MENU_RADIO | (palettes_config == Palettes::DARKNESS ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&Indoor", 0, (Fl_Callback *)indoor_palettes_cb, this,
			FL_MENU_RADIO | (palettes_config == Palettes::INDOOR ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&Custom", 0, (Fl_Callback *)custom_palettes_cb, this,
			FL_MENU_RADIO | (palettes_config == Palettes::CUSTOM ? FL_MENU_VALUE : 0)),
		{},
		OS_MENU_ITEM("Tr&ansparent", FL_F + 10, (Fl_Callback *)transparent_cb, this,
			FL_MENU_TOGGLE | (transparent ? FL_MENU_VALUE : 0)),
#ifdef __APPLE__
		// F11 toggles all open windows in macOS
		OS_MENU_ITEM("Full &Screen", FL_COMMAND + FL_SHIFT + 'f', (Fl_Callback *)full_screen_cb, this, FL_MENU_TOGGLE),
#else
		OS_MENU_ITEM("Full &Screen", FL_F + 11, (Fl_Callback *)full_screen_cb, this, FL_MENU_TOGGLE),
#endif
		{},
		OS_SUBMENU("&Mode"),
		OS_MENU_ITEM("&Blocks", FL_COMMAND + 'B', (Fl_Callback *)blocks_mode_cb, this,
			FL_MENU_RADIO | (mode() == Mode::BLOCKS ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&Events", FL_COMMAND + 'E', (Fl_Callback *)events_mode_cb, this,
			FL_MENU_RADIO | (mode() == Mode::EVENTS ? FL_MENU_VALUE : 0) | FL_MENU_DIVIDER),
		OS_MENU_ITEM("&Switch Mode", FL_Tab, (Fl_Callback *)switch_mode_cb, this, 0),
		{},
		OS_SUBMENU("&Tools"),
		OS_MENU_ITEM("Resize &Blockset...", FL_COMMAND + 'b', (Fl_Callback *)add_sub_cb, this, 0),
		OS_MENU_ITEM("Resize &Map...", FL_COMMAND + 'e', (Fl_Callback *)resize_cb, this, FL_MENU_DIVIDER),
#ifdef __APPLE__
		// Command+H hides all open windows in macOS
		OS_MENU_ITEM("Chan&ge Tileset...", FL_COMMAND + 'j', (Fl_Callback *)change_tileset_cb, this, 0),
#else
		OS_MENU_ITEM("Chan&ge Tileset...", FL_COMMAND + 'h', (Fl_Callback *)change_tileset_cb, this, 0),
#endif
		OS_MENU_ITEM("Edit &Tileset...", FL_COMMAND + 't', (Fl_Callback *)edit_tileset_cb, this, FL_MENU_DIVIDER),
#ifdef __APPLE__
		OS_MENU_ITEM("C&hange Roof...", FL_COMMAND + 'J', (Fl_Callback *)change_roof_cb, this, 0),
#else
		OS_MENU_ITEM("C&hange Roof...", FL_COMMAND + 'H', (Fl_Callback *)change_roof_cb, this, 0),
#endif
		OS_MENU_ITEM("Edit &Roof...", FL_COMMAND + 'f', (Fl_Callback *)edit_roof_cb, this, FL_MENU_DIVIDER),
		OS_MENU_ITEM("Edit Current &Palettes...", FL_COMMAND + 'L', (Fl_Callback *)edit_current_palettes_cb, this, 0),
		{},
		OS_SUBMENU("&Options"),
		OS_MENU_ITEM("Palette system", 0, NULL, NULL, FL_SUBMENU | FL_MENU_DIVIDER),
		OS_MENU_ITEM("&Crystal Palettes", 0, (Fl_Callback *)default_palettes_cb, this,
			FL_MENU_RADIO | (!monochrome_config && !prism_config ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&Monochrome", 0, (Fl_Callback *)monochrome_cb, this,
			FL_MENU_RADIO | (monochrome_config ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("Pris&m Palettes", 0, (Fl_Callback *)prism_cb, this,
			FL_MENU_RADIO | (prism_config ? FL_MENU_VALUE : 0)),
		{},
		OS_MENU_ITEM("Tile &Priority", 0, (Fl_Callback *)allow_priority_cb, this,
			FL_MENU_TOGGLE | (allow_priority_config ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("256 &Tiles", 0, (Fl_Callback *)allow_256_tiles_cb, this,
			FL_MENU_TOGGLE | (allow_256_tiles_config ? FL_MENU_VALUE : 0)),
		// Roof Palettes submenu
		OS_MENU_ITEM("Roo&f Palettes", 0, NULL, NULL, FL_SUBMENU | FL_MENU_DIVIDER),
		OS_MENU_ITEM("&Custom", 0, (Fl_Callback *)roof_custom_cb, this,
			FL_MENU_RADIO | (_roof_palettes == Roof_Palettes::ROOF_CUSTOM ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("Morn + Day, &Night", 0, (Fl_Callback *)roof_day_nite_cb, this,
			FL_MENU_RADIO | (_roof_palettes == Roof_Palettes::ROOF_DAY_NITE ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&Morn, Day, Night", 0, (Fl_Callback *)roof_morn_day_nite_cb, this,
			FL_MENU_RADIO | (_roof_palettes == Roof_Palettes::ROOF_MORN_DAY_NITE ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("Morn + Day, Night, C&ustom", 0, (Fl_Callback *)roof_day_nite_custom_cb, this,
			FL_MENU_RADIO | (_roof_palettes == Roof_Palettes::ROOF_DAY_NITE_CUSTOM ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("M&orn, Day, Night, Custom", 0, (Fl_Callback *)roof_morn_day_nite_custom_cb, this,
			FL_MENU_RADIO | (_roof_palettes == Roof_Palettes::ROOF_MORN_DAY_NITE_CUSTOM ? FL_MENU_VALUE : 0)),
		{},
		OS_MENU_ITEM("Auto-Load &Events", 0, (Fl_Callback *)auto_load_events_cb, this,
			FL_MENU_TOGGLE | (auto_events_config ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("Auto-Load &Special Palettes", 0, (Fl_Callback *)auto_load_special_palettes_cb, this,
			FL_MENU_TOGGLE | (special_palettes_config ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("Auto-Load &Roof Colors", 0, (Fl_Callback *)auto_load_roof_colors_cb, this,
			FL_MENU_TOGGLE | (roof_colors_config ? FL_MENU_VALUE : 0) | FL_MENU_DIVIDER),
		OS_MENU_ITEM("&Drag and Drop", 0, (Fl_Callback *)drag_and_drop_option_cb, this,
			FL_MENU_TOGGLE | (drag_and_drop_config ? FL_MENU_VALUE : 0) | FL_MENU_DIVIDER),
		OS_MENU_ITEM("&Overworld Map Size...", 0, (Fl_Callback *)overworld_map_size_cb, this, 0),
		{},
		OS_SUBMENU("&Help"),
#ifdef __APPLE__
		OS_MENU_ITEM(PROGRAM_NAME " &Help", FL_F + 1, (Fl_Callback *)help_cb, this, 0),
#else
		OS_MENU_ITEM("&Help", FL_F + 1, (Fl_Callback *)help_cb, this, FL_MENU_DIVIDER),
		OS_MENU_ITEM("&About", FL_COMMAND + '/', (Fl_Callback *)about_cb, this, 0),
#endif
		{},
		{}
	};
#ifdef __APPLE__
	// Fix for menu items not working in macOS
	Fl_Menu_Item *menu_items_copy = static_cast<Fl_Menu_Item *>(malloc(sizeof(menu_items)));
	memcpy(menu_items_copy, menu_items, sizeof(menu_items));
	_menu_bar->menu(menu_items_copy);
	// Initialize macOS application menu
	fl_mac_set_about((Fl_Callback *)about_cb, this);
#else
	_menu_bar->copy(menu_items);
#endif

	// Initialize menu bar items
	int first_recent_i = _menu_bar->find_index((Fl_Callback *)open_recent_cb);
	for (int i = 0; i < NUM_RECENT; i++) {
		_recent_mis[i] = const_cast<Fl_Menu_Item *>(&_menu_bar->menu()[first_recent_i + i]);
	}
#define PM_FIND_MENU_ITEM_CB(c) (const_cast<Fl_Menu_Item *>(_menu_bar->find_item((Fl_Callback *)(c))))
	_classic_theme_mi = PM_FIND_MENU_ITEM_CB(classic_theme_cb);
	_aero_theme_mi = PM_FIND_MENU_ITEM_CB(aero_theme_cb);
	_metro_theme_mi = PM_FIND_MENU_ITEM_CB(metro_theme_cb);
	_aqua_theme_mi = PM_FIND_MENU_ITEM_CB(aqua_theme_cb);
	_greybird_theme_mi = PM_FIND_MENU_ITEM_CB(greybird_theme_cb);
	_ocean_theme_mi = PM_FIND_MENU_ITEM_CB(ocean_theme_cb);
	_blue_theme_mi = PM_FIND_MENU_ITEM_CB(blue_theme_cb);
	_olive_theme_mi = PM_FIND_MENU_ITEM_CB(olive_theme_cb);
	_rose_gold_theme_mi = PM_FIND_MENU_ITEM_CB(rose_gold_theme_cb);
	_dark_theme_mi = PM_FIND_MENU_ITEM_CB(dark_theme_cb);
	_brushed_metal_theme_mi = PM_FIND_MENU_ITEM_CB(brushed_metal_theme_cb);
	_high_contrast_theme_mi = PM_FIND_MENU_ITEM_CB(high_contrast_theme_cb);
	_grid_mi = PM_FIND_MENU_ITEM_CB(grid_cb);
	_rulers_mi = PM_FIND_MENU_ITEM_CB(rulers_cb);
	_zoom_mi = PM_FIND_MENU_ITEM_CB(zoom_cb);
	_ids_mi = PM_FIND_MENU_ITEM_CB(ids_cb);
	_hex_mi = PM_FIND_MENU_ITEM_CB(hex_cb);
	_show_priority_mi = PM_FIND_MENU_ITEM_CB(show_priority_cb);
	_gameboy_screen_mi = PM_FIND_MENU_ITEM_CB(gameboy_screen_cb);
	_show_events_mi = PM_FIND_MENU_ITEM_CB(show_events_cb);
	_show_warp_ids_mi = PM_FIND_MENU_ITEM_CB(show_warp_ids_cb);
	_transparent_mi = PM_FIND_MENU_ITEM_CB(transparent_cb);
	_full_screen_mi = PM_FIND_MENU_ITEM_CB(full_screen_cb);
	_morn_mi = PM_FIND_MENU_ITEM_CB(morn_palettes_cb);
	_day_mi = PM_FIND_MENU_ITEM_CB(day_palettes_cb);
	_night_mi = PM_FIND_MENU_ITEM_CB(night_palettes_cb);
	_darkness_mi = PM_FIND_MENU_ITEM_CB(darkness_palettes_cb);
	_indoor_mi = PM_FIND_MENU_ITEM_CB(indoor_palettes_cb);
	_custom_mi = PM_FIND_MENU_ITEM_CB(custom_palettes_cb);
	_blocks_mode_mi = PM_FIND_MENU_ITEM_CB(blocks_mode_cb);
	_events_mode_mi = PM_FIND_MENU_ITEM_CB(events_mode_cb);
	_crystal_palettes_mi = PM_FIND_MENU_ITEM_CB(default_palettes_cb);
	_monochrome_mi = PM_FIND_MENU_ITEM_CB(monochrome_cb);
	_prism_mi = PM_FIND_MENU_ITEM_CB(prism_cb);
	_allow_priority_mi = PM_FIND_MENU_ITEM_CB(allow_priority_cb);
	_allow_256_tiles_mi = PM_FIND_MENU_ITEM_CB(allow_256_tiles_cb);
	_roof_custom_mi = PM_FIND_MENU_ITEM_CB(roof_custom_cb);
	_roof_day_nite_mi = PM_FIND_MENU_ITEM_CB(roof_day_nite_cb);
	_roof_morn_day_nite_mi = PM_FIND_MENU_ITEM_CB(roof_morn_day_nite_cb);
	_roof_day_nite_custom_mi = PM_FIND_MENU_ITEM_CB(roof_day_nite_custom_cb);
	_roof_morn_day_nite_custom_mi = PM_FIND_MENU_ITEM_CB(roof_morn_day_nite_custom_cb);
	_auto_events_mi = PM_FIND_MENU_ITEM_CB(auto_load_events_cb);
	_special_palettes_mi = PM_FIND_MENU_ITEM_CB(auto_load_special_palettes_cb);
	_roof_colors_mi = PM_FIND_MENU_ITEM_CB(auto_load_roof_colors_cb);
	_drag_and_drop_mi = PM_FIND_MENU_ITEM_CB(drag_and_drop_option_cb);
	// Conditional menu items
	_load_event_script_mi = PM_FIND_MENU_ITEM_CB(load_event_script_cb);
	_view_event_script_mi = PM_FIND_MENU_ITEM_CB(view_event_script_cb);
	_reload_event_script_mi = PM_FIND_MENU_ITEM_CB(reload_event_script_cb);
	_unload_event_script_mi = PM_FIND_MENU_ITEM_CB(unload_event_script_cb);
	_load_roof_colors_mi = PM_FIND_MENU_ITEM_CB(load_roof_colors_cb);
	_close_mi = PM_FIND_MENU_ITEM_CB(close_cb);
	_save_mi = PM_FIND_MENU_ITEM_CB(save_cb);
	_save_as_mi = PM_FIND_MENU_ITEM_CB(save_as_cb);
	_save_map_mi = PM_FIND_MENU_ITEM_CB(save_map_cb);
	_save_blockset_mi = PM_FIND_MENU_ITEM_CB(save_metatiles_cb);
	_save_tileset_mi = PM_FIND_MENU_ITEM_CB(save_tileset_cb);
	_save_roof_mi = PM_FIND_MENU_ITEM_CB(save_roof_cb);
	_save_event_script_mi = PM_FIND_MENU_ITEM_CB(save_event_script_cb);
	_print_mi = PM_FIND_MENU_ITEM_CB(print_cb);
	_undo_mi = PM_FIND_MENU_ITEM_CB(undo_cb);
	_redo_mi = PM_FIND_MENU_ITEM_CB(redo_cb);
	_copy_block_mi = PM_FIND_MENU_ITEM_CB(copy_metatile_cb);
	_paste_block_mi = PM_FIND_MENU_ITEM_CB(paste_metatile_cb);
	_swap_block_mi = PM_FIND_MENU_ITEM_CB(swap_metatiles_cb);
	_resize_blockset_mi = PM_FIND_MENU_ITEM_CB(add_sub_cb);
	_resize_map_mi = PM_FIND_MENU_ITEM_CB(resize_cb);
	_change_tileset_mi = PM_FIND_MENU_ITEM_CB(change_tileset_cb);
	_edit_tileset_mi = PM_FIND_MENU_ITEM_CB(edit_tileset_cb);
	_change_roof_mi = PM_FIND_MENU_ITEM_CB(change_roof_cb);
	_edit_roof_mi = PM_FIND_MENU_ITEM_CB(edit_roof_cb);
#undef PM_FIND_MENU_ITEM_CB

#ifndef __APPLE__
	for (int i = 0, md = 0; i < _menu_bar->size(); i++) {
		Fl_Menu_Item *mi = (Fl_Menu_Item *)&_menu_bar->menu()[i];
		if (!mi) { continue; }
		if (md > 0 && mi->label() && !mi->checkbox() && !mi->radio()) {
			Fl_Pixmap *icon = &BLANK_ICON;
			Fl_Multi_Label *ml = new Fl_Multi_Label();
			ml->typea = _FL_IMAGE_LABEL;
			ml->labela = (const char *)icon;
			ml->typeb = FL_NORMAL_LABEL;
			ml->labelb = mi->text;
			mi->image(icon);
			ml->label(mi);
		}
		if (mi->submenu()) { md++; }
		else if (!mi->label()) { md--; }
	}
#endif

	// Configure toolbar buttons

	_new_tb->tooltip("New... (" COMMAND_KEY_PLUS "N)");
	_new_tb->callback((Fl_Callback *)new_cb, this);
	_new_tb->image(NEW_ICON);
	_new_tb->take_focus();

	_open_tb->tooltip("Open... (" COMMAND_KEY_PLUS "O)");
	_open_tb->callback((Fl_Callback *)open_cb, this);
	_open_tb->image(OPEN_ICON);

	_load_event_script_tb->tooltip("Load Event Script... (" COMMAND_KEY_PLUS "A)");
	_load_event_script_tb->callback((Fl_Callback *)load_event_script_cb, this);
	_load_event_script_tb->image(LOAD_ICON);

	_reload_event_script_tb->tooltip("Reload Event Script... (" COMMAND_KEY_PLUS "R)");
	_reload_event_script_tb->callback((Fl_Callback *)reload_event_script_cb, this);
	_reload_event_script_tb->image(RELOAD_ICON);

	_save_tb->tooltip("Save (" COMMAND_KEY_PLUS "S)");
	_save_tb->callback((Fl_Callback *)save_cb, this);
	_save_tb->image(SAVE_ICON);

	_print_tb->tooltip("Print (" COMMAND_KEY_PLUS "P)");
	_print_tb->callback((Fl_Callback *)print_cb, this);
	_print_tb->image(PRINT_ICON);

	_undo_tb->tooltip("Undo (" COMMAND_KEY_PLUS "Z)");
	_undo_tb->callback((Fl_Callback *)undo_cb, this);
	_undo_tb->image(UNDO_ICON);

	_redo_tb->tooltip("Redo (" COMMAND_KEY_PLUS "Y)");
	_redo_tb->callback((Fl_Callback *)redo_cb, this);
	_redo_tb->image(REDO_ICON);

	_grid_tb->tooltip("Grid (" COMMAND_KEY_PLUS "G)");
	_grid_tb->callback((Fl_Callback *)grid_tb_cb, this);
	_grid_tb->image(GRID_ICON);
	_grid_tb->value(grid());

	_rulers_tb->tooltip("Rulers (Ctrl+Shift+R)");
	_rulers_tb->callback((Fl_Callback *)rulers_tb_cb, this);
	_rulers_tb->image(RULERS_ICON);
	_rulers_tb->value(rulers());

	_zoom_tb->tooltip("Zoom (" COMMAND_KEY_PLUS "=)");
	_zoom_tb->callback((Fl_Callback *)zoom_tb_cb, this);
	_zoom_tb->image(ZOOM_ICON);
	_zoom_tb->shortcut(FL_COMMAND + '+');
	_zoom_tb->value(zoom());

	_ids_tb->tooltip("Block IDs (" COMMAND_KEY_PLUS "I)");
	_ids_tb->callback((Fl_Callback *)ids_tb_cb, this);
	_ids_tb->image(IDS_ICON);
	_ids_tb->value(ids());

	_hex_tb->tooltip("Hexadecimal (" COMMAND_KEY_PLUS "$)");
	_hex_tb->callback((Fl_Callback *)hex_tb_cb, this);
	_hex_tb->image(HEX_ICON);
	_hex_tb->shortcut(FL_COMMAND + '$');
	_hex_tb->value(hex());

	_show_priority_tb->tooltip("Show Priority (" COMMAND_SHIFT_KEYS_PLUS "P)");
	_show_priority_tb->callback((Fl_Callback *)show_priority_tb_cb, this);
	_show_priority_tb->image(PRIORITY_ICON);
	_show_priority_tb->value(show_priority());

	_gameboy_screen_tb->tooltip("Game Boy Screen (" COMMAND_SHIFT_KEYS_PLUS "M)");
	_gameboy_screen_tb->callback((Fl_Callback *)gameboy_screen_tb_cb, this);
	_gameboy_screen_tb->image(GAMEBOY_ICON);
	_gameboy_screen_tb->value(gameboy_screen());

	_show_events_tb->tooltip("Show Events (" COMMAND_SHIFT_KEYS_PLUS "R)");
	_show_events_tb->callback((Fl_Callback *)show_events_tb_cb, this);
	_show_events_tb->image(SHOW_ICON);
	_show_events_tb->value(show_events());

	_show_warp_ids_tb->tooltip("Show Warp IDs (Ctrl+#)");
	_show_warp_ids_tb->callback((Fl_Callback *)show_warp_ids_tb_cb, this);
	_show_warp_ids_tb->image(WARP_ICON);
	_show_warp_ids_tb->shortcut(FL_COMMAND + '#');
	_show_warp_ids_tb->value(show_warp_ids());

	_blocks_mode_tb->tooltip("Blocks Mode (" COMMAND_SHIFT_KEYS_PLUS "B)");
	_blocks_mode_tb->callback((Fl_Callback *)blocks_mode_tb_cb, this);
	_blocks_mode_tb->image(BLOCKS_ICON);
	_blocks_mode_tb->value(mode() == Mode::BLOCKS);

	_events_mode_tb->tooltip("Events Mode (" COMMAND_SHIFT_KEYS_PLUS "E)");
	_events_mode_tb->callback((Fl_Callback *)events_mode_tb_cb, this);
	_events_mode_tb->image(EVENTS_ICON);
	_events_mode_tb->value(mode() == Mode::EVENTS);

	_palettes->add("Morn");   // Palettes::MORN
	_palettes->add("Day");    // Palettes::DAY
	_palettes->add("Night");  // Palettes::NITE
	_palettes->add("Dark");   // Palettes::DARKNESS
	_palettes->add("Indoor"); // Palettes::INDOOR
	_palettes->add("Custom"); // Palettes::CUSTOM
	_palettes->value((int)palettes_config);
	_palettes->callback((Fl_Callback *)palettes_cb, this);

	_add_sub_tb->tooltip("Resize Blockset... (" COMMAND_KEY_PLUS "B)");
	_add_sub_tb->callback((Fl_Callback *)add_sub_cb, this);
	_add_sub_tb->image(ADD_SUB_ICON);

	_resize_tb->tooltip("Resize Map... (" COMMAND_KEY_PLUS "E)");
	_resize_tb->callback((Fl_Callback *)resize_cb, this);
	_resize_tb->image(RESIZE_ICON);

#ifdef __APPLE__
	_change_tileset_tb->tooltip("Change Tileset... (" COMMAND_KEY_PLUS "J)");
#else
	_change_tileset_tb->tooltip("Change Tileset... (" COMMAND_KEY_PLUS "H)");
#endif
	_change_tileset_tb->callback((Fl_Callback *)change_tileset_cb, this);
	_change_tileset_tb->image(CHANGE_ICON);

	_edit_tileset_tb->tooltip("Edit Tileset... (" COMMAND_KEY_PLUS "T)");
	_edit_tileset_tb->callback((Fl_Callback *)edit_tileset_cb, this);
	_edit_tileset_tb->image(TILESET_ICON);

#ifdef __APPLE__
	_change_roof_tb->tooltip("Change Roof... (" COMMAND_SHIFT_KEYS_PLUS "J)");
#else
	_change_roof_tb->tooltip("Change Roof... (" COMMAND_SHIFT_KEYS_PLUS "H)");
#endif
	_change_roof_tb->callback((Fl_Callback *)change_roof_cb, this);
	_change_roof_tb->image(CHANGE_ROOF_ICON);

	_edit_roof_tb->tooltip("Edit Roof... (" COMMAND_KEY_PLUS "F)");
	_edit_roof_tb->callback((Fl_Callback *)edit_roof_cb, this);
	_edit_roof_tb->image(ROOF_ICON);

	_load_palettes_tb->tooltip("Load Palettes... (" COMMAND_KEY_PLUS "L)");
	_load_palettes_tb->callback((Fl_Callback *)load_palettes_cb, this);
	_load_palettes_tb->image(LOAD_PALETTES_ICON);

	_edit_current_palettes_tb->tooltip("Edit Current Palettes... (" COMMAND_SHIFT_KEYS_PLUS "L)");
	_edit_current_palettes_tb->callback((Fl_Callback *)edit_current_palettes_cb, this);
	_edit_current_palettes_tb->image(PALETTES_ICON);

	// Configure dialogs

	_new_dir_chooser->title("Choose Project Directory");

	_blk_open_chooser->title("Open Map");
	_blk_open_chooser->filter("BLK Files\t*.{blk,ablk}\nMAP Files\t*.map\n");

	_blk_save_chooser->title("Save Map");
	_blk_save_chooser->filter("BLK Files\t*.blk\n");
	_blk_save_chooser->options(Fl_Native_File_Chooser::Option::SAVEAS_CONFIRM);
	_blk_save_chooser->preset_file("NewMap.blk");

	_pal_load_chooser->title("Open Palettes");
	_pal_load_chooser->filter("PAL Files\t*.pal\n");

	_pal_save_chooser->title("Save Palettes");
	_pal_save_chooser->filter("PAL Files\t*.pal\n");
	_pal_save_chooser->options(Fl_Native_File_Chooser::Option::SAVEAS_CONFIRM);
	_pal_save_chooser->preset_file("palettes.pal");

	_roof_chooser->title("Open Roof Tiles");
	_roof_chooser->filter("PNG Files\t*.png\n2BPP Files\t*.2bpp\n");

	_asm_chooser->title("Open Event Script");
	_asm_chooser->filter("ASM Files\t*.asm\nINC Files\t*.inc\n");

	_png_chooser->title("Print Screenshot");
	_png_chooser->filter("PNG Files\t*.png\n");
	_png_chooser->options(Fl_Native_File_Chooser::Option::SAVEAS_CONFIRM);
	_png_chooser->preset_file("screenshot.png");

	_error_dialog->width_range(280, 700);
	_warning_dialog->width_range(280, 700);
	_success_dialog->width_range(280, 700);
	_unsaved_dialog->width_range(280, 700);

	_print_options_dialog->grid(Config::print_grid());
	_print_options_dialog->ids(Config::print_ids());
	_print_options_dialog->priority(Config::print_priority());
	_print_options_dialog->events(Config::print_events());
	_print_options_dialog->warp_ids(Config::print_warp_ids());

	std::string subject(PROGRAM_NAME " " PROGRAM_VERSION_STRING), message(
		"Copyright \xc2\xa9 " CURRENT_YEAR " " PROGRAM_AUTHOR ".\n"
		"\n"
		"Source code is available at:\n"
		"https://github.com/Rangi42/polished-map\n"
		"\n"
		"Some icons by Yusuke Kamiyamane."
	);
	_about_dialog->subject(subject);
	_about_dialog->message(message);
	_about_dialog->width_range(280, 700);

	_help_window->content(
#include "help.html" // a C++11 raw string literal
		);

	update_icons();
	update_recent_maps();
	update_active_controls();
}

Main_Window::~Main_Window() {
	delete _menu_bar; // includes menu items
	delete _toolbar; // includes toolbar buttons
	delete _sidebar; // includes metatiles
	delete _status_bar; // includes status bar fields
	delete _map_scroll; // includes map and blocks
	delete _dnd_receiver;
	delete _blk_open_chooser;
	delete _blk_save_chooser;
	delete _pal_load_chooser;
	delete _pal_save_chooser;
	delete _png_chooser;
	delete _error_dialog;
	delete _warning_dialog;
	delete _success_dialog;
	delete _unsaved_dialog;
	delete _about_dialog;
	delete _map_options_dialog;
	delete _tileset_options_dialog;
	delete _roof_options_dialog;
	delete _event_options_dialog;
	delete _print_options_dialog;
	delete _resize_dialog;
	delete _add_sub_dialog;
	delete _help_window;
	delete _block_window;
	delete _tileset_window;
	delete _roof_window;
	delete _palette_window;
	delete _monochrome_palette_window;
}

void Main_Window::show() {
	Fl_Overlay_Window::show();
#ifdef _WIN32
	// Fix for 16x16 icon from <http://www.fltk.org/str.php?L925>
	HWND hwnd = fl_xid(this);
	HANDLE big_icon = LoadImage(GetModuleHandle(0), MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON,
		GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CXICON), 0);
	SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(big_icon));
	HANDLE small_icon = LoadImage(GetModuleHandle(0), MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON,
		GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CXSMICON), 0);
	SendMessage(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon));
#elif defined(__LINUX__)
	// Fix for X11 icon alpha mask <https://www.mail-archive.com/fltk@easysw.com/msg02863.html>
	XWMHints *hints = XGetWMHints(fl_display, fl_xid(this));
	hints->flags |= IconMaskHint;
	hints->icon_mask = _icon_mask;
	XSetWMHints(fl_display, fl_xid(this), hints);
	XFree(hints);
#endif
}

bool Main_Window::maximized() const {
#ifdef _WIN32
	WINDOWPLACEMENT wp;
	wp.length = sizeof(wp);
	if (!GetWindowPlacement(fl_xid(this), &wp)) { return false; }
	return wp.showCmd == SW_MAXIMIZE;
#elif defined(__LINUX__)
	Atom wmState = XInternAtom(fl_display, "_NET_WM_STATE", True);
	Atom actual;
	int format;
	unsigned long numItems, bytesAfter;
	unsigned char *properties = NULL;
	int result = XGetWindowProperty(fl_display, fl_xid(this), wmState, 0, 1024, False, AnyPropertyType, &actual, &format,
		&numItems, &bytesAfter, &properties);
	int numMax = 0;
	if (result == Success && format == 32 && properties) {
		Atom maxVert = XInternAtom(fl_display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
		Atom maxHorz = XInternAtom(fl_display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
		for (unsigned long i = 0; i < numItems; i++) {
			Atom property = ((Atom *)properties)[i];
			if (property == maxVert || property == maxHorz) {
				numMax++;
			}
		}
		XFree(properties);
	}
	return numMax == 2;
	return false;
#endif
// TODO: Implement for macOS
return false;
}

void Main_Window::maximize() {
#ifdef _WIN32
	ShowWindow(fl_xid(this), SW_MAXIMIZE);
#elif defined(__LINUX__)
	XEvent event;
	memset(&event, 0, sizeof(event));
	event.xclient.type = ClientMessage;
	event.xclient.window = fl_xid(this);
	event.xclient.message_type = XInternAtom(fl_display, "_NET_WM_STATE", False);
	event.xclient.format = 32;
	event.xclient.data.l[0] = 1;
	event.xclient.data.l[1] = XInternAtom(fl_display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
	event.xclient.data.l[2] = XInternAtom(fl_display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
	event.xclient.data.l[3] = 1;
	XSendEvent(fl_display, DefaultRootWindow(fl_display), False, SubstructureNotifyMask | SubstructureNotifyMask, &event);
#endif
	// TODO: Implement for macOS
}

void Main_Window::apply_transparency() {
	double alpha = transparent() ? 0.75 : 1.0;
#ifdef _WIN32
	HWND hwnd = fl_xid(this);
	LONG_PTR exstyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
	if (!(exstyle & WS_EX_LAYERED)) {
		SetWindowLongPtr(hwnd, GWL_EXSTYLE, exstyle | WS_EX_LAYERED);
	}
	SetLayeredWindowAttributes(hwnd, 0, (BYTE)(alpha * 0xFF), LWA_ALPHA);
#elif defined(__LINUX__)
	Atom atom = XInternAtom(fl_display, "_NET_WM_WINDOW_OPACITY", False);
	uint32_t opacity = (uint32_t)(UINT32_MAX * alpha);
	XChangeProperty(fl_display, fl_xid(this), atom, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&opacity, 1);
#endif
	// TODO: Implement for macOS
}

bool Main_Window::unsaved() const {
	return _map.modified() || _map_events.modified() || _metatileset.modified() ||
		_metatileset.const_tileset().modified() || _metatileset.const_tileset().modified_roof();
}

const char *Main_Window::modified_filename() {
	if (_map.modified()) {
		if (_blk_file.empty()) { return NEW_MAP_NAME; }
		return fl_filename_name(_blk_file.c_str());
	}
	if (_map_events.modified()) {
		if (_asm_file.empty()) { return NEW_MAP_NAME; }
		return fl_filename_name(_asm_file.c_str());
	}
	static char buffer[FL_PATH_MAX] = {};
	const Tileset &tileset = _metatileset.const_tileset();
	if (tileset.modified()) {
		Config::tileset_path(buffer, _directory.c_str(), tileset.name());
	}
	else if (tileset.modified_roof()) {
		Config::roof_path(buffer, _directory.c_str(), tileset.roof_name());
	}
	else {
		Config::metatileset_path(buffer, _directory.c_str(), _metatileset.tileset().name());
	}
	return fl_filename_name(buffer);
}

void Main_Window::draw_overlay() {
	if (_gameboy_screen) {
		int sw = _map_scroll->w() - (_map_scroll->has_y_scroll() ? Fl::scrollbar_size() : 0);
		int sh = _map_scroll->h() - (_map_scroll->has_x_scroll() ? Fl::scrollbar_size() : 0);
		fl_push_clip(_map_scroll->x(), _map_scroll->y(), sw, sh);
		Game_Boy_Screen::draw();
		fl_pop_clip();
	}
}

int Main_Window::handle(int event) {
	int key = 0;
	switch (event) {
	case FL_FOCUS:
	case FL_UNFOCUS:
		return 1;
	case FL_SHORTCUT:
		key = Fl::event_key();
		//if (key & FL_KP == FL_KP) { key -= FL_KP; } // normalize numpad keys into digits
		if (handle_hotkey(key)) { return 1; }
		[[fallthrough]];
	default:
		return Fl_Overlay_Window::handle(event);
	}
}

int Main_Window::handle_hotkey(int key) {
	if (!_map.size()) { return 0; }
	if ((key & FL_KP) == FL_KP) { key -= FL_KP; } // normalize numpad keys into digits
	if (key < '0' || key > '9') { return 0; } // 0-9 keys only
	if (Fl::event_ctrl() && Fl::event_shift()) {
		// Ctrl+Shift+0-9 unassign the hotkey
		auto s = hotkey_metatile(key);
		if (s == no_metatile()) { return 0; }
		uint8_t id = s->second;
		_hotkey_metatiles.erase(key);
		_metatile_hotkeys.erase(id);
		redraw();
	}
	else if (Fl::event_ctrl()) {
		// Ctrl+0-9 assign the selected metatile to the hotkey
		if (!_selected) { return 0; }
		uint8_t id = _selected->id();
		auto sk = metatile_hotkey(id);
		if (sk != no_hotkey()) {
			// Unassign the metatile's previous key
			int prev_key = sk->second;
			_hotkey_metatiles.erase(prev_key);
		}
		auto sm = hotkey_metatile(key);
		if (sm != no_metatile()) {
			// Unassign the key from the previous metatile
			uint8_t prev_id = sm->second;
			_metatile_hotkeys.erase(prev_id);
		}
		_hotkey_metatiles[key] = id;
		_metatile_hotkeys[id] = key;
		redraw();
	}
	else {
		// 0-9 select the metatile assigned to the hotkey
		auto s = hotkey_metatile(key);
		if (s == no_metatile()) { return 0; }
		uint8_t id = s->second;
		if (id >= _metatileset.size()) { return 0; }
		select_metatile(_metatile_buttons[id]);
		return 1;
	}
	return 0;
}

void Main_Window::flood_fill(Block *b, uint8_t f, uint8_t t) {
	if (f == t) { return; }
	std::queue<size_t> queue;
	uint8_t w = _map.width(), h = _map.height();
	uint8_t row = b->row(), col = b->col();
	size_t i = row * w + col;
	queue.push(i);
	while (!queue.empty()) {
		size_t j = queue.front();
		queue.pop();
		Block *bi = _map.block(j);
		if (bi->id() != f) { continue; }
		bi->id(t); // fill
		uint8_t r = bi->row(), c = bi->col();
		if (c > 0) { queue.push(j-1); } // left
		if (c < w - 1) { queue.push(j+1); } // right
		if (r > 0) { queue.push(j-w); } // up
		if (r < h - 1) { queue.push(j+w); } // down
	}
}

void Main_Window::substitute_block(uint8_t f, uint8_t t) {
	size_t n = _map.size();
	for (size_t i = 0; i < n; i++) {
		Block *b = _map.block(i);
		if (b->id() == f) {
			b->id(t);
		}
	}
}

void Main_Window::swap_blocks(uint8_t f, uint8_t t) {
	if (f == t) { return; }
	size_t n = _map.size();
	for (size_t i = 0; i < n; i++) {
		Block *b = _map.block(i);
		if (b->id() == f) {
			b->id(t);
		}
		else if (b->id() == t) {
			b->id(f);
		}
	}
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


#pragma warning(pop)
