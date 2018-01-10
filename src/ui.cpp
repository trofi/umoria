// Copyright (c) 1981-86 Robert A. Koeneke
// Copyright (c) 1987-94 James E. Wilson
//
// This work is free software released under the GNU General Public License
// version 2.0, and comes with ABSOLUTELY NO WARRANTY.
//
// See LICENSE and AUTHORS for more information.

#include "headers.h"

static const char *stat_names[] = {"STR : ", "INT : ", "WIS : ", "DEX : ", "CON : ", "CHR : ",};
#define BLANK_LENGTH 24
static char blank_string[] = "                        ";

// Calculates current boundaries -RAK-
static void panelBounds() {
    dg.panel.top = dg.panel.row * (SCREEN_HEIGHT / 2);
    dg.panel.bottom = dg.panel.top + SCREEN_HEIGHT - 1;
    dg.panel.row_prt = dg.panel.top - 1;
    dg.panel.left = dg.panel.col * (SCREEN_WIDTH / 2);
    dg.panel.right = dg.panel.left + SCREEN_WIDTH - 1;
    dg.panel.col_prt = dg.panel.left - 13;
}

// Given an row (y) and col (x), this routine detects -RAK-
// when a move off the screen has occurred and figures new borders.
// `force` forces the panel bounds to be recalculated, useful for 'W'here.
bool coordOutsidePanel(Coord_t coord, bool force) {
    int row = dg.panel.row;
    int col = dg.panel.col;

    if (force || coord.y < dg.panel.top + 2 || coord.y > dg.panel.bottom - 2) {
        row = (coord.y - SCREEN_HEIGHT / 4) / (SCREEN_HEIGHT / 2);

        if (row > dg.panel.max_rows) {
            row = dg.panel.max_rows;
        } else if (row < 0) {
            row = 0;
        }
    }

    if (force || coord.x < dg.panel.left + 3 || coord.x > dg.panel.right - 3) {
        col = ((coord.x - SCREEN_WIDTH / 4) / (SCREEN_WIDTH / 2));
        if (col > dg.panel.max_cols) {
            col = dg.panel.max_cols;
        } else if (col < 0) {
            col = 0;
        }
    }

    if (row != dg.panel.row || col != dg.panel.col) {
        dg.panel.row = row;
        dg.panel.col = col;
        panelBounds();

        // stop movement if any
        if (config.find_bound) {
            playerEndRunning();
        }

        // Yes, the coordinates are beyond the current panel boundary
        return true;
    }

    return false;
}

// Is the given coordinate within the screen panel boundaries -RAK-
bool coordInsidePanel(Coord_t coord) {
    bool valid_y = coord.y >= dg.panel.top && coord.y <= dg.panel.bottom;
    bool valid_x = coord.x >= dg.panel.left && coord.x <= dg.panel.right;

    return valid_y && valid_x;
}

// Prints the map of the dungeon -RAK-
void drawDungeonPanel() {
    int line = 1;

    // Top to bottom
    for (int y = dg.panel.top; y <= dg.panel.bottom; y++) {
        eraseLine(Coord_t{line++, 13});

        // Left to right
        for (int x = dg.panel.left; x <= dg.panel.right; x++) {
            char ch = caveGetTileSymbol(Coord_t{y, x});
            if (ch != ' ') {
                panelPutTile(ch, Coord_t{y, x});
            }
        }
    }
}

// Draws entire screen -RAK-
void drawCavePanel() {
    clearScreen();
    printCharacterStatsBlock();
    drawDungeonPanel();
    printCharacterCurrentDepth();
}

// We need to reset the view of things. -CJS-
void dungeonResetView() {
    Tile_t const &tile = dg.floor[py.row][py.col];

    // Check for new panel
    if (coordOutsidePanel(Coord_t{py.row, py.col}, false)) {
        drawDungeonPanel();
    }

    // Move the light source
    dungeonMoveCharacterLight(Coord_t{py.row, py.col}, Coord_t{py.row, py.col});

    // A room of light should be lit.
    if (tile.feature_id == TILE_LIGHT_FLOOR) {
        if (py.flags.blind < 1 && !tile.permanent_light) {
            dungeonLightRoom(Coord_t{py.row, py.col});
        }
        return;
    }

    // In doorway of light-room?
    if (tile.perma_lit_room && py.flags.blind < 1) {
        for (int i = py.row - 1; i <= py.row + 1; i++) {
            for (int j = py.col - 1; j <= py.col + 1; j++) {
                if (dg.floor[i][j].feature_id == TILE_LIGHT_FLOOR && !dg.floor[i][j].permanent_light) {
                    dungeonLightRoom(Coord_t{i, j});
                }
            }
        }
    }
}

// Converts stat num into string -RAK-
void statsAsString(uint8_t stat, char *stat_string) {
    if (stat <= 18) {
        (void) sprintf(stat_string, "%6d", stat);
        return;
    }

    int part1 = 18;
    int part2 = stat - 18;

    if (part2 == 100) {
        (void) strcpy(stat_string, "18/100");
        return;
    }

    (void) sprintf(stat_string, " %2d/%02d", part1, part2);
}

// Print character stat in given row, column -RAK-
void displayCharacterStats(int stat) {
    char text[7];
    statsAsString(py.stats.used[stat], text);
    putString(stat_names[stat], Coord_t{6 + stat, STAT_COLUMN});
    putString(text, Coord_t{6 + stat, STAT_COLUMN + 6});
}

// Print character info in given row, column -RAK-
// The longest title is 13 characters, so only pad to 13
static void printCharacterInfoInField(const char *info, int row, int column) {
    // blank out the current field space
    putString(&blank_string[BLANK_LENGTH - 13], Coord_t{row, column});

    putString(info, Coord_t{row, column});
}

// Print long number with header at given row, column
static void printHeaderLongNumber(const char *header, int32_t num, int row, int column) {
    vtype_t str = {'\0'};
    (void) sprintf(str, "%s: %6d", header, num);
    putString(str, Coord_t{row, column});
}

// Print long number (7 digits of space) with header at given row, column
static void printHeaderLongNumber7Spaces(const char *header, int32_t num, int row, int column) {
    vtype_t str = {'\0'};
    (void) sprintf(str, "%s: %7d", header, num);
    putString(str, Coord_t{row, column});
}

// Print number with header at given row, column -RAK-
static void printHeaderNumber(const char *header, int num, int row, int column) {
    vtype_t str = {'\0'};
    (void) sprintf(str, "%s: %6d", header, num);
    putString(str, Coord_t{row, column});
}

// Print long number at given row, column
static void printLongNumber(int32_t num, int row, int column) {
    vtype_t str = {'\0'};
    (void) sprintf(str, "%6d", num);
    putString(str, Coord_t{row, column});
}

// Print number at given row, column -RAK-
static void printNumber(int num, int row, int column) {
    vtype_t str = {'\0'};
    (void) sprintf(str, "%6d", num);
    putString(str, Coord_t{row, column});
}

// Prints title of character -RAK-
void printCharacterTitle() {
    printCharacterInfoInField(playerRankTitle(), 4, STAT_COLUMN);
}

// Prints level -RAK-
void printCharacterLevel() {
    printNumber((int) py.misc.level, 13, STAT_COLUMN + 6);
}

// Prints players current mana points. -RAK-
void printCharacterCurrentMana() {
    printNumber(py.misc.current_mana, 15, STAT_COLUMN + 6);
}

// Prints Max hit points -RAK-
void printCharacterMaxHitPoints() {
    printNumber(py.misc.max_hp, 16, STAT_COLUMN + 6);
}

// Prints players current hit points -RAK-
void printCharacterCurrentHitPoints() {
    printNumber(py.misc.current_hp, 17, STAT_COLUMN + 6);
}

// prints current AC -RAK-
void printCharacterCurrentArmorClass() {
    printNumber(py.misc.display_ac, 19, STAT_COLUMN + 6);
}

// Prints current gold -RAK-
void printCharacterGoldValue() {
    printLongNumber(py.misc.au, 20, STAT_COLUMN + 6);
}

// Prints depth in stat area -RAK-
void printCharacterCurrentDepth() {
    vtype_t depths = {'\0'};

    int depth = dg.current_level * 50;

    if (depth == 0) {
        (void) strcpy(depths, "Town level");
    } else {
        (void) sprintf(depths, "%d feet", depth);
    }

    putStringClearToEOL(depths, Coord_t{23, 65});
}

// Prints status of hunger -RAK-
void printCharacterHungerStatus() {
    if ((PY_WEAK & py.flags.status) != 0u) {
        putString("Weak  ", Coord_t{23, 0});
    } else if ((PY_HUNGRY & py.flags.status) != 0u) {
        putString("Hungry", Coord_t{23, 0});
    } else {
        putString(&blank_string[BLANK_LENGTH - 6], Coord_t{23, 0});
    }
}

// Prints Blind status -RAK-
void printCharacterBlindStatus() {
    if ((PY_BLIND & py.flags.status) != 0u) {
        putString("Blind", Coord_t{23, 7});
    } else {
        putString(&blank_string[BLANK_LENGTH - 5], Coord_t{23, 7});
    }
}

// Prints Confusion status -RAK-
void printCharacterConfusedState() {
    if ((PY_CONFUSED & py.flags.status) != 0u) {
        putString("Confused", Coord_t{23, 13});
    } else {
        putString(&blank_string[BLANK_LENGTH - 8], Coord_t{23, 13});
    }
}

// Prints Fear status -RAK-
void printCharacterFearState() {
    if ((PY_FEAR & py.flags.status) != 0u) {
        putString("Afraid", Coord_t{23, 22});
    } else {
        putString(&blank_string[BLANK_LENGTH - 6], Coord_t{23, 22});
    }
}

// Prints Poisoned status -RAK-
void printCharacterPoisonedState() {
    if ((PY_POISONED & py.flags.status) != 0u) {
        putString("Poisoned", Coord_t{23, 29});
    } else {
        putString(&blank_string[BLANK_LENGTH - 8], Coord_t{23, 29});
    }
}

// Prints Searching, Resting, Paralysis, or 'count' status -RAK-
void printCharacterMovementState() {
    py.flags.status &= ~PY_REPEAT;

    if (py.flags.paralysis > 1) {
        putString("Paralysed", Coord_t{23, 38});
        return;
    }

    if ((PY_REST & py.flags.status) != 0u) {
        char restString[16];

        if (py.flags.rest < 0) {
            (void) strcpy(restString, "Rest *");
        } else if (config.display_counts) {
            (void) sprintf(restString, "Rest %-5d", py.flags.rest);
        } else {
            (void) strcpy(restString, "Rest");
        }

        putString(restString, Coord_t{23, 38});

        return;
    }

    if (game.command_count > 0) {
        char repeatString[16];

        if (config.display_counts) {
            (void) sprintf(repeatString, "Repeat %-3d", game.command_count);
        } else {
            (void) strcpy(repeatString, "Repeat");
        }

        py.flags.status |= PY_REPEAT;

        putString(repeatString, Coord_t{23, 38});

        if ((PY_SEARCH & py.flags.status) != 0u) {
            putString("Search", Coord_t{23, 38});
        }

        return;
    }

    if ((PY_SEARCH & py.flags.status) != 0u) {
        putString("Searching", Coord_t{23, 38});
        return;
    }

    // "repeat 999" is 10 characters
    putString(&blank_string[BLANK_LENGTH - 10], Coord_t{23, 38});
}

// Prints the speed of a character. -CJS-
void printCharacterSpeed() {
    int speed = py.flags.speed;

    // Search mode.
    if ((PY_SEARCH & py.flags.status) != 0u) {
        speed--;
    }

    if (speed > 1) {
        putString("Very Slow", Coord_t{23, 49});
    } else if (speed == 1) {
        putString("Slow     ", Coord_t{23, 49});
    } else if (speed == 0) {
        putString(&blank_string[BLANK_LENGTH - 9], Coord_t{23, 49});
    } else if (speed == -1) {
        putString("Fast     ", Coord_t{23, 49});
    } else {
        putString("Very Fast", Coord_t{23, 49});
    }
}

void printCharacterStudyInstruction() {
    py.flags.status &= ~PY_STUDY;

    if (py.flags.new_spells_to_learn == 0) {
        putString(&blank_string[BLANK_LENGTH - 5], Coord_t{23, 59});
    } else {
        putString("Study", Coord_t{23, 59});
    }
}

// Prints winner status on display -RAK-
void printCharacterWinner() {
    if ((game.noscore & 0x2) != 0) {
        if (game.wizard_mode) {
            putString("Is wizard  ", Coord_t{22, 0});
        } else {
            putString("Was wizard ", Coord_t{22, 0});
        }
    } else if ((game.noscore & 0x1) != 0) {
        putString("Resurrected", Coord_t{22, 0});
    } else if ((game.noscore & 0x4) != 0) {
        putString("Duplicate", Coord_t{22, 0});
    } else if (game.total_winner) {
        putString("*Winner*   ", Coord_t{22, 0});
    }
}

// Prints character-screen info -RAK-
void printCharacterStatsBlock() {
    printCharacterInfoInField(character_races[py.misc.race_id].name, 2, STAT_COLUMN);
    printCharacterInfoInField(classes[py.misc.class_id].title, 3, STAT_COLUMN);
    printCharacterInfoInField(playerRankTitle(), 4, STAT_COLUMN);

    for (int i = 0; i < 6; i++) {
        displayCharacterStats(i);
    }

    printHeaderNumber("LEV ", (int) py.misc.level, 13, STAT_COLUMN);
    printHeaderLongNumber("EXP ", py.misc.exp, 14, STAT_COLUMN);
    printHeaderNumber("MANA", py.misc.current_mana, 15, STAT_COLUMN);
    printHeaderNumber("MHP ", py.misc.max_hp, 16, STAT_COLUMN);
    printHeaderNumber("CHP ", py.misc.current_hp, 17, STAT_COLUMN);
    printHeaderNumber("AC  ", py.misc.display_ac, 19, STAT_COLUMN);
    printHeaderLongNumber("GOLD", py.misc.au, 20, STAT_COLUMN);
    printCharacterWinner();

    uint32_t status = py.flags.status;

    if (((PY_HUNGRY | PY_WEAK) & status) != 0u) {
        printCharacterHungerStatus();
    }

    if ((PY_BLIND & status) != 0u) {
        printCharacterBlindStatus();
    }

    if ((PY_CONFUSED & status) != 0u) {
        printCharacterConfusedState();
    }

    if ((PY_FEAR & status) != 0u) {
        printCharacterFearState();
    }

    if ((PY_POISONED & status) != 0u) {
        printCharacterPoisonedState();
    }

    if (((PY_SEARCH | PY_REST) & status) != 0u) {
        printCharacterMovementState();
    }

    // if speed non zero, print it, modify speed if Searching
    int16_t speed = py.flags.speed - (int16_t) ((PY_SEARCH & status) >> 8);
    if (speed != 0) {
        printCharacterSpeed();
    }

    // display the study field
    printCharacterStudyInstruction();
}

// Prints the following information on the screen. -JWT-
void printCharacterInformation() {
    clearScreen();

    putString("Name        :", Coord_t{2, 1});
    putString("Race        :", Coord_t{3, 1});
    putString("Sex         :", Coord_t{4, 1});
    putString("Class       :", Coord_t{5, 1});

    if (!game.character_generated) {
        return;
    }

    putString(py.misc.name, Coord_t{2, 15});
    putString(character_races[py.misc.race_id].name, Coord_t{3, 15});
    putString((playerGetGenderLabel()), Coord_t{4, 15});
    putString(classes[py.misc.class_id].title, Coord_t{5, 15});
}

// Prints the following information on the screen. -JWT-
void printCharacterStats() {
    for (int i = 0; i < 6; i++) {
        vtype_t buf = {'\0'};

        statsAsString(py.stats.used[i], buf);
        putString(stat_names[i], Coord_t{2 + i, 61});
        putString(buf, Coord_t{2 + i, 66});

        if (py.stats.max[i] > py.stats.current[i]) {
            statsAsString(py.stats.max[i], buf);
            putString(buf, Coord_t{2 + i, 73});
        }
    }

    printHeaderNumber("+ To Hit    ", py.misc.display_to_hit, 9, 1);
    printHeaderNumber("+ To Damage ", py.misc.display_to_damage, 10, 1);
    printHeaderNumber("+ To AC     ", py.misc.display_to_ac, 11, 1);
    printHeaderNumber("  Total AC  ", py.misc.display_ac, 12, 1);
}

// Returns a rating of x depending on y -JWT-
const char *statRating(int y, int x) {
    switch (x / y) {
        case -3:
        case -2:
        case -1:
            return "Very Bad";
        case 0:
        case 1:
            return "Bad";
        case 2:
            return "Poor";
        case 3:
        case 4:
            return "Fair";
        case 5:
            return "Good";
        case 6:
            return "Very Good";
        case 7:
        case 8:
            return "Excellent";
        default:
            return "Superb";
    }
}

// Prints age, height, weight, and SC -JWT-
void printCharacterVitalStatistics() {
    printHeaderNumber("Age          ", (int) py.misc.age, 2, 38);
    printHeaderNumber("Height       ", (int) py.misc.height, 3, 38);
    printHeaderNumber("Weight       ", (int) py.misc.weight, 4, 38);
    printHeaderNumber("Social Class ", (int) py.misc.social_class, 5, 38);
}

// Prints the following information on the screen. -JWT-
void printCharacterLevelExperience() {
    printHeaderLongNumber7Spaces("Level      ", (int32_t) py.misc.level, 9, 28);
    printHeaderLongNumber7Spaces("Experience ", py.misc.exp, 10, 28);
    printHeaderLongNumber7Spaces("Max Exp    ", py.misc.max_exp, 11, 28);

    if (py.misc.level >= PLAYER_MAX_LEVEL) {
        putStringClearToEOL("Exp to Adv.: *******", Coord_t{12, 28});
    } else {
        printHeaderLongNumber7Spaces("Exp to Adv.", (int32_t) (py.base_exp_levels[py.misc.level - 1] * py.misc.experience_factor / 100), 12, 28);
    }

    printHeaderLongNumber7Spaces("Gold       ", py.misc.au, 13, 28);
    printHeaderNumber("Max Hit Points ", py.misc.max_hp, 9, 52);
    printHeaderNumber("Cur Hit Points ", py.misc.current_hp, 10, 52);
    printHeaderNumber("Max Mana       ", py.misc.mana, 11, 52);
    printHeaderNumber("Cur Mana       ", py.misc.current_mana, 12, 52);
}

// Prints ratings on certain abilities -RAK-
void printCharacterAbilities() {
    clearToBottom(14);

    int xbth = py.misc.bth + py.misc.plusses_to_hit * BTH_PER_PLUS_TO_HIT_ADJUST + (class_level_adj[py.misc.class_id][CLASS_BTH] * py.misc.level);
    int xbthb = py.misc.bth_with_bows + py.misc.plusses_to_hit * BTH_PER_PLUS_TO_HIT_ADJUST + (class_level_adj[py.misc.class_id][CLASS_BTHB] * py.misc.level);

    // this results in a range from 0 to 29
    int xfos = 40 - py.misc.fos;
    if (xfos < 0) {
        xfos = 0;
    }

    int xsrh = py.misc.chance_in_search;

    // this results in a range from 0 to 9
    int xstl = py.misc.stealth_factor + 1;
    int xdis = py.misc.disarm + 2 * playerDisarmAdjustment() + playerStatAdjustmentWisdomIntelligence(A_INT) + (class_level_adj[py.misc.class_id][CLASS_DISARM] * py.misc.level / 3);
    int xsave = py.misc.saving_throw + playerStatAdjustmentWisdomIntelligence(A_WIS) + (class_level_adj[py.misc.class_id][CLASS_SAVE] * py.misc.level / 3);
    int xdev = py.misc.saving_throw + playerStatAdjustmentWisdomIntelligence(A_INT) + (class_level_adj[py.misc.class_id][CLASS_DEVICE] * py.misc.level / 3);

    vtype_t xinfra = {'\0'};
    (void) sprintf(xinfra, "%d feet", py.flags.see_infra * 10);

    putString("(Miscellaneous Abilities)", Coord_t{15, 25});
    putString("Fighting    :", Coord_t{16, 1});
    putString(statRating(12, xbth), Coord_t{16, 15});
    putString("Bows/Throw  :", Coord_t{17, 1});
    putString(statRating(12, xbthb), Coord_t{17, 15});
    putString("Saving Throw:", Coord_t{18, 1});
    putString(statRating(6, xsave), Coord_t{18, 15});

    putString("Stealth     :", Coord_t{16, 28});
    putString(statRating(1, xstl), Coord_t{16, 42});
    putString("Disarming   :", Coord_t{17, 28});
    putString(statRating(8, xdis), Coord_t{17, 42});
    putString("Magic Device:", Coord_t{18, 28});
    putString(statRating(6, xdev), Coord_t{18, 42});

    putString("Perception  :", Coord_t{16, 55});
    putString(statRating(3, xfos), Coord_t{16, 69});
    putString("Searching   :", Coord_t{17, 55});
    putString(statRating(6, xsrh), Coord_t{17, 69});
    putString("Infra-Vision:", Coord_t{18, 55});
    putString(xinfra, Coord_t{18, 69});
}

// Used to display the character on the screen. -RAK-
void printCharacter() {
    printCharacterInformation();
    printCharacterVitalStatistics();
    printCharacterStats();
    printCharacterLevelExperience();
    printCharacterAbilities();
}

// Gets a name for the character -JWT-
void getCharacterName() {
    putStringClearToEOL("Enter your player's name  [press <RETURN> when finished]", Coord_t{21, 2});

    putString(&blank_string[BLANK_LENGTH - 23], Coord_t{2, 15});

    if (!getStringInput(py.misc.name, Coord_t{2, 15}, 23) || py.misc.name[0] == 0) {
        getDefaultPlayerName(py.misc.name);
        putString(py.misc.name, Coord_t{2, 15});
    }

    clearToBottom(20);
}

// Changes the name of the character -JWT-
void changeCharacterName() {
    vtype_t temp = {'\0'};
    bool flag = false;

    printCharacter();

    while (!flag) {
        putStringClearToEOL("<f>ile character description. <c>hange character name.", Coord_t{21, 2});

        switch (getKeyInput()) {
            case 'c':
                getCharacterName();
                flag = true;
                break;
            case 'f':
                putStringClearToEOL("File name:", Coord_t{0, 0});

                if (getStringInput(temp, Coord_t{0, 10}, 60) && (temp[0] != 0)) {
                    if (outputPlayerCharacterToFile(temp)) {
                        flag = true;
                    }
                }
                break;
            case ESCAPE:
            case ' ':
            case '\n':
            case '\r':
                flag = true;
                break;
            default:
                terminalBellSound();
                break;
        }
    }
}

// Print list of spells -RAK-
// if non_consecutive is  -1: spells numbered consecutively from 'a' to 'a'+num
//                       >=0: spells numbered by offset from non_consecutive
void displaySpellsList(const int *spell_ids, int number_of_choices, bool comment, int non_consecutive) {
    int col;
    if (comment) {
        col = 22;
    } else {
        col = 31;
    }

    int consecutive_offset = (classes[py.misc.class_id].class_to_use_mage_spells == SPELL_TYPE_MAGE ? NAME_OFFSET_SPELLS : NAME_OFFSET_PRAYERS);

    eraseLine(Coord_t{1, col});
    putString("Name", Coord_t{1, col + 5});
    putString("Lv Mana Fail", Coord_t{1, col + 35});

    // only show the first 22 choices
    if (number_of_choices > 22) {
        number_of_choices = 22;
    }

    for (int i = 0; i < number_of_choices; i++) {
        int spell_id = spell_ids[i];
        Spell_t const &spell = magic_spells[py.misc.class_id - 1][spell_id];

        const char *p = nullptr;
        if (!comment) {
            p = "";
        } else if ((py.flags.spells_forgotten & (1L << spell_id)) != 0) {
            p = " forgotten";
        } else if ((py.flags.spells_learnt & (1L << spell_id)) == 0) {
            p = " unknown";
        } else if ((py.flags.spells_worked & (1L << spell_id)) == 0) {
            p = " untried";
        } else {
            p = "";
        }

        // determine whether or not to leave holes in character choices, non_consecutive -1
        // when learning spells, consecutive_offset>=0 when asking which spell to cast.
        char spell_char;
        if (non_consecutive == -1) {
            spell_char = (char) ('a' + i);
        } else {
            spell_char = (char) ('a' + spell_id - non_consecutive);
        }

        vtype_t out_val = {'\0'};
        (void) sprintf(out_val, "  %c) %-30s%2d %4d %3d%%%s", spell_char, spell_names[spell_id + consecutive_offset], spell.level_required, spell.mana_required,
                       spellChanceOfSuccess(spell_id), p);
        putStringClearToEOL(out_val, Coord_t{2 + i, col});
    }
}

// Increases hit points and level -RAK-
static void playerGainLevel() {
    py.misc.level++;

    vtype_t msg = {'\0'};
    (void) sprintf(msg, "Welcome to level %d.", (int) py.misc.level);
    printMessage(msg);

    playerCalculateHitPoints();

    int32_t new_exp = py.base_exp_levels[py.misc.level - 1] * py.misc.experience_factor / 100;

    if (py.misc.exp > new_exp) {
        // lose some of the 'extra' exp when gaining several levels at once
        int32_t dif_exp = py.misc.exp - new_exp;
        py.misc.exp = new_exp + (dif_exp / 2);
    }

    printCharacterLevel();
    printCharacterTitle();

    Class_t const &player_class = classes[py.misc.class_id];

    if (player_class.class_to_use_mage_spells == SPELL_TYPE_MAGE) {
        playerCalculateAllowedSpellsCount(A_INT);
        playerGainMana(A_INT);
    } else if (player_class.class_to_use_mage_spells == SPELL_TYPE_PRIEST) {
        playerCalculateAllowedSpellsCount(A_WIS);
        playerGainMana(A_WIS);
    }
}

// Prints experience -RAK-
void displayCharacterExperience() {
    if (py.misc.exp > PLAYER_MAX_EXP) {
        py.misc.exp = PLAYER_MAX_EXP;
    }

    while ((py.misc.level < PLAYER_MAX_LEVEL) && (signed) (py.base_exp_levels[py.misc.level - 1] * py.misc.experience_factor / 100) <= py.misc.exp) {
        playerGainLevel();
    }

    if (py.misc.exp > py.misc.max_exp) {
        py.misc.max_exp = py.misc.exp;
    }

    printLongNumber(py.misc.exp, 14, STAT_COLUMN + 6);
}
