#include "prism-palettes.h"

// Color index tables for Prism palette system
// Format: [environment_table][time_of_day_row][palette_slot] = index into bg.pal
// Copied verbatim from pokeprism engine/color.asm (.OutdoorColors / .IndoorColors
// / .DungeonColors). PERM_5 uses .Perm5Colors which is identical to .DungeonColors.
static const uint8_t COLOR_TABLES[3][NUM_HUES][PRISM_NUM_SLOTS] = {
	{ // outdoor (unused / TOWN / ROUTE)
		{0x00, 0x01, 0x02, 0x28, 0x04, 0x05, 0x06, 0x07}, // morn
		{0x08, 0x09, 0x0a, 0x28, 0x0c, 0x0d, 0x0e, 0x0f}, // day
		{0x10, 0x11, 0x12, 0x29, 0x14, 0x15, 0x16, 0x17}, // nite
		{0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f}, // dark
	},
	{ // indoor (INDOOR / GATE)
		{0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x07}, // morn
		{0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x07}, // day
		{0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x07}, // nite
		{0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x07}, // dark
	},
	{ // dungeon (CAVE / PERM_5 / DUNGEON)
		{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}, // morn
		{0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f}, // day
		{0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17}, // nite
		{0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f}, // dark
	},
};

// Constructor initializes empty palette data
Prism_Palettes::Prism_Palettes() : _bg(), _loaded(false) {}

// Clear all loaded palette data
void Prism_Palettes::clear() {
	_bg.clear();
	_loaded = false;
}

// Load Prism background palettes from a .pal file
bool Prism_Palettes::load(const char *bg_pal_path) {
	_bg = Color::parse_palettes(bg_pal_path);
	_loaded = !_bg.empty();
	return _loaded;
}

// Determine which color table to use based on map environment
// Returns: 0=outdoor, 1=indoor, 2=dungeon
int Prism_Palettes::table_for_environment(const std::string &environment) {
	// permission constants: TOWN=1, ROUTE=2, INDOOR=3, CAVE=4, PERM_5=5, GATE=6, DUNGEON=7
	if (environment == "INDOOR" || environment == "GATE" ||
		environment == "3" || environment == "6") {
		return 1; // indoor
	}
	if (environment == "CAVE" || environment == "PERM_5" || environment == "DUNGEON" ||
		environment == "4" || environment == "5" || environment == "7") {
		return 2; // dungeon
	}
	return 0; // outdoor
}

// Get the time-of-day row index for palette selection
// MORN=0, DAY=1, NITE=2, DARKNESS=3
// For INDOOR/CUSTOM environments, defaults to DAY
int Prism_Palettes::row_for_palettes(Palettes pal) {
	int row = (int)pal; // MORN=0, DAY=1, NITE=2, DARKNESS=3
	if (row < 0 || row > 3) { row = (int)Palettes::DAY; } // INDOOR/CUSTOM have no row
	return row;
}

// Resolve 8 palette slots for the given environment and time of day
// Each slot contains 4 hues with 3 color channels (RGB) converted to 8-bit values
void Prism_Palettes::resolve(const std::string &environment, Palettes tod, Prism_Pal out[PRISM_NUM_SLOTS]) const {
	int table = table_for_environment(environment);
	int row = row_for_palettes(tod);
	for (int slot = 0; slot < PRISM_NUM_SLOTS; slot++) {
		size_t idx = COLOR_TABLES[table][row][slot];
		for (int h = 0; h < NUM_HUES; h++) {
			if (idx < _bg.size()) {
				const ColorArray &c = _bg[idx][h];
				out[slot][h][0] = RGB5C(c[0]);
				out[slot][h][1] = RGB5C(c[1]);
				out[slot][h][2] = RGB5C(c[2]);
			}
			else {
				out[slot][h][0] = out[slot][h][1] = out[slot][h][2] = Color::hue_mono((Hue)h);
			}
		}
	}
}
