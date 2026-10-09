// The settings menu: Dear ImGui drawn over the game's picture, through
// psyz's overlay hooks.
//
// F1, or a gamepad's Guide button or both its sticks pressed in, opens and
// closes it. psyz presents the PlayStation's display, then calls the
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
#ifdef LSD_MENU_SDL3_GPU
#include <imgui_impl_sdlgpu3.h>
#else
#include <imgui_impl_opengl3.h>
#endif

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

namespace {

// The font's height at a 720-pixel-high window; larger windows scale up.
const float kFontSize = 18.0f;
const float kBaseHeight = 720.0f;

bool sOpen;
bool sReady;     // Dear ImGui and its renderer are set up
bool sDrawn;     // this present built a frame to draw (SDL GPU)
bool sWasBusy;   // the last frame had a popup open or an item in use
bool sOpening;   // the first frame since it opened
int sCapture = -1; // the button waiting for a key, or -1
float sScale;
ImGuiStyle sBaseStyle;
const char* sProblem; // why the last save or key failed, until the next
PsyzVSyncCb sNextVSync;
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

// Sizes everything to the window: kFontSize at kBaseHeight, larger above.
void ScaleToWindow() {
    float scale = ImGui::GetIO().DisplaySize.y / kBaseHeight;
    if (scale < 1.0f) {
        scale = 1.0f;
    }
    if (SDL_fabsf(scale - sScale) < 0.01f) {
        return;
    }
    sScale = scale;
    ImGuiStyle& style = ImGui::GetStyle();
    style = sBaseStyle;
    style.ScaleAllSizes(scale);
    style.FontScaleMain = scale;
}

// A row of the menu: its label, then the control to its right.
void Label(const char* label) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(ImGui::GetFontSize() * 9.0f);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 14.0f);
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
    Label(label);
    ImGui::PushID(id);
    ImGui::BeginDisabled(Settings_From(id) != nullptr);
    if (ImGui::BeginCombo("##", shown)) {
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

void Slider(SettingId id, const char* label, int min, int max, const char* format,
            const char* note = nullptr) {
    int value = atoi(Settings_Value(id));
    Label(label);
    ImGui::PushID(id);
    ImGui::BeginDisabled(Settings_From(id) != nullptr);
    if (ImGui::SliderInt("##", &value, min, max, format, ImGuiSliderFlags_AlwaysClamp)) {
        char text[16];
        SDL_snprintf(text, sizeof(text), "%d", value);
        Settings_Set(id, text);
    }
    ImGui::EndDisabled();
    ImGui::PopID();
    Note(id, note);
}

void Check(SettingId id, const char* label, const char* note = nullptr) {
    bool on = SDL_strcmp(Settings_Value(id), "on") == 0;
    Label(label);
    ImGui::PushID(id);
    ImGui::BeginDisabled(Settings_From(id) != nullptr);
    if (ImGui::Checkbox("##", &on)) {
        Settings_Set(id, on ? "on" : "off");
    }
    ImGui::EndDisabled();
    ImGui::PopID();
    Note(id, note);
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
        {"nearest", "nearest", "crisp, some pixels wider"},
        {"smooth", "smooth", "blurred"},
        {"integer", "integer", "whole multiples, bordered"},
    };
    static const Choice colours[] = {
        {"console", "console", "15 bits, as the console"},
        {"full", "full", "24 bits, no dither"},
    };
    static const Choice geometries[] = {
        {"console", "console", "whole pixels, flat textures"},
        {"precise", "precise", "between pixels"},
        {"perspective", "perspective", "precise, textures in perspective"},
    };
    char resolution[32];
    int n = atoi(Settings_Value(SETTING_RESOLUTION));
    SDL_snprintf(resolution, sizeof(resolution), "%%dx (%dx%d)", 320 * n, 240 * n);

    ImGui::SeparatorText("Picture");
    if (sOpening) {
        // Keys and the pad start from the first setting. (The window is
        // not "appearing" again: no frame passes while the menu is shut.)
        ImGui::SetKeyboardFocusHere();
        ImGui::SetNavCursorVisible(true);
        sOpening = false;
    }
    Combo(SETTING_ASPECT, "Aspect", aspects, SDL_arraysize(aspects), nullptr);
    Slider(SETTING_RESOLUTION, "Resolution", 1, 8, resolution, "of the 3D");
    Combo(SETTING_SCALE, "Scale", scales, SDL_arraysize(scales));
    Check(SETTING_DITHER, "Dither", "the console's 4x4 pattern");
    Combo(SETTING_COLOUR, "Colour", colours, SDL_arraysize(colours));
    Combo(SETTING_GEOMETRY, "Geometry", geometries, SDL_arraysize(geometries));
}

void DreamSection() {
    static const Choice rates[] = {
        {"60", "60", "the console's"}, {"display", "display", "its refresh rate"},
        {"30", "30"},   {"72", "72"},   {"75", "75"},   {"90", "90"},   {"100", "100"},
        {"120", "120"}, {"144", "144"}, {"165", "165"}, {"180", "180"}, {"240", "240"},
        {"360", "360"},
    };
    bool smooth = SDL_strcmp(Settings_Value(SETTING_SMOOTH), "on") == 0;

    ImGui::SeparatorText("Dream");
    Slider(SETTING_PACE, "Pace", 10, 30, "%d ticks a second", "14 the console's, 20 the game's");
    Check(SETTING_SMOOTH, "Smooth", "frames drawn between the ticks");
    Combo(SETTING_FRAME_RATE, "Frame rate", rates, SDL_arraysize(rates),
          smooth ? nullptr : "with smooth on");
    Slider(SETTING_DRAW_DISTANCE, "Draw distance", 1, 4, "%dx", "the fog that far away");
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
    ImGui::SeparatorText("Keyboard");
    Label("Layout");
    if (ImGui::BeginCombo("##layout", Controls_LayoutName(Controls_Layout()))) {
        for (int i = 0; i < Controls_LayoutCount(); i++) {
            if (ImGui::Selectable(Controls_LayoutName(i), i == Controls_Layout())) {
                Controls_SetLayout(i);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("every button its keys");

    if (!ImGui::BeginTable("keys", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        return;
    }
    ImGui::TableSetupColumn("Button", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 6.0f);
    ImGui::TableSetupColumn("Keys", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
    for (int b = 0; b < Controls_ButtonCount(); b++) {
        char keys[128];
        ImGui::PushID(b);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(Controls_ButtonName(b));
        ImGui::TableNextColumn();
        if (sCapture == b) {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "press a key...");
        } else {
            KeysText(b, keys, sizeof(keys));
            ImGui::TextUnformatted(keys);
        }
        ImGui::TableNextColumn();
        if (sCapture == b) {
            if (ImGui::Button("Cancel")) {
                sCapture = -1;
            }
        } else {
            if (ImGui::Button("Add key")) {
                sCapture = b;
                sProblem = nullptr;
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
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
                                                  IM_COL32(0, 0, 0, 150));
    float width = ImGui::GetFontSize() * 46.0f;
    if (width > io.DisplaySize.x * 0.95f) {
        width = io.DisplaySize.x * 0.95f;
    }
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(width, io.DisplaySize.y * 0.9f), ImGuiCond_Always);
    if (sOpening) {
        ImGui::SetNextWindowFocus();
    }
    ImGui::Begin("Settings", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextWrapped("The game runs on behind this menu, without your keys or pad. "
                       "Changes show at once and are saved to settings.ini and "
                       "controls.ini when it closes (F1, Escape, or the pad's back "
                       "button).");
    if (sProblem != nullptr) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "%s", sProblem);
    }
    PictureSection();
    DreamSection();
    KeyboardSection();
    ImGui::Spacing();
    if (ImGui::Button("Close")) {
        SetOpen(false);
    }
    ImGui::End();

    sWasBusy = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) || ImGui::IsAnyItemActive() ||
               sCapture >= 0;
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
    io.Fonts->AddFontDefaultVector(&font);
    ImGui::StyleColorsDark(&sBaseStyle);
    sBaseStyle.FontSizeBase = kFontSize;
    sBaseStyle.WindowRounding = 6.0f;
    sBaseStyle.FrameRounding = 3.0f;
    sBaseStyle.WindowPadding = ImVec2(14.0f, 12.0f);
    sBaseStyle.Colors[ImGuiCol_WindowBg].w = 0.92f;
    sBaseStyle.Colors[ImGuiCol_PopupBg].w = 1.0f;
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

void SetUpMenu(void) {
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
