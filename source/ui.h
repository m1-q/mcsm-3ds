#pragma once

#include <3ds.h>
#include <citro2d.h>
#include <stdbool.h>

typedef struct {
    C3D_Tex tex;
    C2D_Image img;
    bool loaded;
    int width;
    int height;
} UIImage;

typedef struct {
    float x, y, w, h;
} UIRect;

bool ui_init(void);
void ui_cleanup(void);

void ui_draw_background(void);
void ui_draw_logo(float center_x, float center_y, float target_w);
void ui_draw_menu_button(float x, float y, float w, float h, const char* text, bool is_selected);
void ui_draw_dialog_choice(float x, float y, float w, float h, const char* btn_type, const char* text, bool is_selected, float alpha);
void ui_draw_xp_timer_bar(float x, float y, float w, float h, double progress, float alpha);
void ui_draw_fade(float alpha);
void ui_draw_top_hint_bar(const char* btn1, const char* text1, const char* btn2, const char* text2);
void ui_draw_controls_screen(void);

// Нативный рендер Minecraft BCFNT шрифта
void ui_draw_mc_text(float x, float y, float depth, float scale, const char* text, u32 color, bool with_shadow);
float ui_get_mc_text_width(const char* text, float scale);
