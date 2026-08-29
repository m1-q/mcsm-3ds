#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <3ds.h>
#include <citro3d.h>
#include <citro2d.h>

#include "player.h"
#include "story.h"
#include "ui.h"
#include "save.h"

#define SCREEN_WIDTH  400
#define SCREEN_HEIGHT 240

typedef enum {
	STATE_TITLE,
	STATE_FADE_OUT_TITLE,
	STATE_LOADING_TO_MENU,

	STATE_MAINMENU,
	STATE_MENU_PLAY,
	STATE_MENU_SETTINGS,
	STATE_MENU_GAMEPLAY,
	STATE_MENU_AUDIO,
	STATE_MENU_CONTROLS,
	STATE_MENU_DEBUG,
	STATE_MENU_CREDITS,

	STATE_FADE_OUT_MENU,
	STATE_LOADING_TO_GAME,
	STATE_GAME,
	STATE_PAUSE,

	STATE_EPISODE_CREDITS,
	STATE_FADE_OUT_CREDITS
} AppState;

typedef enum {
	CHOICE_FADE_IN,
	CHOICE_ACTIVE,
	CHOICE_FADE_OUT
} ChoiceUIState;

void enter_title_screen(void);
void enter_main_menu(void);
void play_scene(const char* scene_name);
void trigger_choice(void);
void start_choice_fade_out(const char* next_target);
void setup_choice_layout(void);

static AppState app_state = STATE_TITLE;
static AppState settings_return_state = STATE_MAINMENU;
static int menu_selected_idx = 0;
static float global_fade_alpha = 0.0f;

static SceneData current_scene;
static u64 scene_start_time = 0;
static u64 choice_start_time = 0;
static u64 pause_start_tick = 0;

static ChoiceUIState choice_ui_state = CHOICE_ACTIVE;
static float choice_ui_alpha = 0.0f;
static char pending_next_scene[64] = "";
static int choice_selected_idx = 0;

static bool choice_active = false;
static bool was_playing = false;

static UIRect choice_rects[4];
static const char* choice_types[4];
static const char* choice_texts[4];
static const char* choice_targets[4];

C3D_RenderTarget* top;
C3D_RenderTarget* bottom;

void setup_choice_layout(void) {
	int count = current_scene.total_choices;
	if (count <= 0) return;
	int idx = 0;
	choice_selected_idx = 0;

	float btn_w = 280.0f;
	float btn_x = 20.0f;
	float btn_h = (count == 2) ? 52.0f : ((count == 3) ? 44.0f : 38.0f);
	float gap = (count == 2) ? 16.0f : ((count == 3) ? 10.0f : 6.0f);
	float start_y = (count == 2) ? 42.0f : ((count == 3) ? 24.0f : 12.0f);

	if (current_scene.has_y && idx < count) { choice_rects[idx] = (UIRect){btn_x, start_y + idx*(btn_h+gap), btn_w, btn_h}; choice_types[idx] = "Y"; choice_texts[idx] = current_scene.text_y; choice_targets[idx] = current_scene.target_y; idx++; }
	if (current_scene.has_x && idx < count) { choice_rects[idx] = (UIRect){btn_x, start_y + idx*(btn_h+gap), btn_w, btn_h}; choice_types[idx] = "X"; choice_texts[idx] = current_scene.text_x; choice_targets[idx] = current_scene.target_x; idx++; }
	if (current_scene.has_a && idx < count) { choice_rects[idx] = (UIRect){btn_x, start_y + idx*(btn_h+gap), btn_w, btn_h}; choice_types[idx] = "A"; choice_texts[idx] = current_scene.text_a; choice_targets[idx] = current_scene.target_a; idx++; }
	if (current_scene.has_b && idx < count) { choice_rects[idx] = (UIRect){btn_x, start_y + idx*(btn_h+gap), btn_w, btn_h}; choice_types[idx] = "B"; choice_texts[idx] = current_scene.text_b; choice_targets[idx] = current_scene.target_b; idx++; }
}

void play_scene(const char* scene_name) {
	if (!scene_name || strlen(scene_name) == 0) return;

	// Проверяем наличие слова "credits" в любом варианте пути (например ch6_nether/credits или credits)
	if (strstr(scene_name, "credits") != NULL) {
		app_state = STATE_EPISODE_CREDITS;
		choice_active = false;
		was_playing = false;
		global_fade_alpha = 0.0f;
		if (!player_start_stream("credits.ogv", false)) {
			enter_main_menu();
		}
		return;
	}

	char target_copy[128];
	strncpy(target_copy, scene_name, sizeof(target_copy) - 1);
	target_copy[sizeof(target_copy) - 1] = '\0';

	choice_active = false;
	was_playing = false;
	choice_ui_alpha = 0.0f;

	if (story_load_scene(target_copy, &current_scene)) {
		save_game_write(target_copy);

		if (current_scene.has_choice) {
			if (current_scene.total_duration > current_scene.choice_duration) {
				current_scene.choice_start = current_scene.total_duration - current_scene.choice_duration;
			} else if (current_scene.total_duration > 0.0) {
				current_scene.choice_start = 0.0;
				current_scene.choice_duration = current_scene.total_duration;
			}
		}

		if (player_start_stream(current_scene.video_file, false)) {
			scene_start_time = osGetTime();
		}
	}
}

void trigger_choice(void) {
	choice_active = true;
	choice_start_time = osGetTime();
	choice_ui_state = CHOICE_FADE_IN;
	choice_ui_alpha = 0.0f;
	setup_choice_layout();

	if (g_config.timer_mode == TIMER_INFINITE) {
		player_pause(true);
	}
}

void start_choice_fade_out(const char* next_target) {
	if (!next_target || strlen(next_target) == 0) return;
	if (g_config.timer_mode == TIMER_INFINITE) {
		player_pause(false);
	}
	strncpy(pending_next_scene, next_target, sizeof(pending_next_scene) - 1);
	choice_ui_state = CHOICE_FADE_OUT;
}

void enter_title_screen(void) {
	app_state = STATE_TITLE;
	global_fade_alpha = 0.0f;
	player_start_stream("mcsm_title.ogv", true);
}

void enter_main_menu(void) {
	app_state = STATE_MAINMENU;
	settings_return_state = STATE_MAINMENU;
	menu_selected_idx = 0;
	global_fade_alpha = 1.0f; // Плавный Fade-In меню
	player_start_stream("mcsm_mainmenu.ogv", true);
}

int main(int argc, char* argv[]) {
	romfsInit();
	ndspInit();
	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	C2D_Prepare();

	osSetSpeedupEnable(true);

	top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

	player_init();
	save_system_init();
	ui_init();

	enter_title_screen();

	while (aptMainLoop()) {
		hidScanInput();
		u32 kDown = hidKeysDown();
		u32 kHeld = hidKeysHeld();
		touchPosition touch;
		hidTouchRead(&touch);

		// Плавный Fade-In при открытии меню
		if (app_state == STATE_MAINMENU && global_fade_alpha > 0.0f) {
			global_fade_alpha -= 0.05f;
			if (global_fade_alpha < 0.0f) global_fade_alpha = 0.0f;
		}

		// Секретный дебаг L + R + X
		if ((kHeld & KEY_L) && (kHeld & KEY_R) && (kDown & KEY_X)) {
			g_config.debug_unlocked = !g_config.debug_unlocked;
			save_config_write();
		}

		// ----------------------------------------------------
		// 1. ТИТУЛЬНЫЙ ЭКРАН
		// ----------------------------------------------------
		if (app_state == STATE_TITLE) {
			if ((kDown & KEY_START) || (kDown & KEY_A) || (kDown & KEY_TOUCH)) app_state = STATE_FADE_OUT_TITLE;
		}
		else if (app_state == STATE_FADE_OUT_TITLE) {
			global_fade_alpha += 0.06f;
			if (global_fade_alpha >= 1.0f) {
				global_fade_alpha = 0.0f;
				app_state = STATE_LOADING_TO_MENU;
				if (!player_start_stream("loading.ogv", false)) enter_main_menu();
			}
		}
		else if (app_state == STATE_LOADING_TO_MENU) {
			if (!player_is_playing()) enter_main_menu();
		}
		// ----------------------------------------------------
		// 2. ГЛАВНОЕ МЕНЮ
		// ----------------------------------------------------
		else if (app_state == STATE_MAINMENU) {
			int count = 4;
			if (kDown & KEY_DOWN) menu_selected_idx = (menu_selected_idx + 1) % count;
			if (kDown & KEY_UP) menu_selected_idx = (menu_selected_idx + count - 1) % count;

			if (kDown & KEY_TOUCH) {
				if (touch.py >= 20 && touch.py <= 62) { menu_selected_idx = 0; kDown |= KEY_A; }
				else if (touch.py >= 70 && touch.py <= 112) { menu_selected_idx = 1; kDown |= KEY_A; }
				else if (touch.py >= 120 && touch.py <= 162) { menu_selected_idx = 2; kDown |= KEY_A; }
				else if (touch.py >= 170 && touch.py <= 212) { menu_selected_idx = 3; kDown |= KEY_A; }
			}

			if (kDown & KEY_A) {
				if (menu_selected_idx == 0) { app_state = STATE_MENU_PLAY; menu_selected_idx = 0; }
				else if (menu_selected_idx == 1) { settings_return_state = STATE_MAINMENU; app_state = STATE_MENU_SETTINGS; menu_selected_idx = 0; }
				else if (menu_selected_idx == 2) { app_state = STATE_MENU_CREDITS; }
				else if (menu_selected_idx == 3) { break; }
			}
		}
		// ----------------------------------------------------
		// 3. МЕНЮ PLAY
		// ----------------------------------------------------
		else if (app_state == STATE_MENU_PLAY) {
			bool can_continue = save_game_exists();
			int count = 3;
			if (kDown & KEY_DOWN) menu_selected_idx = (menu_selected_idx + 1) % count;
			if (kDown & KEY_UP) menu_selected_idx = (menu_selected_idx + count - 1) % count;
			if (kDown & KEY_B) { app_state = STATE_MAINMENU; menu_selected_idx = 0; }

			if (kDown & KEY_TOUCH) {
				if (touch.py >= 35 && touch.py <= 81) { menu_selected_idx = 0; kDown |= KEY_A; }
				else if (touch.py >= 95 && touch.py <= 141) { menu_selected_idx = 1; kDown |= KEY_A; }
				else if (touch.py >= 155 && touch.py <= 201) { menu_selected_idx = 2; kDown |= KEY_A; }
			}

			if (kDown & KEY_A) {
				if (menu_selected_idx == 0 && can_continue) app_state = STATE_FADE_OUT_MENU;
				else if (menu_selected_idx == 1) { g_save.has_save = false; flags_clear(); save_game_write("ch1_treehouse/01_intro"); app_state = STATE_FADE_OUT_MENU; }
				else if (menu_selected_idx == 2) app_state = STATE_FADE_OUT_MENU;
			}
		}
		// ----------------------------------------------------
		// 4. МЕНЮ НАСТРОЕК
		// ----------------------------------------------------
		else if (app_state == STATE_MENU_SETTINGS) {
			int count = g_config.debug_unlocked ? 4 : 3;
			if (kDown & KEY_DOWN) menu_selected_idx = (menu_selected_idx + 1) % count;
			if (kDown & KEY_UP) menu_selected_idx = (menu_selected_idx + count - 1) % count;

			if (kDown & KEY_B) {
				save_config_write();
				app_state = settings_return_state;
				menu_selected_idx = (settings_return_state == STATE_PAUSE) ? 1 : 1;
			}

			if (kDown & KEY_TOUCH) {
				if (touch.py >= 25 && touch.py <= 70) { menu_selected_idx = 0; kDown |= KEY_A; }
				else if (touch.py >= 75 && touch.py <= 120) { menu_selected_idx = 1; kDown |= KEY_A; }
				else if (touch.py >= 125 && touch.py <= 170) { menu_selected_idx = 2; kDown |= KEY_A; }
				else if (g_config.debug_unlocked && touch.py >= 175 && touch.py <= 220) { menu_selected_idx = 3; kDown |= KEY_A; }
			}

			if (kDown & KEY_A) {
				if (menu_selected_idx == 0) { app_state = STATE_MENU_GAMEPLAY; menu_selected_idx = 0; }
				else if (menu_selected_idx == 1) { app_state = STATE_MENU_AUDIO; menu_selected_idx = 0; }
				else if (menu_selected_idx == 2) { app_state = STATE_MENU_CONTROLS; }
				else if (menu_selected_idx == 3 && g_config.debug_unlocked) { app_state = STATE_MENU_DEBUG; menu_selected_idx = 0; }
			}
		}
		else if (app_state == STATE_MENU_GAMEPLAY) {
			if (kDown & KEY_B) { save_config_write(); app_state = STATE_MENU_SETTINGS; menu_selected_idx = 0; }
			if (kDown & KEY_DOWN || kDown & KEY_UP) menu_selected_idx = 1 - menu_selected_idx;
			if (kDown & (KEY_A | KEY_RIGHT | KEY_LEFT)) {
				if (menu_selected_idx == 0) g_config.timer_mode = (g_config.timer_mode + 1) % 3;
				else if (menu_selected_idx == 1) g_config.text_lang = 1 - g_config.text_lang;
				save_config_write();
			}
		}
		else if (app_state == STATE_MENU_AUDIO) {
			if (kDown & KEY_B) { save_config_write(); app_state = STATE_MENU_SETTINGS; menu_selected_idx = 1; }
			if (kDown & KEY_DOWN || kDown & KEY_UP) menu_selected_idx = 1 - menu_selected_idx;
			if (kDown & (KEY_A | KEY_RIGHT | KEY_LEFT)) {
				if (menu_selected_idx == 0) g_config.voice_lang = 1 - g_config.voice_lang;
				else if (menu_selected_idx == 1) g_config.subs_mode = (g_config.subs_mode + 1) % 3;
				save_config_write();
			}
		}
		else if (app_state == STATE_MENU_DEBUG) {
			if (kDown & KEY_B) { save_config_write(); app_state = STATE_MENU_SETTINGS; menu_selected_idx = 3; }
			if (kDown & (KEY_A | KEY_RIGHT | KEY_LEFT)) {
				g_config.skip_scenes_enabled = !g_config.skip_scenes_enabled;
				save_config_write();
			}
		}
		else if (app_state == STATE_MENU_CONTROLS) {
			if ((kDown & KEY_B) || (kDown & KEY_TOUCH) || (kDown & KEY_A)) { app_state = STATE_MENU_SETTINGS; menu_selected_idx = 2; }
		}
		else if (app_state == STATE_MENU_CREDITS) {
			if ((kDown & KEY_B) || (kDown & KEY_TOUCH) || (kDown & KEY_A)) { app_state = STATE_MAINMENU; menu_selected_idx = 2; }
		}
		else if (app_state == STATE_FADE_OUT_MENU) {
			global_fade_alpha += 0.06f;
			if (global_fade_alpha >= 1.0f) {
				global_fade_alpha = 0.0f;
				app_state = STATE_LOADING_TO_GAME;
				if (!player_start_stream("loading.ogv", false)) {
					app_state = STATE_GAME;
					play_scene(save_game_exists() ? g_save.current_scene : "ch1_treehouse/01_intro");
				}
			}
		}
		else if (app_state == STATE_LOADING_TO_GAME) {
			if (!player_is_playing()) {
				app_state = STATE_GAME;
				play_scene(save_game_exists() ? g_save.current_scene : "ch1_treehouse/01_intro");
			}
		}
		// ----------------------------------------------------
		// 5. ТИТРЫ ЭПИЗОДА (Защита от моментального сброса)
		// ----------------------------------------------------
		else if (app_state == STATE_EPISODE_CREDITS) {
			if (player_is_playing()) {
				was_playing = true;
			}

			// Скипаем только если ролик реально играл и закончился, ИЛИ нажата кнопка пользователем
			if ((kDown & (KEY_START | KEY_A | KEY_B | KEY_X | KEY_TOUCH)) || (was_playing && !player_is_playing())) {
				app_state = STATE_FADE_OUT_CREDITS;
			}
		}
		else if (app_state == STATE_FADE_OUT_CREDITS) {
			global_fade_alpha += 0.06f;
			if (global_fade_alpha >= 1.0f) {
				global_fade_alpha = 0.0f;
				app_state = STATE_LOADING_TO_MENU;
				if (!player_start_stream("loading.ogv", false)) {
					enter_main_menu();
				}
			}
		}
		// ----------------------------------------------------
		// 6. ИГРОВОЙ ПРОЦЕСС
		// ----------------------------------------------------
		else if (app_state == STATE_GAME) {
			double elapsed = (double)(osGetTime() - scene_start_time) / 1000.0;

			if (kDown & KEY_START) {
				app_state = STATE_PAUSE;
				player_pause(true);
				menu_selected_idx = 0;
				pause_start_tick = osGetTime();
				continue;
			}

			if ((kDown & (KEY_X | KEY_R)) && !choice_active && g_config.skip_scenes_enabled) {
				char next_target[128] = "";
				if (strlen(current_scene.target_default) > 0) strncpy(next_target, current_scene.target_default, sizeof(next_target) - 1);
				else if (current_scene.has_y && strlen(current_scene.target_y) > 0) strncpy(next_target, current_scene.target_y, sizeof(next_target) - 1);
				else if (current_scene.has_a && strlen(current_scene.target_a) > 0) strncpy(next_target, current_scene.target_a, sizeof(next_target) - 1);

				if (strlen(next_target) > 0) {
					play_scene(next_target);
					continue;
				}
			}

			if (player_is_playing()) was_playing = true;

			if (player_is_playing() && current_scene.has_choice && !choice_active && current_scene.choice_start >= 0.0) {
				if (elapsed >= current_scene.choice_start) {
					trigger_choice();
				}
			}
			else if (was_playing && !player_is_playing() && current_scene.has_choice) {
				was_playing = false;
				if (g_config.timer_mode != TIMER_INFINITE) {
					start_choice_fade_out(choice_targets[choice_selected_idx]);
				}
			}
			else if (was_playing && !player_is_playing() && !current_scene.has_choice) {
				was_playing = false;
				if (strlen(current_scene.target_default) > 0) {
					play_scene(current_scene.target_default);
					continue;
				} else {
					enter_main_menu();
					continue;
				}
			}

			if (choice_active) {
				if (choice_ui_state == CHOICE_FADE_IN) {
					choice_ui_alpha += 0.08f;
					if (choice_ui_alpha >= 1.0f) { choice_ui_alpha = 1.0f; choice_ui_state = CHOICE_ACTIVE; }
				}
				else if (choice_ui_state == CHOICE_FADE_OUT) {
					choice_ui_alpha -= 0.08f;
					if (choice_ui_alpha <= 0.0f) {
						choice_ui_alpha = 0.0f;
						choice_active = false;
						play_scene(pending_next_scene);
						continue;
					}
				}
				else if (choice_ui_state == CHOICE_ACTIVE) {
					double choice_elapsed = (double)(osGetTime() - choice_start_time) / 1000.0;
					double duration = current_scene.choice_duration;
					if (g_config.timer_mode == TIMER_EXTENDED) duration *= 1.5;

					double progress = (g_config.timer_mode == TIMER_INFINITE) ? 0.0 : ((duration > 0.0) ? (choice_elapsed / duration) : 1.0);

					if (kDown & KEY_DOWN) choice_selected_idx = (choice_selected_idx + 1) % current_scene.total_choices;
					if (kDown & KEY_UP) choice_selected_idx = (choice_selected_idx + current_scene.total_choices - 1) % current_scene.total_choices;

					if (kDown & KEY_A) start_choice_fade_out(choice_targets[choice_selected_idx]);
					if ((kDown & KEY_Y) && current_scene.has_y) start_choice_fade_out(current_scene.target_y);
					else if ((kDown & KEY_X) && current_scene.has_x) start_choice_fade_out(current_scene.target_x);
					else if ((kDown & KEY_B) && current_scene.has_b) start_choice_fade_out(current_scene.target_b);

					if (kDown & KEY_TOUCH) {
						for (int i = 0; i < current_scene.total_choices; i++) {
							UIRect r = choice_rects[i];
							if (touch.px >= r.x && touch.px <= (r.x + r.w) && touch.py >= r.y && touch.py <= (r.y + r.h)) {
								choice_selected_idx = i;
								start_choice_fade_out(choice_targets[i]);
								break;
							}
						}
					}

					if (g_config.timer_mode != TIMER_INFINITE && progress >= 1.0) {
						start_choice_fade_out(choice_targets[choice_selected_idx]);
					}
				}
			}
		}
		// ----------------------------------------------------
		// 7. МЕНЮ ПАУЗЫ
		// ----------------------------------------------------
		else if (app_state == STATE_PAUSE) {
			if (kDown & KEY_DOWN) menu_selected_idx = (menu_selected_idx + 1) % 3;
			if (kDown & KEY_UP) menu_selected_idx = (menu_selected_idx + 2) % 3;

			if (kDown & KEY_TOUCH) {
				if (touch.py >= 35 && touch.py <= 81) { menu_selected_idx = 0; kDown |= KEY_A; }
				else if (touch.py >= 95 && touch.py <= 141) { menu_selected_idx = 1; kDown |= KEY_A; }
				else if (touch.py >= 155 && touch.py <= 201) { menu_selected_idx = 2; kDown |= KEY_A; }
			}

			if ((kDown & (KEY_START | KEY_B)) || ((kDown & KEY_A) && menu_selected_idx == 0)) {
				u64 pause_duration = osGetTime() - pause_start_tick;
				scene_start_time += pause_duration;
				choice_start_time += pause_duration;
				player_pause(false);
				app_state = STATE_GAME;
			} else if (kDown & KEY_A && menu_selected_idx == 1) {
				settings_return_state = STATE_PAUSE;
				app_state = STATE_MENU_SETTINGS;
				menu_selected_idx = 0;
			} else if ((kDown & KEY_A) && menu_selected_idx == 2) {
				enter_main_menu();
			}
		}

		// ----------------------------------------------------
		// ОТРЕНДЕРИТЬ ВЕРХНИЙ ЭКРАН
		// ----------------------------------------------------
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
		C2D_SceneBegin(top);

		player_draw_frame(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);

		bool is_in_pause_menu = (app_state == STATE_PAUSE || (settings_return_state == STATE_PAUSE &&
		(app_state == STATE_MENU_SETTINGS || app_state == STATE_MENU_GAMEPLAY ||
		app_state == STATE_MENU_AUDIO || app_state == STATE_MENU_DEBUG ||
		app_state == STATE_MENU_CONTROLS)));

		if (is_in_pause_menu) {
			C2D_DrawRectSolid(0.0f, 0.0f, 0.6f, 400.0f, 240.0f, C2D_Color32(0, 0, 0, 130));
			if (app_state == STATE_PAUSE) {
				ui_draw_top_hint_bar("A", (g_config.text_lang == LANG_RUSSIAN ? "Выбрать" : "Select"),
									 "B", (g_config.text_lang == LANG_RUSSIAN ? "Продолжить" : "Resume"));
			} else {
				ui_draw_top_hint_bar("A", (g_config.text_lang == LANG_RUSSIAN ? "Выбрать" : "Select"),
									 "B", (g_config.text_lang == LANG_RUSSIAN ? "Назад" : "Back"));
			}
		}
		else if (app_state == STATE_EPISODE_CREDITS) {
			ui_draw_top_hint_bar("A", (g_config.text_lang == LANG_RUSSIAN ? "Пропустить" : "Skip"), NULL, NULL);
		}
		else {
			if (app_state == STATE_MAINMENU || app_state == STATE_MENU_PLAY ||
				app_state == STATE_MENU_SETTINGS || app_state == STATE_MENU_GAMEPLAY ||
				app_state == STATE_MENU_AUDIO || app_state == STATE_MENU_DEBUG ||
				app_state == STATE_MENU_CREDITS || app_state == STATE_MENU_CONTROLS) {
				ui_draw_logo(200.0f, 48.0f, 250.0f);
				}

				if (app_state == STATE_MAINMENU) {
					ui_draw_top_hint_bar("A", (g_config.text_lang == LANG_RUSSIAN ? "Выбрать" : "Select"), NULL, NULL);
				} else if (app_state == STATE_MENU_PLAY || app_state == STATE_MENU_SETTINGS ||
					app_state == STATE_MENU_GAMEPLAY || app_state == STATE_MENU_AUDIO ||
					app_state == STATE_MENU_DEBUG) {
					ui_draw_top_hint_bar("A", (g_config.text_lang == LANG_RUSSIAN ? "Выбрать" : "Select"),
										 "B", (g_config.text_lang == LANG_RUSSIAN ? "Назад" : "Back"));
					} else if (app_state == STATE_MENU_CONTROLS || app_state == STATE_MENU_CREDITS) {
						ui_draw_top_hint_bar("B", (g_config.text_lang == LANG_RUSSIAN ? "Назад" : "Back"), NULL, NULL);
					}
		}

		if (global_fade_alpha > 0.0f) ui_draw_fade(global_fade_alpha);

		// ----------------------------------------------------
		// ОТРЕНДЕРИТЬ НИЖНИЙ ЭКРАН
		// ----------------------------------------------------
		C2D_TargetClear(bottom, C2D_Color32(0, 0, 0, 255));
		C2D_SceneBegin(bottom);

		bool is_ru = (g_config.text_lang == LANG_RUSSIAN);

		if (app_state == STATE_TITLE || app_state == STATE_EPISODE_CREDITS) {
			// Чистый черный экран на титрах и титульнике
		}
		else if (app_state == STATE_MAINMENU) {
			ui_draw_background();
			ui_draw_menu_button(30.0f, 20.0f, 260.0f, 42.0f, is_ru ? "ИГРАТЬ" : "PLAY", menu_selected_idx == 0);
			ui_draw_menu_button(30.0f, 70.0f, 260.0f, 42.0f, is_ru ? "НАСТРОЙКИ" : "SETTINGS", menu_selected_idx == 1);
			ui_draw_menu_button(30.0f, 120.0f, 260.0f, 42.0f, is_ru ? "АВТОРЫ" : "CREDITS", menu_selected_idx == 2);
			ui_draw_menu_button(30.0f, 170.0f, 260.0f, 42.0f, is_ru ? "ВЫХОД" : "EXIT GAME", menu_selected_idx == 3);
		}
		else if (app_state == STATE_MENU_PLAY) {
			ui_draw_background();
			ui_draw_menu_button(30.0f, 35.0f, 260.0f, 46.0f, save_game_exists() ? (is_ru ? "ПРОДОЛЖИТЬ" : "CONTINUE") : (is_ru ? "[ПРОДОЛЖИТЬ]" : "[CONTINUE]"), menu_selected_idx == 0);
			ui_draw_menu_button(30.0f, 95.0f, 260.0f, 46.0f, is_ru ? "НОВАЯ ИГРА" : "NEW GAME", menu_selected_idx == 1);
			ui_draw_menu_button(30.0f, 155.0f, 260.0f, 46.0f, is_ru ? "ЭПИЗОДЫ" : "EPISODES", menu_selected_idx == 2);
		}
		else if (app_state == STATE_MENU_SETTINGS) {
			ui_draw_background();
			ui_draw_menu_button(30.0f, 25.0f, 260.0f, 44.0f, is_ru ? "ГЕЙМПЛЕЙ" : "GAMEPLAY", menu_selected_idx == 0);
			ui_draw_menu_button(30.0f, 75.0f, 260.0f, 44.0f, is_ru ? "ВИДЕО И ЗВУК" : "VIDEO & AUDIO", menu_selected_idx == 1);
			ui_draw_menu_button(30.0f, 125.0f, 260.0f, 44.0f, is_ru ? "УПРАВЛЕНИЕ" : "CONTROLS", menu_selected_idx == 2);
			if (g_config.debug_unlocked) {
				ui_draw_menu_button(30.0f, 175.0f, 260.0f, 44.0f, "[DEBUG MENU]", menu_selected_idx == 3);
			}
		}
		else if (app_state == STATE_MENU_GAMEPLAY) {
			ui_draw_background();
			char buf_timer[64], buf_lang[64];
			const char* timer_names_ru[] = { "Таймер: Обычный", "Таймер: Увеличенный", "Таймер: Без времени" };
			const char* timer_names_en[] = { "Timer: Normal", "Timer: Extended", "Timer: Infinite" };
			snprintf(buf_timer, sizeof(buf_timer), "%s", is_ru ? timer_names_ru[g_config.timer_mode] : timer_names_en[g_config.timer_mode]);
			snprintf(buf_lang, sizeof(buf_lang), "%s: %s", is_ru ? "Язык" : "Language", is_ru ? "Русский" : "English");

			ui_draw_menu_button(30.0f, 45.0f, 260.0f, 46.0f, buf_timer, menu_selected_idx == 0);
			ui_draw_menu_button(30.0f, 105.0f, 260.0f, 46.0f, buf_lang, menu_selected_idx == 1);
		}
		else if (app_state == STATE_MENU_AUDIO) {
			ui_draw_background();
			char buf_voice[64], buf_subs[64];
			const char* subs_names_ru[] = { "Субтитры: Выкл", "Субтитры: English", "Субтитры: Русский" };
			const char* subs_names_en[] = { "Subtitles: OFF", "Subtitles: English", "Subtitles: Russian" };
			snprintf(buf_voice, sizeof(buf_voice), "%s: %s", is_ru ? "Озвучка" : "Voice", g_config.voice_lang == LANG_RUSSIAN ? "Русский" : "English");
			snprintf(buf_subs, sizeof(buf_subs), "%s", is_ru ? subs_names_ru[g_config.subs_mode] : subs_names_en[g_config.subs_mode]);

			ui_draw_menu_button(30.0f, 45.0f, 260.0f, 46.0f, buf_voice, menu_selected_idx == 0);
			ui_draw_menu_button(30.0f, 105.0f, 260.0f, 46.0f, buf_subs, menu_selected_idx == 1);
		}
		else if (app_state == STATE_MENU_DEBUG) {
			ui_draw_background();
			char buf_skip[64];
			snprintf(buf_skip, sizeof(buf_skip), "Skip Cutscenes: %s", g_config.skip_scenes_enabled ? "ON [X/R]" : "OFF");
			ui_draw_menu_button(30.0f, 80.0f, 260.0f, 46.0f, buf_skip, true);
		}
		else if (app_state == STATE_MENU_CONTROLS) {
			ui_draw_controls_screen();
		}
		else if (app_state == STATE_MENU_CREDITS) {
			ui_draw_background();
			C2D_DrawRectSolid(20.0f, 35.0f, 0.2f, 280.0f, 170.0f, C2D_Color32(0, 0, 0, 190));
			ui_draw_mc_text(40.0f, 50.0f, 0.25f, 0.65f, is_ru ? "АВТОРЫ" : "CREDITS", C2D_Color32(255, 215, 0, 255), true);
			ui_draw_mc_text(40.0f, 80.0f, 0.25f, 0.55f, "Original: Telltale & Mojang", C2D_Color32(230, 230, 230, 255), true);
			ui_draw_mc_text(40.0f, 110.0f, 0.25f, 0.55f, "PSP Port: entitybtw", C2D_Color32(230, 230, 230, 255), true);
			ui_draw_mc_text(40.0f, 140.0f, 0.25f, 0.55f, "3DS Port: m1_q", C2D_Color32(230, 230, 230, 255), true);
			ui_draw_mc_text(40.0f, 170.0f, 0.25f, 0.55f, "Engine: Citro2D + Theora", C2D_Color32(180, 180, 180, 255), true);
		}
		else if (app_state == STATE_PAUSE) {
			ui_draw_background();
			ui_draw_menu_button(30.0f, 35.0f, 260.0f, 46.0f, is_ru ? "ПРОДОЛЖИТЬ" : "RESUME", menu_selected_idx == 0);
			ui_draw_menu_button(30.0f, 95.0f, 260.0f, 46.0f, is_ru ? "НАСТРОЙКИ" : "SETTINGS", menu_selected_idx == 1);
			ui_draw_menu_button(30.0f, 155.0f, 260.0f, 46.0f, is_ru ? "В ГЛАВНОЕ МЕНЮ" : "QUIT TO MENU", menu_selected_idx == 2);
		}
		else if (app_state == STATE_GAME) {
			ui_draw_background();

			if (choice_active) {
				double choice_elapsed = (double)(osGetTime() - choice_start_time) / 1000.0;
				double duration = current_scene.choice_duration;
				if (g_config.timer_mode == TIMER_EXTENDED) duration *= 1.5;

				double progress = (g_config.timer_mode == TIMER_INFINITE) ? 0.0 : ((duration > 0.0) ? (choice_elapsed / duration) : 1.0);

				for (int i = 0; i < current_scene.total_choices; i++) {
					UIRect r = choice_rects[i];
					bool is_highlighted = (choice_selected_idx == i);
					ui_draw_dialog_choice(r.x, r.y, r.w, r.h, choice_types[i], choice_texts[i], is_highlighted, choice_ui_alpha);
				}

				if (g_config.timer_mode != TIMER_INFINITE) {
					float timer_y = (current_scene.total_choices == 2) ? 180.0f : 188.0f;
					ui_draw_xp_timer_bar(30.0f, timer_y, 260.0f, 8.0f, progress, choice_ui_alpha);
				}
			}
		}

		if (global_fade_alpha > 0.0f) ui_draw_fade(global_fade_alpha);

		C3D_FrameEnd(0);
	}

	player_cleanup();
	ui_cleanup();
	osSetSpeedupEnable(false);
	gfxExit();
	ndspExit();
	romfsExit();
	return 0;
}
