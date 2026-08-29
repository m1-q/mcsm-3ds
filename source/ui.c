#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "stb_image.h"
#pragma GCC diagnostic pop

static UIImage img_bg;
static UIImage img_logo;
static UIImage img_btn_static;
static UIImage img_btn_selected;
static UIImage img_btn_a;
static UIImage img_btn_b;
static UIImage img_btn_x;
static UIImage img_btn_y;
static UIImage img_controls;

static C2D_Font mc_font = NULL;
static C2D_TextBuf ui_text_buf = NULL;

static inline unsigned bitCeil(unsigned x) {
    return x <= 1 ? 1 : (1u << (32 - __builtin_clz(x - 1)));
}

static inline u32 morton_coord(u32 x, u32 y) {
    u32 idx = 0;
    for (int i = 0; i < 3; i++) {
        idx |= ((x >> i) & 1) << (i * 2 + 0);
        idx |= ((y >> i) & 1) << (i * 2 + 1);
    }
    return idx;
}

static bool load_png_to_image(const char* filepath, UIImage* out) {
    memset(out, 0, sizeof(UIImage));

    int w, h, channels;
    unsigned char* data = stbi_load(filepath, &w, &h, &channels, 4);
    if (!data) return false;

    out->width = w;
    out->height = h;

    u32 tex_w = bitCeil(w);
    u32 tex_h = bitCeil(h);

    C3D_TexInit(&out->tex, tex_w, tex_h, GPU_RGBA8);
    C3D_TexSetFilter(&out->tex, GPU_NEAREST, GPU_NEAREST);

    u8* dst = (u8*)out->tex.data;
    memset(dst, 0, out->tex.size);

    u32 tiles_x = tex_w / 8;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int src_idx = (y * w + x) * 4;
            u8 r = data[src_idx + 0];
            u8 g = data[src_idx + 1];
            u8 b = data[src_idx + 2];
            u8 a = data[src_idx + 3];

            u32 tile_x = x / 8;
            u32 tile_y = y / 8;
            u32 in_tile_x = x % 8;
            u32 in_tile_y = y % 8;

            u32 tile_idx = (tile_y * tiles_x + tile_x) * 64 + morton_coord(in_tile_x, in_tile_y);
            u32 dst_byte = tile_idx * 4;

            dst[dst_byte + 0] = a;
            dst[dst_byte + 1] = b;
            dst[dst_byte + 2] = g;
            dst[dst_byte + 3] = r;
        }
    }

    stbi_image_free(data);

    Tex3DS_SubTexture* subtex = (Tex3DS_SubTexture*)malloc(sizeof(Tex3DS_SubTexture));
    subtex->width = w;
    subtex->height = h;
    subtex->left = 0.0f;
    subtex->top = 1.0f;
    subtex->right = (float)w / (float)tex_w;
    subtex->bottom = 1.0f - ((float)h / (float)tex_h);

    out->img.tex = &out->tex;
    out->img.subtex = subtex;
    out->loaded = true;
    return true;
}

static void free_ui_image(UIImage* img) {
    if (img->loaded) {
        C3D_TexDelete(&img->tex);
        if (img->img.subtex) free((void*)img->img.subtex);
        img->loaded = false;
    }
}

bool ui_init(void) {
    ui_text_buf = C2D_TextBufNew(4096);

    load_png_to_image("sdmc:/assets/pause_bg.png", &img_bg);
    if (!load_png_to_image("sdmc:/assets/logo.png", &img_logo)) {
        load_png_to_image("sdmc:/assets/mainmenu/logo.png", &img_logo);
    }
    load_png_to_image("sdmc:/assets/static.png", &img_btn_static);
    load_png_to_image("sdmc:/assets/selected.png", &img_btn_selected);

    load_png_to_image("sdmc:/assets/btn_a.png", &img_btn_a);
    load_png_to_image("sdmc:/assets/btn_b.png", &img_btn_b);
    load_png_to_image("sdmc:/assets/btn_x.png", &img_btn_x);
    load_png_to_image("sdmc:/assets/btn_y.png", &img_btn_y);

    load_png_to_image("sdmc:/assets/controls.png", &img_controls);

    const char* font_candidates[] = {
        "sdmc:/assets/minecraft.bcfnt",
        "sdmc:/assets/Minecraft Seven_2.bcfnt",
        "sdmc:/assets/Minecraft_Seven_2.bcfnt",
        "sdmc:/assets/font.bcfnt"
    };

    for (size_t i = 0; i < sizeof(font_candidates) / sizeof(font_candidates[0]); i++) {
        FILE* fp = fopen(font_candidates[i], "rb");
        if (fp) {
            fclose(fp);
            mc_font = C2D_FontLoad(font_candidates[i]);
            if (mc_font) break;
        }
    }

    return true;
}

void ui_cleanup(void) {
    if (mc_font) {
        C2D_FontFree(mc_font);
        mc_font = NULL;
    }
    if (ui_text_buf) {
        C2D_TextBufDelete(ui_text_buf);
        ui_text_buf = NULL;
    }
    free_ui_image(&img_bg);
    free_ui_image(&img_logo);
    free_ui_image(&img_btn_static);
    free_ui_image(&img_btn_selected);
    free_ui_image(&img_btn_a);
    free_ui_image(&img_btn_b);
    free_ui_image(&img_btn_x);
    free_ui_image(&img_btn_y);
    free_ui_image(&img_controls);
}

float ui_get_mc_text_width(const char* text, float scale) {
    if (!text || !ui_text_buf) return 0.0f;
    C2D_Text text_obj;
    C2D_TextBufClear(ui_text_buf);
    if (mc_font) C2D_TextFontParse(&text_obj, mc_font, ui_text_buf, text);
    else C2D_TextParse(&text_obj, ui_text_buf, text);
    C2D_TextOptimize(&text_obj);

    float w = 0.0f, h = 0.0f;
    C2D_TextGetDimensions(&text_obj, scale, scale, &w, &h);
    return w;
}

void ui_draw_mc_text(float x, float y, float depth, float scale, const char* text, u32 color, bool with_shadow) {
    if (!text || !ui_text_buf) return;

    u8 a = (u8)((color >> 24) & 0xFF);
    if (a < 5) return;

    C2D_Text text_obj;
    C2D_TextBufClear(ui_text_buf);
    if (mc_font) C2D_TextFontParse(&text_obj, mc_font, ui_text_buf, text);
    else C2D_TextParse(&text_obj, ui_text_buf, text);
    C2D_TextOptimize(&text_obj);

    if (with_shadow) {
        u32 shadow_color = C2D_Color32(35, 35, 35, a);
        C2D_DrawText(&text_obj, C2D_WithColor, x + 1.2f, y + 1.2f, depth - 0.01f, scale, scale, shadow_color);
    }

    C2D_DrawText(&text_obj, C2D_WithColor, x, y, depth, scale, scale, color);
}

void ui_draw_background(void) {
    if (img_bg.loaded) {
        C2D_DrawImageAt(img_bg.img, 0.0f, 0.0f, 0.1f, NULL, 320.0f / (float)img_bg.width, 240.0f / (float)img_bg.height);
    } else {
        C2D_DrawRectSolid(0.0f, 0.0f, 0.1f, 320.0f, 240.0f, C2D_Color32(35, 25, 20, 255));
    }
}

void ui_draw_controls_screen(void) {
    if (img_controls.loaded) {
        C2D_DrawImageAt(img_controls.img, 0.0f, 0.0f, 0.2f, NULL, 320.0f / (float)img_controls.width, 240.0f / (float)img_controls.height);
    } else {
        C2D_DrawRectSolid(0.0f, 0.0f, 0.1f, 320.0f, 240.0f, C2D_Color32(15, 15, 15, 255));
        ui_draw_mc_text(30.0f, 110.0f, 0.25f, 0.55f, "Place controls.png in /assets/", C2D_Color32(200, 200, 200, 255), true);
    }
}

void ui_draw_logo(float center_x, float center_y, float target_w) {
    if (!img_logo.loaded) return;
    float scale = target_w / (float)img_logo.width;
    float draw_w = (float)img_logo.width * scale;
    float draw_h = (float)img_logo.height * scale;
    C2D_DrawImageAt(img_logo.img, center_x - draw_w / 2.0f, center_y - draw_h / 2.0f, 0.8f, NULL, scale, scale);
}

void ui_draw_menu_button(float x, float y, float w, float h, const char* text, bool is_selected) {
    UIImage* btn_img = is_selected ? &img_btn_selected : &img_btn_static;

    if (btn_img->loaded) {
        C2D_DrawImageAt(btn_img->img, x, y, 0.2f, NULL, w / (float)btn_img->width, h / (float)btn_img->height);
    } else {
        u32 bg_col = is_selected ? C2D_Color32(90, 90, 90, 255) : C2D_Color32(60, 60, 60, 255);
        u32 border = is_selected ? C2D_Color32(255, 215, 0, 255) : C2D_Color32(20, 20, 20, 255);
        C2D_DrawRectSolid(x - 2, y - 2, 0.2f, w + 4, h + 4, border);
        C2D_DrawRectSolid(x, y, 0.21f, w, h, bg_col);
    }

    if (text && ui_text_buf) {
        C2D_Text text_obj;
        C2D_TextBufClear(ui_text_buf);
        if (mc_font) C2D_TextFontParse(&text_obj, mc_font, ui_text_buf, text);
        else C2D_TextParse(&text_obj, ui_text_buf, text);
        C2D_TextOptimize(&text_obj);

        float scale = 0.55f;
        float tw = 0.0f, th = 0.0f;
        C2D_TextGetDimensions(&text_obj, scale, scale, &tw, &th);

        float text_x = x + (w - tw) / 2.0f;
        // Сдвигаем на 2.5px выше для идеального оптического центра плашки
        float text_y = y + (h - th) / 2.0f - 2.5f;

        u32 text_col = is_selected ? C2D_Color32(255, 255, 120, 255) : C2D_Color32(225, 225, 225, 255);
        ui_draw_mc_text(text_x, text_y, 0.25f, scale, text, text_col, true);
    }
}

void ui_draw_dialog_choice(float x, float y, float w, float h, const char* btn_type, const char* text, bool is_selected, float alpha) {
    if (alpha <= 0.01f) return;
    if (alpha > 1.0f) alpha = 1.0f;

    u8 a_val = (u8)(alpha * 255.0f);
    C2D_ImageTint tint;
    C2D_AlphaImageTint(&tint, alpha);

    UIImage* btn_img = is_selected ? &img_btn_selected : &img_btn_static;

    if (btn_img->loaded) {
        C2D_DrawImageAt(btn_img->img, x, y, 0.2f, &tint, w / (float)btn_img->width, h / (float)btn_img->height);
    } else {
        u32 bg_col = is_selected ? C2D_Color32(90, 90, 90, a_val) : C2D_Color32(60, 60, 60, a_val);
        u32 border = is_selected ? C2D_Color32(255, 215, 0, a_val) : C2D_Color32(20, 20, 20, a_val);
        C2D_DrawRectSolid(x - 2, y - 2, 0.2f, w + 4, h + 4, border);
        C2D_DrawRectSolid(x, y, 0.21f, w, h, bg_col);
    }

    UIImage* icon = NULL;
    if (!strcmp(btn_type, "A")) icon = &img_btn_a;
    else if (!strcmp(btn_type, "B")) icon = &img_btn_b;
    else if (!strcmp(btn_type, "X")) icon = &img_btn_x;
    else if (!strcmp(btn_type, "Y")) icon = &img_btn_y;

    float icon_size = h > 40.0f ? 28.0f : 24.0f;
    float icon_x = x + 10.0f;
    float icon_y = y + (h - icon_size) / 2.0f;

    if (icon && icon->loaded) {
        C2D_DrawImageAt(icon->img, icon_x, icon_y, 0.25f, &tint, icon_size / (float)icon->width, icon_size / (float)icon->height);
    } else {
        C2D_DrawRectSolid(icon_x, icon_y, 0.24f, icon_size, icon_size, C2D_Color32(60, 60, 60, a_val));
    }

    if (text && ui_text_buf) {
        C2D_Text text_obj;
        C2D_TextBufClear(ui_text_buf);
        if (mc_font) C2D_TextFontParse(&text_obj, mc_font, ui_text_buf, text);
        else C2D_TextParse(&text_obj, ui_text_buf, text);
        C2D_TextOptimize(&text_obj);

        float scale = h > 40.0f ? 0.52f : 0.46f;
        float tw = 0.0f, th = 0.0f;
        C2D_TextGetDimensions(&text_obj, scale, scale, &tw, &th);

        float text_x = x + icon_size + 16.0f;
        float text_y = y + (h - th) / 2.0f - 2.5f;

        u32 text_col = is_selected ? C2D_Color32(255, 255, 120, a_val) : C2D_Color32(245, 245, 245, a_val);
        ui_draw_mc_text(text_x, text_y, 0.25f, scale, text, text_col, true);
    }
}

void ui_draw_xp_timer_bar(float x, float y, float w, float h, double progress, float alpha) {
    if (alpha <= 0.01f) return;
    if (alpha > 1.0f) alpha = 1.0f;

    u8 a_val = (u8)(alpha * 255.0f);

    C2D_DrawRectSolid(x - 2, y - 2, 0.2f, w + 4, h + 4, C2D_Color32(15, 15, 15, a_val));
    C2D_DrawRectSolid(x, y, 0.21f, w, h, C2D_Color32(40, 40, 40, a_val));

    float half_w = (w / 2.0f) * (float)(1.0 - progress);
    float center_x = x + (w / 2.0f);
    float bar_left = center_x - half_w;
    float bar_width = half_w * 2.0f;

    if (bar_width > 0.0f) {
        u32 bar_color;
        if (progress > 0.75) bar_color = C2D_Color32(235, 45, 45, a_val);
        else if (progress > 0.50) bar_color = C2D_Color32(245, 160, 25, a_val);
        else if (progress > 0.25) bar_color = C2D_Color32(235, 225, 45, a_val);
        else bar_color = C2D_Color32(85, 225, 45, a_val);

        C2D_DrawRectSolid(bar_left, y, 0.22f, bar_width, h, bar_color);
    }
}

void ui_draw_top_hint_bar(const char* btn1, const char* text1, const char* btn2, const char* text2) {
    C2D_DrawRectSolid(0.0f, 216.0f, 0.7f, 400.0f, 240.0f, C2D_Color32(0, 0, 0, 160));

    float cur_x = 16.0f;

    if (btn1 && text1) {
        UIImage* icon1 = NULL;
        if (!strcmp(btn1, "A")) icon1 = &img_btn_a;
        else if (!strcmp(btn1, "B")) icon1 = &img_btn_b;
        else if (!strcmp(btn1, "X")) icon1 = &img_btn_x;
        else if (!strcmp(btn1, "Y")) icon1 = &img_btn_y;

        if (icon1 && icon1->loaded) {
            C2D_DrawImageAt(icon1->img, cur_x, 220.0f, 0.75f, NULL, 16.0f / (float)icon1->width, 16.0f / (float)icon1->height);
        }
        cur_x += 20.0f;

        ui_draw_mc_text(cur_x, 220.0f, 0.75f, 0.48f, text1, C2D_Color32(235, 235, 235, 255), true);
        cur_x += ui_get_mc_text_width(text1, 0.48f) + 24.0f;
    }

    if (btn2 && text2) {
        UIImage* icon2 = NULL;
        if (!strcmp(btn2, "A")) icon2 = &img_btn_a;
        else if (!strcmp(btn2, "B")) icon2 = &img_btn_b;
        else if (!strcmp(btn2, "X")) icon2 = &img_btn_x;
        else if (!strcmp(btn2, "Y")) icon2 = &img_btn_y;

        if (icon2 && icon2->loaded) {
            C2D_DrawImageAt(icon2->img, cur_x, 220.0f, 0.75f, NULL, 16.0f / (float)icon2->width, 16.0f / (float)icon2->height);
        }
        cur_x += 20.0f;

        ui_draw_mc_text(cur_x, 220.0f, 0.75f, 0.48f, text2, C2D_Color32(235, 235, 235, 255), true);
    }
}

void ui_draw_fade(float alpha) {
    if (alpha <= 0.0f) return;
    if (alpha > 1.0f) alpha = 1.0f;

    u8 a = (u8)(alpha * 255.0f);
    u32 fade_color = C2D_Color32(0, 0, 0, a);
    C2D_DrawRectSolid(0.0f, 0.0f, 0.99f, 400.0f, 240.0f, fade_color);
}
