/**********************************************************************************************
*
*   A simple particle editor using libpartikel built with libpartikel, raylib and raygui.
*
*   LICENSE: zlib/libpng
*
*   This program is licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software:
*
*   Copyright (c) 2021 BIAGINI Nathan (@nathBiag on twitter)
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/


#include <raylib.h>
#include <raymath.h>

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#undef RAYGUI_IMPLEMENTATION
#define GUI_FILE_DIALOG_IMPLEMENTATION
#include "gui_file_dialog.h"

#define LIBPARTIKEL_IMPLEMENTATION
#include "partikel.h"

#define EDITOR_WIDTH 1664
#define EDITOR_HEIGHT 936
#define SIMULATION_HEIGHT 450
#define EMITTER_BAR_HEIGHT 25
#define EMITTERS_CONTROLS_HEIGHT (EDITOR_HEIGHT - SIMULATION_HEIGHT)
#define EMITTER_COUNT 8
#define SIMULATION_RECT ((Rectangle){0, TOOLBAR_HEIGHT, EDITOR_WIDTH, SIMULATION_HEIGHT})
#define CONTROLS_RECT ((Rectangle){0, SIMULATION_HEIGHT, EDITOR_WIDTH, EMITTERS_CONTROLS_HEIGHT})
#define SPRITE_EDITOR_SIZE 130
#define SPRITE_EDITOR_RECT ((Rectangle){CONTROLS_RECT.x + SPRITE_EDITOR_SIZE / 2, CONTROLS_RECT.y + EMITTER_BAR_HEIGHT + 10, SPRITE_EDITOR_SIZE, SPRITE_EDITOR_SIZE})
#define SELECTOR_WIDTH 250
#define SELECTOR_HEIGHT 130
#define COLOR_PICKER_WIDTH 200
#define COLOR_PICKER_HEIGHT 100
#define ALPHA_PICKER_WIDTH 200
#define ALPHA_PICKER_HEIGHT 50
#define TOOLBAR_HEIGHT 25

static void UpdateParticleSpriteEditor(void);
static void UpdateParticleSpriteOrigin(Vector2 pos);
static void ProcessInputs(void);
static void DrawUI(void);
static void DrawMetrics(void);
static void DrawToolbar(void);
static void DrawExportPopup(void);
static void DrawEmittersBar(void);
static void DrawParticleSpriteEditor(void);
static void DrawEmittersControls(void);
static void DrawFloatRangeSelector(const char *name, Vector2 pos, FloatRange *val, float min, float max);
static void DrawIntRangeSelector(const char *name, Vector2 pos, IntRange *val, int min, int max);
static void DrawVector2Selector(const char *name, Vector2 pos, Vector2 *val, float min, float max);
static void DrawColorPicker(const char *name, Vector2 pos, Color *color);
static void DrawAlphaPicker(const char *name, Vector2 pos, unsigned char *alpha);
static void InitParticleSystem(void);
static Color HexToRGB(int hex);

// --- Importing/Exporting ---

static bool Export(void);
static bool Import(const char *path);
static int WriteEmitterIntValue(char *str, size_t size, int val);
static int WriteEmitterFloatValue(char *str, size_t size, float val);
static int WriteEmitterFloatRange(char *str, size_t size, FloatRange val);
static int WriteEmitterIntRange(char *str, size_t size, IntRange val);
static int WriteEmitterVector2(char *str, size_t size, Vector2 val);
static int WriteEmitterColor(char *str, size_t size, Color val);
static int WriteEmitterString(char *str, size_t size, const char *to_write);
static int ReadEmitterIntValue(char *str, int *val);
static int ReadEmitterFloatValue(char *str, float *val);
static int ReadEmitterFloatRange(char *str, FloatRange *val);
static int ReadEmitterIntRange(char *str, IntRange *val);
static int ReadEmitterVector2(char *str, Vector2 *val);
static int ReadEmitterColor(char *str, Color *val);
static int ReadEmitterString(char *str, char *read_str);
static const char *GetExportCommentLine(void);

// ---------------------------

// --- UI state ---

typedef struct
{
    unsigned int id;
    char texture_path[512];
    Emitter *emitter;
    RenderTexture2D particle_editor_render_tex;
} EmitterControl;

static EmitterControl emitters[EMITTER_COUNT] = {
    {.id = 0},
    {.id = 1},
    {.id = 2},
    {.id = 3},
    {.id = 4},
    {.id = 5},
    {.id = 6},
    {.id = 7}
};
static EmitterControl *selected_emitter = NULL;
static GuiFileDialogState sprite_dialog_state;
static GuiFileDialogState import_dialog_state;
static unsigned int particle_count = 0;
static bool last_export_res = false;
static bool export_popup = false;
static char selected_file[512] = {0};
static bool has_imported_file = false;

// ----------------

// --- Particle system ---

static EmitterConfig base_cfg = {
    .direction = (Vector2){0, 0},
    .directionAngle = (FloatRange){0, 0},
    .velocityAngle = (FloatRange){0, 0},
    .offset = (FloatRange){0, 0},
    .originAcceleration = (FloatRange){0, 0},
    .burst = (IntRange){1, 1},
    .capacity = 50,
    .emissionRate = 10,
    .origin = (Vector2){0, 0},
    .externalAcceleration = (Vector2){0, 0},
    .baseScale = (Vector2){1, 1},
    .scaleIncrease = (Vector2){0, 0},
    .startColor = WHITE,
    .endColor = WHITE,
    .age = (FloatRange){0.5, 0.5},
    .blendMode = BLEND_ADDITIVE,
    .rotationSpeed = (FloatRange){0, 0}
};

static ParticleSystem *ps = NULL;
static RenderTexture2D simulation_render_tex;

// -----------------------

int main()
{
#ifdef __APPLE__
    SetConfigFlags(FLAG_WINDOW_HIGHDPI);
#endif

    InitWindow(EDITOR_WIDTH, EDITOR_HEIGHT, "Particle editor");

    GuiLoadStyle("../editor_style/cyber.rgs");

    selected_emitter = &emitters[0];

    sprite_dialog_state = InitGuiFileDialog(520, 410, GetWorkingDirectory(), false);
    import_dialog_state = InitGuiFileDialog(520, 410, GetWorkingDirectory(), false);
    simulation_render_tex = LoadRenderTexture(EDITOR_WIDTH, SIMULATION_HEIGHT);

    InitParticleSystem();

    selected_emitter->emitter->isActive = true;

    while (!WindowShouldClose())
    {
        // check for imported file
        if (import_dialog_state.SelectFilePressed)
        {
            memset(selected_file, 0, sizeof(selected_file));
            strcpy(selected_file, TextFormat("%s/%s", import_dialog_state.dirPathText, import_dialog_state.fileNameText));

            bool res = Import(selected_file);

            assert(res); // display error popup

            has_imported_file = true;
            import_dialog_state.SelectFilePressed = false;
        }

        UpdateParticleSpriteEditor();

        BeginTextureMode(simulation_render_tex);
        ClearBackground(BLACK);
        ParticleSystem_Draw(ps);
        EndTextureMode();

        BeginDrawing();

        ClearBackground(BLACK);

        if (!export_popup)
            ProcessInputs();

        particle_count = ParticleSystem_Update(ps, GetFrameTime());

        DrawTexturePro(
            simulation_render_tex.texture,
            (Rectangle){0, 0, simulation_render_tex.texture.width, -simulation_render_tex.texture.height},
            (Rectangle){0, 0, EDITOR_WIDTH, SIMULATION_HEIGHT},
            Vector2Zero(),
            0,
            WHITE);

        DrawUI();
        DrawMetrics();
        DrawToolbar();

        EndDrawing();
    }

    ParticleSystem_Free(ps);
    UnloadRenderTexture(simulation_render_tex);

    CloseWindow();

    return 0;
}

static void UpdateParticleSpriteEditor(void)
{
    // Look for sprite change (file dialog)

    if (sprite_dialog_state.SelectFilePressed)
    {
        // Load image file (if supported extension)
        if (IsFileExtension(sprite_dialog_state.fileNameText, ".png"))
        {
            strcpy(selected_emitter->texture_path, TextFormat("%s/%s", sprite_dialog_state.dirPathText, sprite_dialog_state.fileNameText));

            UnloadTexture(selected_emitter->emitter->config.texture);
            UnloadRenderTexture(selected_emitter->particle_editor_render_tex);

            Texture2D tex = LoadTexture(selected_emitter->texture_path);

            selected_emitter->emitter->config.texture = tex;
            selected_emitter->particle_editor_render_tex = LoadRenderTexture(tex.width, tex.height);
            selected_emitter->emitter->config.textureOrigin = (Vector2){tex.width / 2, tex.height / 2};
        }

        sprite_dialog_state.SelectFilePressed = false;
    }

    Texture2D tex = selected_emitter->emitter->config.texture;
    Vector2 origin = selected_emitter->emitter->config.textureOrigin;

    BeginTextureMode(selected_emitter->particle_editor_render_tex);
    ClearBackground(BLACK);
    DrawTexture(selected_emitter->emitter->config.texture, 0, 0, WHITE);
    DrawLineEx((Vector2){0, origin.y}, (Vector2){tex.width, origin.y}, 2, RED);
    DrawLineEx((Vector2){origin.x, 0}, (Vector2){origin.x, tex.height}, 2, RED);
    EndTextureMode();
}

static void UpdateParticleSpriteOrigin(Vector2 pos)
{
    // Convert mouse click to sprite's coordinates

    Texture2D tex = selected_emitter->emitter->config.texture;
    Vector2 ratio = {tex.width / SPRITE_EDITOR_RECT.width, tex.height / SPRITE_EDITOR_RECT.height};
    Vector2 origin = {(pos.x - SPRITE_EDITOR_RECT.x) * ratio.x, (pos.y - SPRITE_EDITOR_RECT.y) * ratio.y};

    selected_emitter->emitter->config.textureOrigin = origin;
}

static void ProcessInputs(void)
{
    // Emitter selection

    if (IsKeyPressed(KEY_ONE))
    {
        selected_emitter = &emitters[0];
    }
    else if (IsKeyPressed(KEY_TWO))
    {
        selected_emitter = &emitters[1];
    }
    else if (IsKeyPressed(KEY_THREE))
    {
        selected_emitter = &emitters[2];
    }
    else if (IsKeyPressed(KEY_FOUR))
    {
        selected_emitter = &emitters[3];
    }
    else if (IsKeyPressed(KEY_FIVE))
    {
        selected_emitter = &emitters[4];
    }
    else if (IsKeyPressed(KEY_SIX))
    {
        selected_emitter = &emitters[5];
    }
    else if (IsKeyPressed(KEY_SEVEN))
    {
        selected_emitter = &emitters[6];
    }
    else if (IsKeyPressed(KEY_EIGHT))
    {
        selected_emitter = &emitters[7];
    }

    // Particle sprite editor origin change
    if (IsMouseButtonDown(0))
    {
        Vector2 click_pos = GetMousePosition();

        if (CheckCollisionPointRec(click_pos, SPRITE_EDITOR_RECT))
        {
            UpdateParticleSpriteOrigin(click_pos);
            return;
        }
    }

    if (IsMouseButtonPressed(0))
    {
        Vector2 click_pos = GetMousePosition();

        if (!sprite_dialog_state.fileDialogActive && !import_dialog_state.fileDialogActive && CheckCollisionPointRec(click_pos, SIMULATION_RECT))
        {
            ParticleSystem_SetOrigin(ps, click_pos);
            ParticleSystem_Burst(ps);
        }
    }
}

static void DrawUI(void)
{
    GuiEnable();

    if (sprite_dialog_state.fileDialogActive || import_dialog_state.fileDialogActive)
        GuiLock();

    if (export_popup)
    {
        DrawExportPopup();
        GuiDisable();
    }

    GuiPanel((Rectangle){CONTROLS_RECT.x - 2, CONTROLS_RECT.y, CONTROLS_RECT.width + 4, CONTROLS_RECT.height + 2});

    DrawEmittersBar();
    DrawParticleSpriteEditor();
    DrawEmittersControls();
}

static void DrawMetrics(void)
{
    DrawText(TextFormat("FPS: %d", GetFPS()), 0, TOOLBAR_HEIGHT + 5, 15, WHITE);
    DrawText(TextFormat("Particle count: %d", particle_count), 0, TOOLBAR_HEIGHT + 30, 15, WHITE);
}

static void DrawToolbar(void)
{
    Rectangle rect = {0, 0, EDITOR_WIDTH, TOOLBAR_HEIGHT};

    if (GuiButton((Rectangle){rect.x, rect.y, 150, TOOLBAR_HEIGHT}, "Load"))
    {
        import_dialog_state.fileDialogActive = true;
    }

    if (GuiButton((Rectangle){rect.x + 160, rect.y, 150, TOOLBAR_HEIGHT}, "Export"))
    {
        last_export_res = Export();
        export_popup = true;
    }
}

static void DrawExportPopup(void)
{
    Rectangle rect = {SIMULATION_RECT.width / 2 - 250 / 2, SIMULATION_RECT.height / 2 - 50, 250, 65};

    if (GuiWindowBox(rect, "Export"))
        export_popup = false;

    GuiLabel((Rectangle){rect.x, rect.y + 30, rect.width, 30}, last_export_res ? "Success" : "Failed");
}

static void DrawEmittersBar()
{
    Color border_color = HexToRGB(GuiGetStyle(DEFAULT, BORDER_COLOR_FOCUSED));

    DrawLine(CONTROLS_RECT.x, CONTROLS_RECT.y + EMITTER_BAR_HEIGHT, EDITOR_WIDTH, CONTROLS_RECT.y + EMITTER_BAR_HEIGHT, border_color);

    float emitter_width = CONTROLS_RECT.width / EMITTER_COUNT;

    for (int i = 0; i < EMITTER_COUNT; i++)
    {
        Rectangle rect = {i * emitter_width, CONTROLS_RECT.y, emitter_width, EMITTER_BAR_HEIGHT};

        DrawLine(rect.x, rect.y, rect.x, rect.y + EMITTER_BAR_HEIGHT, border_color);

        GuiSetStyle(LABEL, TEXT_ALIGNMENT, GUI_TEXT_ALIGN_CENTER);
        GuiSetStyle(DEFAULT, TEXT_SIZE, 20);
        GuiSetStyle(LABEL, TEXT_COLOR_NORMAL, GuiGetStyle(DEFAULT, i == selected_emitter->id ? TEXT_COLOR_FOCUSED : TEXT_COLOR_NORMAL));
        GuiLabel(rect, TextFormat("%d", i + 1));

        EmitterControl *ec = &emitters[i];

        ec->emitter->isActive = GuiCheckBox((Rectangle){rect.x + rect.width - 30, rect.y + 3, 20, 20}, "", ec->emitter->isActive);
    }
}

static void DrawEmittersControls(void)
{
    selected_emitter->emitter->config.capacity = (int)GuiSlider(
        (Rectangle){CONTROLS_RECT.x + SPRITE_EDITOR_RECT.width + 225, CONTROLS_RECT.y + 40, 175, 20},
        "Capacity", "",
        selected_emitter->emitter->config.capacity,
        1, 5000);

    GuiLabel(
        (Rectangle){CONTROLS_RECT.x + SPRITE_EDITOR_RECT.width + 225 + 120, CONTROLS_RECT.y + 40, 175, 20},
        TextFormat("%d", selected_emitter->emitter->config.capacity));

    // Emitter config controls

    // Below the sprite editor

    DrawVector2Selector(
        "Base scale",
        (Vector2){SPRITE_EDITOR_RECT.x - SPRITE_EDITOR_RECT.width / 2 + 20, SPRITE_EDITOR_RECT.y + SPRITE_EDITOR_RECT.height + 40},
        &selected_emitter->emitter->config.baseScale,
        0, 1);

    DrawVector2Selector(
        "Scale increase",
        (Vector2){SPRITE_EDITOR_RECT.x - SPRITE_EDITOR_RECT.width / 2 + 20, SPRITE_EDITOR_RECT.y + SPRITE_EDITOR_RECT.height + SELECTOR_HEIGHT + 50},
        &selected_emitter->emitter->config.scaleIncrease,
        0, 1);

    // First column

    float x = CONTROLS_RECT.x + SELECTOR_WIDTH + 30;
    float y = CONTROLS_RECT.y + EMITTER_BAR_HEIGHT + 30;

    DrawFloatRangeSelector(
        "Direction angle",
        (Vector2){x, y + 10},
        &selected_emitter->emitter->config.directionAngle,
        -100, 100);

    DrawFloatRangeSelector(
        "Velocity angle",
        (Vector2){x, y + SELECTOR_HEIGHT + 20},
        &selected_emitter->emitter->config.velocityAngle,
        -100, 100);

    DrawVector2Selector(
        "Acceleration",
        (Vector2){x, y + (SELECTOR_HEIGHT * 2) + 30},
        &selected_emitter->emitter->config.externalAcceleration,
        -2000, 2000);

    // Second column

    x = CONTROLS_RECT.x + SELECTOR_WIDTH * 2 + 40;

    DrawFloatRangeSelector(
        "Velocity",
        (Vector2){x, y + 10},
        &selected_emitter->emitter->config.velocity,
        0, 1000);

    DrawVector2Selector(
        "Direction",
        (Vector2){x, y + SELECTOR_HEIGHT + 20},
        &selected_emitter->emitter->config.direction,
        -1, 1);

    DrawFloatRangeSelector(
        "Rotation speed",
        (Vector2){x, y + (SELECTOR_HEIGHT * 2) + 30},
        &selected_emitter->emitter->config.rotationSpeed,
        0, 1000);

    // Third column

    x = CONTROLS_RECT.x + SELECTOR_WIDTH * 3 + 50;

    DrawIntRangeSelector(
        "Burst",
        (Vector2){x, y + 10},
        &selected_emitter->emitter->config.burst,
        0, 500);

    DrawFloatRangeSelector(
        "Life time",
        (Vector2){x, y + SELECTOR_HEIGHT + 20},
        &selected_emitter->emitter->config.age,
        0, 30);

    // Fourth column

    x = CONTROLS_RECT.x + SELECTOR_WIDTH * 4 + 60;

    DrawColorPicker(
        "Start color",
        (Vector2){x, y + 10},
        &selected_emitter->emitter->config.startColor);
    DrawColorPicker(
        "End color",
        (Vector2){x, y + COLOR_PICKER_HEIGHT + 30},
        &selected_emitter->emitter->config.endColor);
    DrawAlphaPicker(
        "Start alpha",
        (Vector2){x, y + (COLOR_PICKER_HEIGHT * 2) + 55},
        &selected_emitter->emitter->config.startColor.a);
    DrawAlphaPicker(
        "End alpha",
        (Vector2){x, y + (COLOR_PICKER_HEIGHT * 2 + ALPHA_PICKER_HEIGHT) + 80},
        &selected_emitter->emitter->config.endColor.a);

    GuiUnlock();

    GuiFileDialog(&sprite_dialog_state);
    GuiFileDialog(&import_dialog_state);
}

static void DrawParticleSpriteEditor(void)
{
    Texture2D tex = selected_emitter->particle_editor_render_tex.texture;

    GuiPanel(SPRITE_EDITOR_RECT);

    DrawTexturePro(
        tex,
        (Rectangle){0, 0, tex.width, -tex.height},
        SPRITE_EDITOR_RECT,
        Vector2Zero(),
        0,
        WHITE);

    if (GuiButton((Rectangle){SPRITE_EDITOR_RECT.x, SPRITE_EDITOR_RECT.y + SPRITE_EDITOR_RECT.height + 10, SPRITE_EDITOR_RECT.width, 20}, "Change sprite"))
    {
        sprite_dialog_state.fileDialogActive = true;
    }
}

static void DrawFloatRangeSelector(const char *name, Vector2 pos, FloatRange *val, float min, float max)
{
    Rectangle rect = {pos.x, pos.y, SELECTOR_WIDTH, SELECTOR_HEIGHT};

    GuiPanel(rect);
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, GUI_TEXT_ALIGN_CENTER);
    GuiLabel((Rectangle){rect.x, rect.y, rect.width, 30}, name);

    // min

    GuiSetStyle(LABEL, TEXT_ALIGNMENT, GUI_TEXT_ALIGN_CENTER);
    GuiLabel((Rectangle){rect.x, rect.y + 30, rect.width, 30}, TextFormat("%.03f", val->min));

    val->min = GuiSlider((Rectangle){rect.x + 40, rect.y + 55, rect.width - 80, 20}, "min", "", val->min, min, max);

    if (GuiButton((Rectangle){rect.x + (rect.width - 30), rect.y + 55, 20, 20}, "0"))
        val->min = 0;

    // max

    GuiSetStyle(LABEL, TEXT_ALIGNMENT, GUI_TEXT_ALIGN_CENTER);
    GuiLabel((Rectangle){rect.x, rect.y + 75, rect.width, 30}, TextFormat("%.02f", val->max));

    val->max = GuiSlider((Rectangle){rect.x + 40, rect.y + 100, rect.width - 80, 20}, "max", "", val->max, min, max);

    if (GuiButton((Rectangle){rect.x + (rect.width - 30), rect.y + 100, 20, 20}, "0"))
        val->max = 0;
}

static void DrawIntRangeSelector(const char *name, Vector2 pos, IntRange *val, int min, int max)
{
    Rectangle rect = {pos.x, pos.y, SELECTOR_WIDTH, SELECTOR_HEIGHT};

    GuiPanel(rect);
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, GUI_TEXT_ALIGN_CENTER);
    GuiLabel((Rectangle){rect.x, rect.y, rect.width, 30}, name);

    // min

    GuiSetStyle(LABEL, TEXT_ALIGNMENT, GUI_TEXT_ALIGN_CENTER);
    GuiLabel((Rectangle){rect.x, rect.y + 30, rect.width, 30}, TextFormat("%d", val->min));

    val->min = (int)GuiSlider((Rectangle){rect.x + 40, rect.y + 55, rect.width - 80, 20}, "min", "", (int)val->min, min, max);

    if (GuiButton((Rectangle){rect.x + (rect.width - 30), rect.y + 55, 20, 20}, "0"))
        val->min = 0;

    // max

    GuiSetStyle(LABEL, TEXT_ALIGNMENT, GUI_TEXT_ALIGN_CENTER);
    GuiLabel((Rectangle){rect.x, rect.y + 75, rect.width, 30}, TextFormat("%d", val->max));

    val->max = (int)GuiSlider((Rectangle){rect.x + 40, rect.y + 100, rect.width - 80, 20}, "max", "", (int)val->max, min, max);

    if (GuiButton((Rectangle){rect.x + (rect.width - 30), rect.y + 100, 20, 20}, "0"))
        val->max = 0;
}

static void DrawVector2Selector(const char *name, Vector2 pos, Vector2 *val, float min, float max)
{
    Rectangle rect = {pos.x, pos.y, SELECTOR_WIDTH, SELECTOR_HEIGHT};

    GuiPanel(rect);
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, GUI_TEXT_ALIGN_CENTER);
    GuiLabel((Rectangle){rect.x, rect.y, rect.width, 30}, name);

    // min

    GuiSetStyle(LABEL, TEXT_ALIGNMENT, GUI_TEXT_ALIGN_CENTER);
    GuiLabel((Rectangle){rect.x, rect.y + 30, rect.width, 30}, TextFormat("%.02f", val->x));

    val->x = GuiSlider((Rectangle){rect.x + 40, rect.y + 55, rect.width - 80, 20}, "X", "", val->x, min, max);

    if (GuiButton((Rectangle){rect.x + (rect.width - 30), rect.y + 55, 20, 20}, "0"))
        val->x = 0;

    // max

    GuiSetStyle(LABEL, TEXT_ALIGNMENT, GUI_TEXT_ALIGN_CENTER);
    GuiLabel((Rectangle){rect.x, rect.y + 75, rect.width, 30}, TextFormat("%.02f", val->y));

    val->y = GuiSlider((Rectangle){rect.x + 40, rect.y + 100, rect.width - 80, 20}, "Y", "", val->y, min, max);

    if (GuiButton((Rectangle){rect.x + (rect.width - 30), rect.y + 100, 20, 20}, "0"))
        val->y = 0;
}

static void DrawColorPicker(const char *name, Vector2 pos, Color *color)
{
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, GUI_TEXT_ALIGN_CENTER);
    GuiLabel((Rectangle){pos.x, pos.y, COLOR_PICKER_WIDTH, 20}, name);

    *color = GuiColorPicker((Rectangle){pos.x, pos.y + 20, COLOR_PICKER_WIDTH, COLOR_PICKER_HEIGHT}, *color);
}

static void DrawAlphaPicker(const char *name, Vector2 pos, unsigned char *alpha)
{
    float v = *alpha / 255.0;

    GuiSetStyle(LABEL, TEXT_ALIGNMENT, GUI_TEXT_ALIGN_CENTER);
    GuiLabel((Rectangle){pos.x, pos.y, ALPHA_PICKER_WIDTH, 20}, name);

    v = GuiColorBarAlpha((Rectangle){pos.x, pos.y + 20, ALPHA_PICKER_WIDTH, ALPHA_PICKER_HEIGHT}, v);

    *alpha = v * 255;
}

static void InitParticleSystem(void)
{
    ps = ParticleSystem_New();

    for (int i = 0; i < EMITTER_COUNT; i++)
    {
        EmitterControl *ec = &emitters[i];

        memcpy(ec->texture_path, "../particles/default.png", strlen("../particles/default.png"));

        base_cfg.texture = LoadTexture(ec->texture_path);
        base_cfg.textureOrigin = (Vector2){base_cfg.texture.width / 2, base_cfg.texture.height / 2};

        ec->emitter = Emitter_New(base_cfg);
        ec->emitter->isActive = false;
        ec->particle_editor_render_tex = LoadRenderTexture(base_cfg.texture.width, base_cfg.texture.height);

        ParticleSystem_Register(ps, ec->emitter);
    }
}

static Color HexToRGB(int hex)
{
    Color color;

    color.r = (hex >> 16) & 0xFF;
    color.g = (hex >> 8) & 0xFF;
    color.b = hex & 0xFF;
    color.a = 255;

    return color;
}

static bool Export(void)
{
    // TODO: open a popup to select file name when no file has been imported
    FILE *f = fopen(has_imported_file ? selected_file : "foobar", "w");

    if (!f)
        return false;

    if (fputs(GetExportCommentLine(), f) < 0)
        return false;

    for (int i = 0; i < EMITTER_COUNT; i++)
    {
        EmitterControl *ec = &emitters[i];
        Emitter *e = ec->emitter;

        char emitter_str[512] = {0};
        int cursor = 0;
        int len;

        if ((len = WriteEmitterIntValue(emitter_str + cursor, sizeof(emitter_str) - cursor - 1, (int)e->isActive)) < 0)
            goto write_error;

        cursor += len;

        if ((len = WriteEmitterVector2(emitter_str + cursor, sizeof(emitter_str) - cursor - 1, e->config.direction)) < 0)
            goto write_error;

        cursor += len;

        if ((len = WriteEmitterFloatRange(emitter_str + cursor, sizeof(emitter_str) - cursor - 1, e->config.velocity)) < 0)
            goto write_error;

        cursor += len;

        if ((len = WriteEmitterFloatRange(emitter_str + cursor, sizeof(emitter_str) - cursor - 1, e->config.directionAngle)) < 0)
            goto write_error;

        cursor += len;

        if ((len = WriteEmitterFloatRange(emitter_str + cursor, sizeof(emitter_str) - cursor - 1, e->config.velocityAngle)) < 0)
            goto write_error;

        cursor += len;

        if ((len = WriteEmitterFloatRange(emitter_str + cursor, sizeof(emitter_str) - cursor - 1, e->config.offset)) < 0)
            goto write_error;

        cursor += len;

        if ((len = WriteEmitterFloatRange(emitter_str + cursor, sizeof(emitter_str) - cursor - 1, e->config.originAcceleration)) < 0)
            goto write_error;

        cursor += len;

        if ((len = WriteEmitterIntRange(emitter_str + cursor, sizeof(emitter_str) - cursor - 1, e->config.burst)) < 0)
            goto write_error;

        cursor += len;

        if ((len = WriteEmitterIntValue(emitter_str + cursor, sizeof(emitter_str) - cursor - 1, e->config.capacity)) < 0)
            goto write_error;

        cursor += len;

        if ((len = WriteEmitterVector2(emitter_str + cursor, sizeof(emitter_str) - cursor - 1, e->config.origin)) < 0)
            goto write_error;

        cursor += len;

        if ((len = WriteEmitterVector2(emitter_str + cursor, sizeof(emitter_str) - cursor - 1, e->config.externalAcceleration)) < 0)
            goto write_error;

        cursor += len;

        if ((len = WriteEmitterVector2(emitter_str + cursor, sizeof(emitter_str) - cursor - 1, e->config.baseScale)) < 0)
            goto write_error;

        cursor += len;

        if ((len = WriteEmitterVector2(emitter_str + cursor, sizeof(emitter_str) - cursor - 1, e->config.scaleIncrease)) < 0)
            goto write_error;

        cursor += len;

        if ((len = WriteEmitterColor(emitter_str + cursor, sizeof(emitter_str) - cursor - 1, e->config.startColor)) < 0)
            goto write_error;

        cursor += len;

        if ((len = WriteEmitterColor(emitter_str + cursor, sizeof(emitter_str) - cursor - 1, e->config.endColor)) < 0)
            goto write_error;

        cursor += len;

        if ((len = WriteEmitterFloatRange(emitter_str + cursor, sizeof(emitter_str) - cursor - 1, e->config.age)) < 0)
            goto write_error;

        cursor += len;

        if ((len = WriteEmitterFloatValue(emitter_str + cursor, sizeof(emitter_str) - cursor - 1, e->config.baseRotation)) < 0)
            goto write_error;

        cursor += len;

        if ((len = WriteEmitterFloatRange(emitter_str + cursor, sizeof(emitter_str) - cursor - 1, e->config.rotationSpeed)) < 0)
            goto write_error;

        cursor += len;

        if ((len = WriteEmitterVector2(emitter_str + cursor, sizeof(emitter_str) - cursor - 1, e->config.textureOrigin)) < 0)
            goto write_error;

        cursor += len;

        if ((len = WriteEmitterString(emitter_str + cursor, sizeof(emitter_str) - cursor - 1, ec->texture_path)) < 0)
            goto write_error;

        if (fputs(emitter_str, f) < 0)
            goto write_error;
        if (fputs("\n", f) < 0)
            goto write_error;
    }

    return true;

write_error:
    fclose(f);

    return false;
}

static bool Import(const char *path)
{
    FILE *f = fopen(path, "r");

    if (!f)
        return false;

    char *line = NULL;
    size_t line_len = 0;
    int len = 0;
    int cursor = 0;
    int i = 0;

    while (getline(&line, &line_len, f) != -1)
    {
        if (line[0] == '#')
            continue;

        EmitterControl *ec = &emitters[i];
        Emitter *e = ec->emitter;
        char *tokens[32];

        char *token = strtok(line, "|");
        int token_count = 0;

        while (token != NULL)
        {
            tokens[token_count++] = token;
            token = strtok(NULL, "|");
        }

        int is_active;

        if (ReadEmitterIntValue(tokens[0], &is_active) < 0)
            goto read_error;

        e->isActive = is_active;

        if (ReadEmitterVector2(tokens[1], &e->config.direction) < 0)
            goto read_error;

        if (ReadEmitterFloatRange(tokens[2], &e->config.velocity) < 0)
            goto read_error;

        if (ReadEmitterFloatRange(tokens[3], &e->config.directionAngle) < 0)
            goto read_error;

        if (ReadEmitterFloatRange(tokens[4], &e->config.velocityAngle) < 0)
            goto read_error;

        if (ReadEmitterFloatRange(tokens[5], &e->config.offset) < 0)
            goto read_error;

        if (ReadEmitterFloatRange(tokens[6], &e->config.originAcceleration) < 0)
            goto read_error;

        if (ReadEmitterIntRange(tokens[7], &e->config.burst) < 0)
            goto read_error;

        if (ReadEmitterIntValue(tokens[8], (int *)&e->config.capacity) < 0)
            goto read_error;

        if (ReadEmitterVector2(tokens[9], &e->config.origin) < 0)
            goto read_error;

        if (ReadEmitterVector2(tokens[10], &e->config.externalAcceleration) < 0)
            goto read_error;

        if (ReadEmitterVector2(tokens[11], &e->config.baseScale) < 0)
            goto read_error;

        if (ReadEmitterVector2(tokens[12], &e->config.scaleIncrease) < 0)
            goto read_error;

        if (ReadEmitterColor(tokens[13], &e->config.startColor) < 0)
            goto read_error;

        if (ReadEmitterColor(tokens[14], &e->config.endColor) < 0)
            goto read_error;

        if (ReadEmitterFloatRange(tokens[15], &e->config.age) < 0)
            goto read_error;

        if (ReadEmitterFloatValue(tokens[16], &e->config.baseRotation) < 0)
            goto read_error;

        if (ReadEmitterFloatRange(tokens[17], &e->config.rotationSpeed) < 0)
            goto read_error;

        if (ReadEmitterVector2(tokens[18], &e->config.textureOrigin) < 0)
            goto read_error;

        if (ReadEmitterString(tokens[19], ec->texture_path) < 0)
            goto read_error;

        UnloadTexture(ec->emitter->config.texture);
        UnloadRenderTexture(ec->particle_editor_render_tex);

        Texture2D tex = LoadTexture(ec->texture_path);

        ec->emitter->config.texture = tex;
        ec->particle_editor_render_tex = LoadRenderTexture(tex.width, tex.height);

        i++;
    }

    return true;

read_error:
    fclose(f);

    return false;
}

static int WriteEmitterIntValue(char *str, size_t size, int val)
{
    char val_str[32] = {0};

    int len = snprintf(val_str, sizeof(val_str), "%d|", val);

    if (len > size)
        return -1;

    strncpy(str, val_str, size);

    return len;
}

static int WriteEmitterFloatValue(char *str, size_t size, float val)
{
    char val_str[32] = {0};

    int len = snprintf(val_str, sizeof(val_str), "%f|", val);

    if (len > size)
        return -1;

    strncpy(str, val_str, size);

    return len;
}

static int WriteEmitterFloatRange(char *str, size_t size, FloatRange val)
{
    char val_str[32] = {0};

    int len = snprintf(val_str, sizeof(val_str), "%.03f,%.03f|", val.min, val.max);

    if (len > size)
        return -1;

    strncpy(str, val_str, size);

    return len;
}

static int WriteEmitterIntRange(char *str, size_t size, IntRange val)
{
    char val_str[32] = {0};

    int len = snprintf(val_str, sizeof(val_str), "%d,%d|", val.min, val.max);

    if (len > size)
        return -1;

    strncpy(str, val_str, size);

    return len;
}

static int WriteEmitterVector2(char *str, size_t size, Vector2 val)
{
    char val_str[32] = {0};

    int len = snprintf(val_str, sizeof(val_str), "%.03f,%.03f|", val.x, val.y);

    if (len > size)
        return -1;

    strncpy(str, val_str, size);

    return len;
}

static int WriteEmitterColor(char *str, size_t size, Color val)
{
    char val_str[32] = {0};

    int len = snprintf(val_str, sizeof(val_str), "%d,%d,%d,%d|", val.r, val.g, val.b, val.a);

    if (len > size)
        return -1;

    strncpy(str, val_str, size);

    return len;
}

static int WriteEmitterString(char *str, size_t size, const char *to_write)
{
    if (strlen(to_write) > size)
        return -1;

    return snprintf(str, strlen(to_write) + 1, "%s|", to_write);
}

static int ReadEmitterIntValue(char *str, int *val)
{
    int len = sscanf(str, "%d", val);

    if (!len)
        return -1;

    return len;
}

static int ReadEmitterFloatValue(char *str, float *val)
{
    int len = sscanf(str, "%f", val);

    if (!len)
        return -1;

    return len;
}

static int ReadEmitterFloatRange(char *str, FloatRange *val)
{
    int len = sscanf(str, "%f,%f", &val->min, &val->max);

    if (!len)
        return -1;

    return len;
}

static int ReadEmitterIntRange(char *str, IntRange *val)
{
    int len = sscanf(str, "%d,%d", &val->min, &val->max);

    if (!len)
        return -1;

    return len;
}

static int ReadEmitterVector2(char *str, Vector2 *val)
{
    int len = sscanf(str, "%f,%f", &val->x, &val->y);

    if (!len)
        return -1;

    return len;
}

static int ReadEmitterColor(char *str, Color *val)
{
    int len = sscanf(str, "%hhu,%hhu,%hhu,%hhu", &val->r, &val->g, &val->b, &val->a);

    if (!len)
        return -1;

    return len;
}

static int ReadEmitterString(char *str, char *read_str)
{
    int len = sscanf(str, "%s", read_str);

    if (!len)
        return -1;

    return len;
}

static const char *GetExportCommentLine(void)
{
    return "# is active | direction | velocity | direction angle | velocity angle | offset | \
origin acceleration | burst | capacity | origin | external acceleration | base scale | scale increase | \
start color | end color | life time | base rotation | rotation speed | texture origin | texture path\n";
}