// The settings menu: Dear ImGui drawn over the game's picture, through
// psyz's overlay hooks, dressed as the game's title menu: its font
// (ETC\FONTICON.TIM, read from the disc when the menu is set up, in whole
// multiples of its 8x8 pixels), its blue, its grey entries with the one
// under the cursor yellow, and its pink headings.
//
// F1, or a gamepad's Guide button or both its sticks pressed in, opens and
// closes it, and so does the title menu's SETTINGS (src/title_settings.c). psyz presents the PlayStation's display, then calls the
// overlay, which draws into the window's own buffer (the swapchain texture
// with SDL GPU, the default framebuffer with OpenGL): never into the
// game's VRAM, so the debug server's screenshots and VRAM dumps, which read
// the VRAM, don't show it.
//
// While it is open the game keeps running, but sees no keys or pad:
// Psyz_PadsHold has its pads read as nothing pressed, and a button still held
// when it closes reads as released until it is let go. The dream's clock
// runs on and the music plays (START pauses a dream, before opening it).
// Only the events psyz polls reach Dear ImGui, and only while it is open,
// so its queue stays empty the rest of the time.
//
// Changes go to src/settings.c and src/controls.c at once; the settings
// reach psyz and the dream at the next VSync callback, between the game's
// frames rather than in the middle of psyz's present. Both files are
// written when the menu closes.

#include "menu.h"
#include "disc.h"

extern "C" {
#include "controls.h"
#include "settings.h"
}

#include <psyz.h>
#include <psyz/input.h>
#include <psyz/overlay.h>
#include <psyz/system.h>
#ifdef LSD_MENU_SDL3_GPU
#include <psyz/overlay_sdl3_gpu.h>
#else
#include <psyz/overlay_sdl3_gl.h>
#endif

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_internal.h> // ImFontLoader, GetFocusID
#ifdef LSD_MENU_SDL3_GPU
#include <imgui_impl_sdlgpu3.h>
#else
#include <imgui_impl_opengl3.h>
#endif

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

namespace {

// The font's height before ScaleToWindow: the game's 8x8 glyphs at twice
// their size.
const float kFontSize = 16.0f;
const int kGlyph = 8;

// The title menu's colours as it is drawn: the backdrop's blue, the grey of
// its entries, yellow for the one under the cursor (sTitleMenuTarget's 80
// and 128 of 128), and the pink of its "Day 001".
const ImVec4 kBlue(16 / 255.0f, 0.0f, 62 / 255.0f, 1.0f);
const ImVec4 kGrey(156 / 255.0f, 156 / 255.0f, 156 / 255.0f, 1.0f);
const ImVec4 kYellow(1.0f, 1.0f, 0.0f, 1.0f);
const ImVec4 kPink(1.0f, 156 / 255.0f, 156 / 255.0f, 1.0f);

bool sOpen;
bool sReady;     // Dear ImGui and its renderer are set up
bool sDrawn;     // this present built a frame to draw (SDL GPU)
bool sWasBusy;   // the last frame had a popup open or an item in use
bool sOpening;   // the first frame since it opened
bool sPadHeld;   // a pad's button held since it opened, kept from Dear ImGui
int sCapture = -1; // the button waiting for a key, or -1
float sScale;
ImGuiStyle sBaseStyle;
const char* sProblem; // why the last save or key failed, until the next
PsyzVSyncCb sNextVSync;
char* sDiscPath;
#ifdef LSD_MENU_SDL3_GPU
SDL_GPUDevice* sDevice;
#endif

void Save() {
    sProblem = nullptr;
    if (Settings_Save() != 0 || Controls_Save() != 0) {
        sProblem = "Could not save the settings (see the log).";
    }
}

void SetOpen(bool open) {
    if (open == sOpen || !sReady) {
        return;
    }
    sOpen = open;
    sCapture = -1;
    Psyz_PadsHold(open);
    if (open) {
        ImGuiIO& io = ImGui::GetIO();
        io.ClearInputKeys();
        io.ClearInputMouse();
        sWasBusy = false;
        sOpening = true;
        sPadHeld = true;
    } else {
        Save();
    }
}

// The port's own keys, which a button can't take: F1 (this menu), F4
// (fullscreen) and F6 (psyz's VRAM view).
bool IsReserved(SDL_Scancode key) {
    return key == SDL_SCANCODE_F1 || key == SDL_SCANCODE_F4 || key == SDL_SCANCODE_F6;
}

void AddKey(int b, SDL_Scancode key) {
    int keys[CONTROLS_KEYS_MAX];
    int n = Controls_Keys(b, keys);
    sCapture = -1;
    sProblem = nullptr;
    if (IsReserved(key) || SDL_GetScancodeName(key)[0] == '\0') {
        sProblem = "F1, F4 and F6 are the port's own keys.";
        return;
    }
    for (int k = 0; k < n; k++) {
        if (keys[k] == key) {
            return;
        }
    }
    if (n == CONTROLS_KEYS_MAX) {
        SDL_memmove(keys, keys + 1, sizeof(*keys) * (n - 1)); // the oldest goes
        n--;
    }
    keys[n++] = key;
    Controls_SetKeys(b, keys, n);
}

bool IsMenuButton(const SDL_GamepadButtonEvent& e) {
    if (e.button == SDL_GAMEPAD_BUTTON_GUIDE) {
        return true;
    }
    SDL_Gamepad* pad = SDL_GetGamepadFromID(e.which);
    if (pad == nullptr) {
        return false;
    }
    return (e.button == SDL_GAMEPAD_BUTTON_LEFT_STICK &&
            SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_RIGHT_STICK)) ||
           (e.button == SDL_GAMEPAD_BUTTON_RIGHT_STICK &&
            SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_LEFT_STICK));
}

void OnEvent(const SDL_Event* e) {
    if (!sReady) {
        return;
    }
    switch (e->type) {
    case SDL_EVENT_KEY_DOWN:
        if (e->key.scancode == SDL_SCANCODE_F1) {
            if (!e->key.repeat) {
                SetOpen(!sOpen);
            }
            return;
        }
        if (sOpen && sCapture >= 0) {
            if (!e->key.repeat) {
                AddKey(sCapture, e->key.scancode);
            }
            return;
        }
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        if (IsMenuButton(e->gbutton)) {
            SetOpen(!sOpen);
            return;
        }
        break;
    default:
        break;
    }
    if (sOpen) {
        ImGui_ImplSDL3_ProcessEvent(e);
    }
}

// The game's font: one bit a pixel, bit 7 leftmost, for ' ' to '~'.
unsigned char sGlyphs[95][kGlyph];

unsigned Le16(const unsigned char* p) {
    return p[0] | (unsigned)p[1] << 8;
}

unsigned Le32(const unsigned char* p) {
    return Le16(p) | (unsigned)Le16(p + 2) << 16;
}

// Reads ETC\FONTICON.TIM (in the game's data directory, CDI\) into sGlyphs: a 4-bit TIM with a CLUT (id 0x10,
// flags 8, then the CLUT's and the image's blocks: length, x, y, width in
// 16-bit units, height, data), 8x8 glyphs by character code, 32 to a row
// (the first row, the control codes, empty). A pixel
// is ink where its CLUT colour is bright: the glyphs are white (0x7FFF)
// with a drop shadow in a dark grey-blue (0x2421) that hardly shows on the
// title menu's backdrop, left out here.
bool LoadGameFont() {
    size_t size = 0;
    unsigned char* tim = (unsigned char*)Disc_ReadFile(sDiscPath, "CDI\\ETC\\FONTICON.TIM", &size);
    bool ok = false;
    if (tim != nullptr && size >= 20 && Le32(tim) == 0x10 && Le32(tim + 4) == 8) {
        const unsigned char* clut = tim + 8 + 12;
        unsigned clutLen = Le32(tim + 8);
        unsigned colours = Le16(tim + 8 + 8) * Le16(tim + 8 + 10);
        const unsigned char* image = tim + 8 + clutLen;
        if (clutLen >= 12 + 2 * colours && colours >= 16 && 8 + clutLen + 12 <= size) {
            unsigned pitch = Le16(image + 8) * 2; // bytes a row
            unsigned height = Le16(image + 10);
            const unsigned char* px = image + 12;
            if (pitch >= 128 && height >= 32 && 8 + clutLen + 12 + pitch * height <= size) {
                for (int c = 0; c < 95; c++) {
                    int gx = (c + ' ') % 32 * kGlyph;
                    int gy = (c + ' ') / 32 * kGlyph;
                    for (int y = 0; y < kGlyph; y++) {
                        unsigned char bits = 0;
                        for (int x = 0; x < kGlyph; x++) {
                            unsigned char b = px[(gy + y) * pitch + (gx + x) / 2];
                            unsigned index = (gx + x) & 1 ? b >> 4 : b & 15;
                            unsigned colour = Le16(clut + 2 * index);
                            if ((colour & 31) + (colour >> 5 & 31) + (colour >> 10 & 31) >= 48) {
                                bits |= 0x80 >> x;
                            }
                        }
                        sGlyphs[c][y] = bits;
                    }
                }
                ok = true;
            }
        }
    }
    SDL_free(tim);
    return ok;
}

// An ImFontLoader for sGlyphs: a font of any size draws each glyph at the
// whole multiple of 8 nearest its size, every glyph as wide as it is high,
// as the game spaces them.
bool FontContainsGlyph(ImFontAtlas*, ImFontConfig*, ImWchar c) {
    return c >= ' ' && c <= '~';
}

bool FontBakedInit(ImFontAtlas*, ImFontConfig*, ImFontBaked* baked, void*) {
    baked->Ascent = baked->Size;
    baked->Descent = 0.0f;
    return true;
}

bool FontLoadGlyph(ImFontAtlas* atlas, ImFontConfig* src, ImFontBaked* baked, void*, ImWchar c,
                   ImFontGlyph* glyph, float* advance) {
    static ImVector<unsigned char> pixels;
    if (c < ' ' || c > '~') {
        return false;
    }
    if (advance != nullptr) {
        *advance = baked->Size;
        return true;
    }
    glyph->Codepoint = c;
    glyph->AdvanceX = baked->Size;
    if (c == ' ') {
        return true;
    }
    float density = src->RasterizerDensity * baked->RasterizerDensity;
    int k = (int)(baked->Size * density / kGlyph + 0.5f);
    if (k < 1) {
        k = 1;
    }
    int n = kGlyph * k;
    ImFontAtlasRectId id = ImFontAtlasPackAddRect(atlas, n, n);
    if (id == ImFontAtlasRectId_Invalid) {
        return false;
    }
    pixels.resize(n * n);
    for (int y = 0; y < n; y++) {
        for (int x = 0; x < n; x++) {
            pixels[y * n + x] = sGlyphs[c - ' '][y / k] & (0x80 >> (x / k)) ? 255 : 0;
        }
    }
    glyph->X0 = 0.0f;
    glyph->Y0 = 0.0f;
    glyph->X1 = n / density;
    glyph->Y1 = n / density;
    glyph->Visible = true;
    glyph->PackId = id;
    ImFontAtlasBakedSetFontGlyphBitmap(atlas, baked, src, glyph, ImFontAtlasPackGetRect(atlas, id),
                                       pixels.Data, ImTextureFormat_Alpha8, n);
    return true;
}

ImFontLoader* GameFontLoader() {
    static ImFontLoader loader;
    loader.Name = "FONTICON.TIM";
    loader.FontSrcContainsGlyph = FontContainsGlyph;
    loader.FontBakedInit = FontBakedInit;
    loader.FontBakedLoadGlyph = FontLoadGlyph;
    return &loader;
}

// Sizes everything to the window: the glyphs at half the size of the game's
// own (a 240-line picture's pixels), but at least twice their 8x8, so
// 16 pixels up to a window 1199 high, then 24, 32...
void ScaleToWindow() {
    int k = (int)(ImGui::GetIO().DisplaySize.y / 480.0f + 0.5f);
    float scale = (k < 2 ? 2 : k) * kGlyph / kFontSize;
    if (SDL_fabsf(scale - sScale) < 0.01f) {
        return;
    }
    sScale = scale;
    ImGuiStyle& style = ImGui::GetStyle();
    style = sBaseStyle;
    style.ScaleAllSizes(scale);
    style.FontScaleMain = scale;
}

// A row of the menu: its label, grey, or yellow when the control after it
// has the cursor (as the title menu shows its entries), then that control.
void Label(const char* label, bool focused) {
    float font = ImGui::GetFontSize();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(focused ? kYellow : kGrey, "%s", label);
    ImGui::SameLine(font * 15.0f);
    ImGui::SetNextItemWidth(font * 13.0f);
}

// Whether the control about to be made with this id has the cursor.
bool HasCursor(const char* id) {
    return ImGui::GetFocusID() == ImGui::GetID(id);
}

// After a control: why it can't change, or when a change shows.
void Note(SettingId id, const char* note = nullptr) {
    const char* from = Settings_From(id);
    if (from != nullptr) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s, for this run", from);
    } else if (note != nullptr) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", note);
    }
}

struct Choice {
    const char* value; // as settings.ini has it
    const char* text;  // as the menu shows it
    const char* note = nullptr; // beside it while chosen
};

void Combo(SettingId id, const char* label, const Choice* choices, int count,
           const char* note = nullptr) {
    const char* value = Settings_Value(id);
    const char* shown = value;
    for (int i = 0; i < count; i++) {
        if (SDL_strcasecmp(value, choices[i].value) == 0) {
            shown = choices[i].text;
            if (note == nullptr) {
                note = choices[i].note;
            }
        }
    }
    ImGui::PushID(id);
    bool focused = HasCursor("##");
    Label(label, focused);
    ImGui::BeginDisabled(Settings_From(id) != nullptr);
    if (focused) {
        ImGui::PushStyleColor(ImGuiCol_Text, kYellow);
    }
    bool open = ImGui::BeginCombo("##", shown);
    if (focused) {
        ImGui::PopStyleColor();
    }
    if (open) {
        for (int i = 0; i < count; i++) {
            bool selected = SDL_strcasecmp(value, choices[i].value) == 0;
            ImGui::BeginDisabled(!selected && !Settings_Accepts(id, choices[i].value));
            if (ImGui::Selectable(choices[i].text, selected)) {
                Settings_Set(id, choices[i].value);
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
            ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    ImGui::PopID();
    Note(id, note);
}

// Left and right (the arrows, the d-pad or the left stick) move the slider
// under the cursor by step, without first activating it as Dear ImGui would
// have (Space, or circle). Its keys are kept from Dear ImGui's navigation.
int Nudge(ImGuiID id, int step) {
    static const ImGuiKey keys[][3] = {
        {ImGuiKey_LeftArrow, ImGuiKey_GamepadDpadLeft, ImGuiKey_GamepadLStickLeft},
        {ImGuiKey_RightArrow, ImGuiKey_GamepadDpadRight, ImGuiKey_GamepadLStickRight},
    };
    int by = 0;
    for (int side = 0; side < 2; side++) {
        for (ImGuiKey key : keys[side]) {
            ImGui::SetKeyOwner(key, id);
            if (ImGui::IsKeyPressed(key, ImGuiInputFlags_Repeat, id)) {
                by += side ? step : -step;
            }
        }
    }
    return by;
}

void Slider(SettingId id, const char* label, int min, int max, const char* format,
            const char* note = nullptr, int step = 1) {
    int value = atoi(Settings_Value(id));
    ImGui::PushID(id);
    bool focused = HasCursor("##");
    Label(label, focused);
    ImGui::BeginDisabled(Settings_From(id) != nullptr);
    ImGui::PushStyleColor(ImGuiCol_Text, focused ? kYellow : kGrey);
    bool changed =
        ImGui::SliderInt("##", &value, min, max, format, ImGuiSliderFlags_AlwaysClamp);
    if (focused && !ImGui::IsItemActive() && Settings_From(id) == nullptr) {
        int by = Nudge(ImGui::GetItemID(), step);
        if (by != 0) {
            value = SDL_clamp((value + by) / step * step, min, max);
            changed = true;
        }
    }
    if (changed) {
        char text[16];
        SDL_snprintf(text, sizeof(text), "%d", value);
        Settings_Set(id, text);
    }
    ImGui::PopStyleColor();
    ImGui::EndDisabled();
    ImGui::PopID();
    Note(id, note);
}

void Check(SettingId id, const char* label, const char* note = nullptr) {
    bool on = SDL_strcmp(Settings_Value(id), "on") == 0;
    ImGui::PushID(id);
    Label(label, HasCursor("##"));
    ImGui::BeginDisabled(Settings_From(id) != nullptr);
    if (ImGui::Checkbox("##", &on)) {
        Settings_Set(id, on ? "on" : "off");
    }
    ImGui::EndDisabled();
    ImGui::PopID();
    Note(id, note);
}

// A heading, in the pink of the title menu's "Day 001".
void Heading(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, kPink);
    ImGui::SeparatorText(text);
    ImGui::PopStyleColor();
}

void PictureSection() {
    static const Choice aspects[] = {
        {"4:3", "4:3", "from the next dream"},
        {"16:10", "16:10", "from the next dream"},
        {"16:9", "16:9", "from the next dream"},
        {"21:9", "21:9", "from the next dream"},
        {"32:9", "32:9", "from the next dream"},
    };
    static const Choice scales[] = {
        {"sharp", "sharp", "crisp, even pixels"},
        {"nearest", "nearest", "crisp, uneven"},
        {"smooth", "smooth", "blurred"},
        {"integer", "integer", "whole multiples"},
    };
    static const Choice colours[] = {
        {"console", "console", "15-bit, as the PS1"},
        {"full", "full", "24-bit, no dither"},
    };
    static const Choice geometries[] = {
        {"console", "console", "whole pixels"},
        {"precise", "precise", "between pixels"},
        {"perspective", "perspective", "and true textures"},
    };
    static const Choice resolutions[] = {
        {"1", "1x  320x240", "the PS1's"}, {"2", "2x  640x480"},   {"3", "3x  960x720"},
        {"4", "4x 1280x960"},              {"5", "5x 1600x1200"},  {"6", "6x 1920x1440"},
        {"7", "7x 2240x1680"},             {"8", "8x 2560x1920"},
    };

    Heading("PICTURE");
    if (sOpening) {
        // Keys and the pad start from the first setting. (The window is
        // not "appearing" again: no frame passes while the menu is shut.)
        ImGui::SetKeyboardFocusHere();
        ImGui::SetNavCursorVisible(true);
        sOpening = false;
    }
    Combo(SETTING_ASPECT, "ASPECT", aspects, SDL_arraysize(aspects), nullptr);
    Combo(SETTING_RESOLUTION, "RESOLUTION", resolutions, SDL_arraysize(resolutions), "of the 3D");
    Combo(SETTING_SCALE, "SCALE", scales, SDL_arraysize(scales));
    Check(SETTING_DITHER, "DITHER", "the PS1's 4x4");
    Combo(SETTING_COLOUR, "COLOUR", colours, SDL_arraysize(colours));
    Combo(SETTING_GEOMETRY, "GEOMETRY", geometries, SDL_arraysize(geometries));
}

void DreamSection() {
    static const Choice rates[] = {
        {"60", "60", "the PS1's"}, {"display", "display", "its refresh rate"},
        {"30", "30"},   {"72", "72"},   {"75", "75"},   {"90", "90"},   {"100", "100"},
        {"120", "120"}, {"144", "144"}, {"165", "165"}, {"180", "180"}, {"240", "240"},
        {"360", "360"},
    };
    static const Choice distances[] = {
        {"1", "1x", "the PS1's fog"},
        {"2", "2x", "the fog twice as far"},
        {"3", "3x", "the fog 3x as far"},
        {"4", "4x", "the fog 4x as far"},
    };
    static const Choice fogs[] = {
        {"console", "console", "the PS1's"},
        {"soft", "soft", "no pop-in at the edges"},
    };
    bool smooth = SDL_strcmp(Settings_Value(SETTING_SMOOTH), "on") == 0;

    Heading("DREAM");
    Slider(SETTING_PACE, "PACE", 10, 30, "%d a second", "14 PS1, 20 game");
    Check(SETTING_SMOOTH, "SMOOTH", "frames between ticks");
    Combo(SETTING_FRAME_RATE, "FRAME RATE", rates, SDL_arraysize(rates),
          smooth ? nullptr : "with smooth on");
    Combo(SETTING_DRAW_DISTANCE, "DRAW DISTANCE", distances, SDL_arraysize(distances));
    Combo(SETTING_FOG, "FOG", fogs, SDL_arraysize(fogs));
}

void SoundSection() {
    static const Choice interpolations[] = {
        {"console", "console", "the PS1's, soft"},
        {"cubic", "cubic", "brighter"},
        {"sinc", "sinc", "brightest"},
    };

    Heading("SOUND");
    Slider(SETTING_VOLUME, "VOLUME", 0, 100, "%d%%", nullptr, 5);
    Slider(SETTING_MUSIC_VOLUME, "MUSIC", 0, 100, "%d%%", nullptr, 5);
    Slider(SETTING_EFFECTS_VOLUME, "EFFECTS", 0, 100, "%d%%", nullptr, 5);
    Slider(SETTING_MOVIE_VOLUME, "MOVIES", 0, 100, "%d%%", nullptr, 5);
    Combo(SETTING_INTERPOLATION, "INTERPOLATION", interpolations,
          SDL_arraysize(interpolations));
}

void KeysText(int b, char* out, size_t size) {
    int keys[CONTROLS_KEYS_MAX];
    int n = Controls_Keys(b, keys);
    out[0] = '\0';
    for (int k = 0; k < n; k++) {
        SDL_strlcat(out, k ? ", " : "", size);
        SDL_strlcat(out, SDL_GetScancodeName((SDL_Scancode)keys[k]), size);
    }
    if (n == 0) {
        SDL_strlcpy(out, "(none)", size);
    }
}

void KeyboardSection() {
    Heading("KEYBOARD");
    ImGui::PushID("layout");
    bool focused = HasCursor("##");
    Label("LAYOUT", focused);
    if (focused) {
        ImGui::PushStyleColor(ImGuiCol_Text, kYellow);
    }
    bool open = ImGui::BeginCombo("##", Controls_LayoutName(Controls_Layout()));
    if (focused) {
        ImGui::PopStyleColor();
    }
    if (open) {
        for (int i = 0; i < Controls_LayoutCount(); i++) {
            if (ImGui::Selectable(Controls_LayoutName(i), i == Controls_Layout())) {
                Controls_SetLayout(i);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::TextDisabled("all keys at once");

    if (!ImGui::BeginTable("keys", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        return;
    }
    float font = ImGui::GetFontSize();
    ImGui::TableSetupColumn("button", ImGuiTableColumnFlags_WidthFixed, font * 9.0f);
    ImGui::TableSetupColumn("keys", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
    for (int b = 0; b < Controls_ButtonCount(); b++) {
        char name[16];
        char keys[128];
        ImGui::PushID(b);
        bool row = sCapture == b || HasCursor("ADD KEY") || HasCursor("CLEAR");
        SDL_strlcpy(name, Controls_ButtonName(b), sizeof(name));
        for (char* c = name; *c != '\0'; c++) {
            *c = (char)SDL_toupper(*c);
        }
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(row ? kYellow : kGrey, "%s", name);
        ImGui::TableNextColumn();
        if (sCapture == b) {
            ImGui::TextColored(kYellow, "press a key");
        } else {
            KeysText(b, keys, sizeof(keys));
            ImGui::TextWrapped("%s", keys);
        }
        ImGui::TableNextColumn();
        if (sCapture == b) {
            if (ImGui::Button("CANCEL")) {
                sCapture = -1;
            }
        } else {
            if (ImGui::Button("ADD KEY")) {
                sCapture = b;
                sProblem = nullptr;
            }
            ImGui::SameLine();
            if (ImGui::Button("CLEAR")) {
                Controls_SetKeys(b, nullptr, 0);
            }
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
}

void DrawMenu() {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiKey cancel =
        io.ConfigNavSwapGamepadButtons ? ImGuiKey_GamepadFaceDown : ImGuiKey_GamepadFaceRight;
    if (sCapture >= 0 && ImGui::IsKeyPressed(cancel, false)) {
        sCapture = -1; // the pad's back button: no key after all
    } else if (!sWasBusy && (ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
                             ImGui::IsKeyPressed(cancel, false))) {
        SetOpen(false);
    }

    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), io.DisplaySize,
                                                  IM_COL32(16, 0, 62, 160));
    float width = ImGui::GetFontSize() * 54.0f;
    if (width > io.DisplaySize.x * 0.95f) {
        width = io.DisplaySize.x * 0.95f;
    }
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(width, io.DisplaySize.y * 0.9f), ImGuiCond_Always);
    if (sOpening) {
        ImGui::SetNextWindowFocus();
    }
    ImGui::Begin("SETTINGS", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoTitleBar);
    ImGui::TextColored(kYellow, "SETTINGS");
    ImGui::TextWrapped("The game goes on behind this menu. Changes show at once and are "
                       "saved when it closes: F1, Escape, or cross.");
    if (sProblem != nullptr) {
        ImGui::TextColored(kPink, "%s", sProblem);
    }
    PictureSection();
    DreamSection();
    SoundSection();
    KeyboardSection();
    ImGui::Spacing();
    if (ImGui::Button("CLOSE")) {
        SetOpen(false);
    }
    ImGui::End();

    sWasBusy = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) || ImGui::IsAnyItemActive() ||
               sCapture >= 0;
}

bool AnyPadButtonHeld() {
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    bool held = false;
    for (int i = 0; i < count && !held; i++) {
        SDL_Gamepad* pad = SDL_GetGamepadFromID(ids[i]);
        for (int b = 0; pad != nullptr && b < SDL_GAMEPAD_BUTTON_COUNT && !held; b++) {
            held = SDL_GetGamepadButton(pad, (SDL_GamepadButton)b);
        }
    }
    SDL_free(ids);
    return held;
}

// PsyzOverlayFrameCB: builds the frame while the menu is open (and, with
// OpenGL, draws it).
void OnFrame() {
    sDrawn = false;
    if (!sOpen) {
        return;
    }
#ifdef LSD_MENU_SDL3_GPU
    ImGui_ImplSDLGPU3_NewFrame();
#else
    ImGui_ImplOpenGL3_NewFrame();
#endif
    ImGui_ImplSDL3_NewFrame();
    if (sPadHeld) {
        // Dear ImGui reads the pads' buttons as they are, so the circle that
        // picked SETTINGS would count as a press: no gamepad navigation
        // until every button is up.
        sPadHeld = AnyPadButtonHeld();
        ImGuiIO& io = ImGui::GetIO();
        if (sPadHeld) {
            io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
        } else {
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        }
    }
    ScaleToWindow();
    ImGui::NewFrame();
    DrawMenu();
    ImGui::Render();
#ifdef LSD_MENU_SDL3_GPU
    // Its vertices and font go up in a command buffer of its own, ahead of
    // psyz's, which is recording the present and gets the render pass.
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(sDevice);
    if (cmd != nullptr) {
        ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), cmd);
        SDL_SubmitGPUCommandBuffer(cmd);
        sDrawn = true;
    }
#else
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif
}

void SetStyle(ImGuiStyle& style) {
    ImVec4* c = style.Colors;
    auto rgb = [](int r, int g, int b, float a = 1.0f) {
        return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a);
    };
    ImGui::StyleColorsDark(&style);
    style.FontSizeBase = kFontSize;
    style.WindowRounding = style.ChildRounding = style.FrameRounding = 0.0f;
    style.PopupRounding = style.ScrollbarRounding = style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;
    style.WindowBorderSize = 1.0f;
    style.WindowPadding = ImVec2(16.0f, 12.0f);
    style.FramePadding = ImVec2(6.0f, 3.0f);
    style.ItemSpacing = ImVec2(8.0f, 4.0f);
    style.SeparatorTextBorderSize = 2.0f;
    c[ImGuiCol_Text] = kGrey;
    c[ImGuiCol_TextDisabled] = rgb(110, 96, 170);
    c[ImGuiCol_WindowBg] = ImVec4(kBlue.x, kBlue.y, kBlue.z, 0.94f);
    c[ImGuiCol_PopupBg] = rgb(26, 8, 86);
    c[ImGuiCol_Border] = rgb(90, 70, 160);
    c[ImGuiCol_FrameBg] = rgb(32, 16, 96);
    c[ImGuiCol_FrameBgHovered] = rgb(48, 28, 120);
    c[ImGuiCol_FrameBgActive] = rgb(64, 40, 140);
    c[ImGuiCol_Button] = rgb(32, 16, 96);
    c[ImGuiCol_ButtonHovered] = rgb(48, 28, 120);
    c[ImGuiCol_ButtonActive] = rgb(64, 40, 140);
    c[ImGuiCol_Header] = rgb(48, 28, 120);
    c[ImGuiCol_HeaderHovered] = rgb(64, 40, 140);
    c[ImGuiCol_HeaderActive] = rgb(80, 56, 160);
    c[ImGuiCol_CheckMark] = kYellow;
    c[ImGuiCol_SliderGrab] = rgb(156, 156, 0);
    c[ImGuiCol_SliderGrabActive] = kYellow;
    c[ImGuiCol_Separator] = rgb(90, 70, 160);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = rgb(64, 40, 140);
    c[ImGuiCol_ScrollbarGrabHovered] = rgb(80, 56, 160);
    c[ImGuiCol_ScrollbarGrabActive] = rgb(96, 72, 180);
    c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt] = rgb(255, 255, 255, 0.04f);
    c[ImGuiCol_NavCursor] = kYellow;
}

void CreateContext() {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    // Circle (east) confirms and cross (south) goes back, as in the game.
    io.ConfigNavSwapGamepadButtons = true;
    ImFontConfig font;
    font.SizePixels = kFontSize;
    if (LoadGameFont()) {
        font.FontLoader = GameFontLoader();
        font.OversampleH = font.OversampleV = 1;
        font.PixelSnapH = true;
        SDL_strlcpy(font.Name, "FONTICON.TIM", sizeof(font.Name));
        io.Fonts->AddFont(&font);
    } else {
        fprintf(stderr, "lsd: no ETC\\FONTICON.TIM on the disc; the settings menu uses "
                        "Dear ImGui's font\n");
        io.Fonts->AddFontDefaultVector(&font);
    }
    SetStyle(sBaseStyle);
    ImGui::GetStyle() = sBaseStyle;
    sScale = 1.0f;
}

#ifdef LSD_MENU_SDL3_GPU
void OnInit(SDL_Window* window, SDL_GPUDevice* device) {
    CreateContext();
    ImGui_ImplSDL3_InitForSDLGPU(window);
    ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_AutoAll);
    ImGui_ImplSDLGPU3_InitInfo info;
    info.Device = device;
    info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(device, window);
    sDevice = device;
    sReady = ImGui_ImplSDLGPU3_Init(&info);
}

void OnRender(SDL_GPUCommandBuffer* cmd, SDL_GPURenderPass* pass) {
    if (sDrawn) {
        ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), cmd, pass);
    }
}
#else
void OnInit(SDL_Window* window, SDL_GLContext gl) {
    int profile = 0;
    CreateContext();
    ImGui_ImplSDL3_InitForOpenGL(window, gl);
    ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_AutoAll);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profile);
    sReady = ImGui_ImplOpenGL3_Init(profile == SDL_GL_CONTEXT_PROFILE_ES ? "#version 300 es"
                                                                         : "#version 330 core");
}
#endif

void OnDestroy() {
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    if (sOpen) {
        Save();
    }
    if (sReady) {
#ifdef LSD_MENU_SDL3_GPU
        ImGui_ImplSDLGPU3_Shutdown();
#else
        ImGui_ImplOpenGL3_Shutdown();
#endif
    }
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    sReady = false;
    sOpen = false;
}

void OnVSync() {
    if (sNextVSync != nullptr) {
        sNextVSync();
    }
    Settings_Apply();
}

} // namespace

void Menu_Open(void) {
    SetOpen(true);
}

int Menu_IsOpen(void) {
    return sOpen;
}

void SetUpMenu(const char* discPath) {
    sDiscPath = SDL_strdup(discPath);
#ifdef LSD_MENU_SDL3_GPU
    Psyz_OverlayInit_SDL3GPU(OnInit);
    Psyz_OverlayRender_SDL3GPU(OnRender);
#else
    Psyz_OverlayInit_SDL3GL(OnInit);
#endif
    Psyz_OverlayEvent_SDL3(OnEvent);
    Psyz_OverlayFrameCB(OnFrame);
    Psyz_OverlayDestroyCB(OnDestroy);
    sNextVSync = Psyz_SetVSyncCb(OnVSync);
}
