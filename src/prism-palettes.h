#ifndef PRISM_PALETTES_H
#define PRISM_PALETTES_H

#include <string>

#include "utils.h"
#include "colors.h"

// Pokemon Prism BG palette system (engine/color.asm + tilesets/bg.pal).
//
// Prism assigns a palette slot 0-7 to each tile-position in a block (via the
// binary *_attributes.bin), then resolves that slot to four real colors by:
//   1. picking a color-index table from the map's permission (outdoor/indoor/dungeon),
//   2. picking a row from the time-of-day (morn/day/nite/dark),
//   3. indexing tilesets/bg.pal with the resulting BG palette index.
// bg.pal colors are final (no hue/desaturation transform).

#define PRISM_NUM_SLOTS 8

// One resolved palette: 4 hues x 3 channels, 8-bit RGB, indexed by Hue.
typedef uchar Prism_Pal[NUM_HUES][NUM_CHANNELS];

class Prism_Palettes {
private:
	PalVec _bg; // parsed bg.pal (5-bit values), indexed [palette][Hue]
	bool _loaded;
public:
	Prism_Palettes();
	bool load(const char *bg_pal_path); // reuses Color::parse_palettes
	inline bool loaded(void) const { return _loaded; }
	void clear(void);
	// outdoor=0, indoor=1, dungeon=2 (engine/color.asm .TilesetColorsPointers)
	static int table_for_environment(const std::string &environment);
	// morn=0, day=1, nite=2, dark=3
	static int row_for_palettes(Palettes pal);
	// fill 8 resolved palettes (8-bit RGB) for the given environment + time of day
	void resolve(const std::string &environment, Palettes tod, Prism_Pal out[PRISM_NUM_SLOTS]) const;
};

#endif
