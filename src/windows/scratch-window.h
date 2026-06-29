#ifndef SCRATCH_WINDOW_H
#define SCRATCH_WINDOW_H

#pragma warning(push, 0)
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#pragma warning(pop)

#include "widgets.h"

class Main_Window;

class Scratch_Double_Window : public Fl_Double_Window {
public:
	Scratch_Double_Window(int x, int y, int w, int h, const char *l = NULL);
	int handle(int event);
};

// A non-modal window hosting the tileset scratch canvas. It only owns GUI; the grid data and
// undo history live in Main_Window::_scratch_map (which this class is a friend of). The Block
// widgets render through the Main_Window the same way the real map does, so zoom/grid/ids
// settings are honored automatically.
class Scratch_Window {
private:
	int _dx, _dy;
	bool _built;
	bool _positioned;
	Main_Window *_mw;
	Scratch_Double_Window *_window;
	Toolbar *_toolbar;
	Toolbar_Button *_undo_tb, *_redo_tb, *_resize_tb;
	Workspace *_scratch_scroll;
	Fl_Group *_scratch_group;
	friend class Scratch_Double_Window;
public:
	Scratch_Window(int x, int y);
	~Scratch_Window();
private:
	void initialize(void);
public:
	inline bool built(void) const { return _built; }
	inline bool shown(void) const { return _window && _window->shown(); }
	void build(Main_Window *mw);
	void clear_canvas(void);
	void hide_window(void);
	void show(const Fl_Widget *p);
	void redraw(void);
	void sync_zoom(void);
private:
	void sync_layout(void);
	void do_undo(void);
	void do_redo(void);
	void do_resize(void);
	static void undo_cb(Fl_Widget *w, Scratch_Window *sw);
	static void redo_cb(Fl_Widget *w, Scratch_Window *sw);
	static void resize_cb(Fl_Widget *w, Scratch_Window *sw);
	static void close_cb(Fl_Widget *w, Scratch_Window *sw);
};

#endif
