#pragma warning(push, 0)
#include <FL/Fl.H>
#pragma warning(pop)

#include "themes.h"
#include "icons.h"
#include "map.h"
#include "main-window.h"
#include "scratch-window.h"

Scratch_Double_Window::Scratch_Double_Window(int x, int y, int w, int h, const char *l) : Fl_Double_Window(x, y, w, h, l) {}

int Scratch_Double_Window::handle(int event) {
	Scratch_Window *sw = (Scratch_Window *)user_data();
	if (event == FL_KEYBOARD && sw && Fl::event_command()) {
		switch (Fl::event_key()) {
		case 'z':
			sw->do_undo();
			return 1;
		case 'y':
			sw->do_redo();
			return 1;
		}
	}
	return Fl_Double_Window::handle(event);
}

Scratch_Window::Scratch_Window(int x, int y) : _dx(x), _dy(y), _built(false), _positioned(false), _mw(NULL), _window(NULL),
	_toolbar(NULL), _undo_tb(NULL), _redo_tb(NULL), _resize_tb(NULL), _scratch_scroll(NULL), _scratch_group(NULL) {}

Scratch_Window::~Scratch_Window() {
	delete _window;
}

void Scratch_Window::initialize() {
	if (_window) { return; }
	Fl_Group *prev_current = Fl_Group::current();
	Fl_Group::current(NULL);
	// Populate window
	_window = new Scratch_Double_Window(_dx, _dy, 640, 480, "Scratch Canvas");
	_toolbar = new Toolbar(0, 0, 640, 26);
	_undo_tb = new Toolbar_Button(0, 0, 24, 24);
	_redo_tb = new Toolbar_Button(0, 0, 24, 24);
	_resize_tb = new Toolbar_Button(0, 0, 24, 24);
	_toolbar->end();
	_window->begin();
	int ty = _toolbar->h();
	_scratch_scroll = new Workspace(0, ty, 640, 480 - ty);
	_scratch_scroll->type(Fl_Scroll::BOTH);
	_scratch_group = new Fl_Group(0, ty, 0, 0);
	_scratch_group->end();
	_scratch_scroll->end();
	_window->end();
	// Initialize window
	_window->box(OS_BG_BOX);
	// Non-modal keeps the canvas floating above the main window without blocking it, so the user
	// can paint on the canvas and click straight back into the map.
	_window->set_non_modal();
	_window->resizable(_scratch_scroll);
	_window->callback((Fl_Callback *)close_cb, this);
	_window->user_data(this);
	// Initialize toolbar buttons
	_undo_tb->tooltip("Undo (" COMMAND_KEY_PLUS "Z)");
	_undo_tb->callback((Fl_Callback *)undo_cb, this);
	_undo_tb->image(UNDO_ICON);
	_redo_tb->tooltip("Redo (" COMMAND_KEY_PLUS "Y)");
	_redo_tb->callback((Fl_Callback *)redo_cb, this);
	_redo_tb->image(REDO_ICON);
	_resize_tb->tooltip("Resize...");
	_resize_tb->callback((Fl_Callback *)resize_cb, this);
	_resize_tb->image(RESIZE_ICON);
	Fl_Group::current(prev_current);
}

void Scratch_Window::build(Main_Window *mw) {
	initialize();
	_mw = mw;
	Map &map = mw->_scratch_map;
	int ms = mw->metatile_size();
	uint8_t w = map.width(), h = map.height();
	_scratch_group->begin();
	for (uint8_t y = 0; y < h; y++) {
		for (uint8_t x = 0; x < w; x++) {
			Block *b = map.block(x, y);
			b->user_data(mw);
			b->callback((Fl_Callback *)Main_Window::change_scratch_block_cb, mw);
			_scratch_group->add(b);
		}
	}
	_scratch_group->end();
	// resize_blocks offsets every block by MAP_MARGIN tiles, so pad the group with EVENT_MARGIN
	// (same as the real map) and scroll past the top-left margin to land on the content.
	_scratch_group->size(ms * ((int)w + EVENT_MARGIN), ms * ((int)h + EVENT_MARGIN));
	map.resize_blocks(_scratch_group->x(), _scratch_group->y(), ms);
	_scratch_scroll->init_sizes();
	_scratch_scroll->contents(_scratch_group->w(), _scratch_group->h());
	_scratch_scroll->scroll_to(MAP_MARGIN * ms, MAP_MARGIN * ms);
	_built = true;
	_window->redraw();
}

void Scratch_Window::clear_canvas() {
	if (_scratch_group) {
		_scratch_group->clear(); // deletes the Block widgets (Map keeps only the pointer array)
		_scratch_group->size(0, 0);
	}
	_built = false;
}

void Scratch_Window::hide_window() {
	if (_window) { _window->hide(); }
}

void Scratch_Window::show(const Fl_Widget *p) {
	initialize();
	// Only center on the very first open; afterward the window keeps wherever the user left it
	// (hidden windows retain their position), so the Ctrl+K toggle doesn't recenter it.
	if (!_positioned) {
		int x = p->x() + (p->w() - _window->w()) / 2;
		int y = p->y() + (p->h() - _window->h()) / 2;
		_window->position(x, y);
		_positioned = true;
	}
	// Re-sync block geometry to the current zoom each time the window is shown
	sync_layout();
	_window->show();
}

void Scratch_Window::sync_layout() {
	if (!_mw || !_mw->_scratch_map.size()) { return; }
	int ms = _mw->metatile_size();
	uint8_t w = _mw->_scratch_map.width(), h = _mw->_scratch_map.height();
	_scratch_group->size(ms * ((int)w + EVENT_MARGIN), ms * ((int)h + EVENT_MARGIN));
	_mw->_scratch_map.resize_blocks(_scratch_group->x(), _scratch_group->y(), ms);
	_scratch_scroll->init_sizes();
	_scratch_scroll->contents(_scratch_group->w(), _scratch_group->h());
}

void Scratch_Window::sync_zoom() {
	if (!shown()) { return; }
	sync_layout();
	_window->redraw();
}

void Scratch_Window::redraw() {
	if (_scratch_group) { _scratch_group->redraw(); }
	if (_scratch_scroll) { _scratch_scroll->redraw(); }
}

void Scratch_Window::do_undo() {
	if (!_mw || !_mw->_scratch_map.size()) { return; }
	_mw->_scratch_map.undo();
	redraw();
}

void Scratch_Window::do_redo() {
	if (!_mw || !_mw->_scratch_map.size()) { return; }
	_mw->_scratch_map.redo();
	redraw();
}

void Scratch_Window::do_resize() {
	if (!_mw || !_mw->_scratch_map.size()) { return; }
	_mw->_resize_dialog->map_size(_mw->_scratch_map.width(), _mw->_scratch_map.height());
	_mw->_resize_dialog->show(_window);
	if (_mw->_resize_dialog->canceled()) { return; }
	int nw = _mw->_resize_dialog->map_width(), nh = _mw->_resize_dialog->map_height();
	if (nw != _mw->_scratch_map.width() || nh != _mw->_scratch_map.height()) {
		_mw->resize_scratch(nw, nh);
	}
}

void Scratch_Window::undo_cb(Fl_Widget *, Scratch_Window *sw) {
	sw->do_undo();
}

void Scratch_Window::redo_cb(Fl_Widget *, Scratch_Window *sw) {
	sw->do_redo();
}

void Scratch_Window::resize_cb(Fl_Widget *, Scratch_Window *sw) {
	sw->do_resize();
}

void Scratch_Window::close_cb(Fl_Widget *, Scratch_Window *sw) {
	if (sw->_mw) { sw->_mw->save_scratch(); }
	sw->_window->hide();
}
