// SETTINGS in the title menu: a seventh entry under SHAKE, in the menu's
// own font and colours, that opens the settings menu (src/menu.cpp).
//
// The title menu is a TaskCore built from a table, sTitleMenuTarget: its
// entries' names, positions (12 apart, percent of half the screen from its
// centre), which ones the cursor skips, and the item list each opens (only
// SHAKE has one). TaskCore makes one TextRow per name and sizes its
// per-entry arrays by their count, so the port gives the table longer copies
// of those four, with SETTINGS last. Confirming it (circle) runs the menu's
// confirmSlot, wrapped here to open the settings menu instead; START still
// starts the day from any entry.
//
// The title menu times out after a while without input (frameBound) and
// plays the intro again. While the settings menu is open, opened from here
// or with F1, the game sees no input (Psyz_PadsHold), so its update is
// wrapped to keep the frame count at 0 meanwhile. With the settings menu
// never opened, nothing the game does changes; the entry only adds a row.
//
// Built with the game's C (it needs TitleMenu's table), not with the port's
// other files.

#include "common.h"
#include "screen_sprite.h"
#include "title_menu.h"
#include "menu.h"
#include "title_settings.h"

extern TaskCoreTarget sTitleMenuTarget;

// The game's six entries, then SETTINGS.
#define GAME_SLOTS (TITLEMENU_SHAKE + 1)
#define SETTINGS_SLOT GAME_SLOTS
#define SLOTS (GAME_SLOTS + 1)

static char sSettingsName[] = "SETTINGS";

static char* sNames[SLOTS + 1];
static void* sHiddenSlots[SLOTS + 1];
static ScreenSpritePos sPositions[SLOTS];
static TaskCoreItemList* sSlotLists[SLOTS];

static void (*sConfirmSlot)(TitleMenu* self);
static void (*sUpdate)(TitleMenu* self, BasicClass* sender, s32 event);

static void SettingsConfirmSlot(TitleMenu* self) {
    if (self->activeSlot == SETTINGS_SLOT) {
        Menu_Open();
        return;
    }
    sConfirmSlot(self);
}

static void SettingsUpdate(TitleMenu* self, BasicClass* sender, s32 event) {
    if (Menu_IsOpen()) {
        self->frameCounter = 0; // no timing out while the game sees no input
    }
    sUpdate(self, sender, event);
}

void TitleSettings_Init(void) {
    TaskCoreTarget* target = &sTitleMenuTarget;
    int i;

    for (i = 0; i < GAME_SLOTS; i++) {
        sNames[i] = target->names[i];
        sHiddenSlots[i] = target->hiddenSlots[i];
        sPositions[i] = target->slotPositions[i];
        sSlotLists[i] = target->slotLists[i];
    }
    sNames[SETTINGS_SLOT] = sSettingsName;
    sPositions[SETTINGS_SLOT].x = sPositions[GAME_SLOTS - 1].x;
    sPositions[SETTINGS_SLOT].y = 2 * sPositions[GAME_SLOTS - 1].y - sPositions[GAME_SLOTS - 2].y;
    target->names = sNames;
    target->hiddenSlots = sHiddenSlots;
    target->slotPositions = sPositions;
    target->slotLists = sSlotLists;

    sConfirmSlot = gTitleMenuMethods.confirmSlot;
    gTitleMenuMethods.confirmSlot = SettingsConfirmSlot;
    sUpdate = gTitleMenuMethods.update;
    gTitleMenuMethods.update = SettingsUpdate;
}
