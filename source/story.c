#pragma GCC diagnostic ignored "-Wformat-truncation"

#include "story.h"
#include "save.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool file_exists_on_sd(const char* path) {
    FILE* f = fopen(path, "r");
    if (f) { fclose(f); return true; }
    return false;
}

static void trim_clean(char* str) {
    if (!str) return;
    char* end = str + strlen(str) - 1;
    while (end >= str && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        *end = '\0';
        end--;
    }
}

static void apply_flag_placeholders(char* str, size_t max_len) {
    if (!str || !strchr(str, '{')) return;

    char buffer[256];
    char* src = str;
    char* dst = buffer;
    char* end = buffer + sizeof(buffer) - 1;

    while (*src && dst < end) {
        if (*src == '{') {
            char* close_brace = strchr(src, '}');
            if (close_brace) {
                char flag_name[32];
                size_t len = close_brace - (src + 1);
                if (len < sizeof(flag_name)) {
                    strncpy(flag_name, src + 1, len);
                    flag_name[len] = '\0';

                    const char* val = flag_get(flag_name);
                    if (strlen(val) == 0) {
                        if (!strcmp(flag_name, "build")) val = "creeper";
                        else if (!strcmp(flag_name, "reuben")) val = "withruben";
                        else if (!strcmp(flag_name, "lukas")) val = "withlukas";
                        else if (!strcmp(flag_name, "sword")) val = "withsword";
                        else if (!strcmp(flag_name, "saved")) val = "gabriel";
                    }

                    while (*val && dst < end) {
                        *dst++ = *val++;
                    }
                    src = close_brace + 1;
                    continue;
                }
            }
        }
        *dst++ = *src++;
    }
    *dst = '\0';
    strncpy(str, buffer, max_len - 1);
    str[max_len - 1] = '\0';
}

bool story_load_scene(const char* scene_path, SceneData* out) {
    if (!scene_path || strlen(scene_path) == 0) return false;

    char safe_path[256];
    strncpy(safe_path, scene_path, sizeof(safe_path) - 1);
    safe_path[sizeof(safe_path) - 1] = '\0';
    trim_clean(safe_path);

    apply_flag_placeholders(safe_path, sizeof(safe_path));

    memset(out, 0, sizeof(SceneData));
    out->total_duration = 0.0;
    out->choice_start = -1.0;
    out->choice_duration = 21.0;

    char chapter_dir[128] = "";
    char scene_name[128] = "";

    const char* slash = strchr(safe_path, '/');
    if (slash) {
        size_t dir_len = slash - safe_path;
        if (dir_len >= sizeof(chapter_dir)) dir_len = sizeof(chapter_dir) - 1;
        strncpy(chapter_dir, safe_path, dir_len);
        chapter_dir[dir_len] = '\0';
        strncpy(scene_name, slash + 1, sizeof(scene_name) - 1);
    } else {
        strncpy(scene_name, safe_path, sizeof(scene_name) - 1);
    }

    char raw_video_val[256] = "";
    bool in_target_section = false;
    bool found = false;

    char candidate_txt_paths[4][256];
    int num_candidates = 0;

    if (strlen(chapter_dir) > 0) {
        snprintf(candidate_txt_paths[num_candidates++], 256, "sdmc:/assets/video/%s/%s.txt", chapter_dir, chapter_dir);
        snprintf(candidate_txt_paths[num_candidates++], 256, "sdmc:/assets/video/%s.txt", chapter_dir);
        snprintf(candidate_txt_paths[num_candidates++], 256, "sdmc:/assets/video/%s/chapter.txt", chapter_dir);
    } else {
        snprintf(candidate_txt_paths[num_candidates++], 256, "sdmc:/assets/video/story.txt");
    }

    FILE* fp = NULL;
    for (int i = 0; i < num_candidates; i++) {
        fp = fopen(candidate_txt_paths[i], "r");
        if (fp) break;
    }

    if (fp) {
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            char* l = line;
            while (*l == ' ' || *l == '\t') l++;
            if (*l == '#' || *l == '\0' || *l == '\r' || *l == '\n') continue;

            if (*l == '[') {
                if (in_target_section) break;
                char cur_sec[128];
                if (sscanf(l, "[%127[^]]]", cur_sec) == 1) {
                    trim_clean(cur_sec);
                    apply_flag_placeholders(cur_sec, sizeof(cur_sec));
                    if (!strcmp(cur_sec, scene_name) || !strcmp(cur_sec, safe_path)) {
                        in_target_section = true;
                        found = true;
                    }
                }
                continue;
            }

            if (in_target_section) {
                char key[64], val[256], target[128];

                if (sscanf(l, "set_flag = %31[^:]:%31[^\r\n]", out->set_flag_key, out->set_flag_val) == 2) {
                    trim_clean(out->set_flag_key);
                    trim_clean(out->set_flag_val);
                    flag_set(out->set_flag_key, out->set_flag_val);
                }
                else if (sscanf(l, "next = %127[^\r\n]", target) == 1) {
                    trim_clean(target);
                    apply_flag_placeholders(target, sizeof(target));
                    if (!strchr(target, '/') && strlen(chapter_dir) > 0) {
                        snprintf(out->target_default, sizeof(out->target_default), "%s/%s", chapter_dir, target);
                    } else {
                        strncpy(out->target_default, target, sizeof(out->target_default) - 1);
                    }
                }
                else if (sscanf(l, "video = %255[^\r\n]", raw_video_val) == 1) {
                    trim_clean(raw_video_val);
                    apply_flag_placeholders(raw_video_val, sizeof(raw_video_val));
                }
                else if (sscanf(l, "total_duration = %lf", &out->total_duration) == 1) {}
                else if (sscanf(l, "duration = %lf", &out->choice_duration) == 1) {}
                else if (sscanf(l, "choice_start = %lf", &out->choice_start) == 1) {}
                else if (sscanf(l, "%63[^= ] = %255[^\r\n]", key, val) == 2) {
                    char btn = key[0];
                    char* arrow = strstr(val, "->");
                    if (arrow) {
                        *arrow = '\0';
                        char* opt_target = arrow + 2;
                        while (*opt_target == ' ') opt_target++;
                        trim_clean(val);
                        trim_clean(opt_target);

                        apply_flag_placeholders(opt_target, sizeof(target));

                        char full_opt_target[256];
                        if (!strchr(opt_target, '/') && strlen(chapter_dir) > 0) {
                            snprintf(full_opt_target, sizeof(full_opt_target), "%s/%s", chapter_dir, opt_target);
                        } else {
                            strncpy(full_opt_target, opt_target, sizeof(full_opt_target) - 1);
                        }

                        if (btn == 'Y' || btn == 'y') { strncpy(out->text_y, val, sizeof(out->text_y) - 1); strncpy(out->target_y, full_opt_target, sizeof(out->target_y) - 1); out->has_y = true; }
                        else if (btn == 'X' || btn == 'x') { strncpy(out->text_x, val, sizeof(out->text_x) - 1); strncpy(out->target_x, full_opt_target, sizeof(out->target_x) - 1); out->has_x = true; }
                        else if (btn == 'A' || btn == 'a') { strncpy(out->text_a, val, sizeof(out->text_a) - 1); strncpy(out->target_a, full_opt_target, sizeof(out->target_a) - 1); out->has_a = true; }
                        else if (btn == 'B' || btn == 'b') { strncpy(out->text_b, val, sizeof(out->text_b) - 1); strncpy(out->target_b, full_opt_target, sizeof(out->target_b) - 1); out->has_b = true; }
                    }
                }
            }
        }
        fclose(fp);
    }

    char test_vid[256];
    if (strlen(raw_video_val) > 0) {
        snprintf(test_vid, sizeof(test_vid), "sdmc:/assets/video/%s/%s", chapter_dir, raw_video_val);
        if (file_exists_on_sd(test_vid)) {
            snprintf(out->video_file, sizeof(out->video_file), "%s/%s", chapter_dir, raw_video_val);
        } else {
            strncpy(out->video_file, raw_video_val, sizeof(out->video_file) - 1);
        }
    } else {
        snprintf(test_vid, sizeof(test_vid), "sdmc:/assets/video/%s/%s.ogv", chapter_dir, scene_name);
        if (file_exists_on_sd(test_vid)) {
            snprintf(out->video_file, sizeof(out->video_file), "%s/%s.ogv", chapter_dir, scene_name);
        } else {
            snprintf(out->video_file, sizeof(out->video_file), "%s.ogv", scene_name);
        }
    }

    out->total_choices = (out->has_y ? 1 : 0) + (out->has_x ? 1 : 0) + (out->has_a ? 1 : 0) + (out->has_b ? 1 : 0);
    out->has_choice = (out->total_choices > 0);

    if (strlen(out->target_default) == 0) {
        if (out->has_y && strlen(out->target_y) > 0) strncpy(out->target_default, out->target_y, sizeof(out->target_default) - 1);
        else if (out->has_a && strlen(out->target_a) > 0) strncpy(out->target_default, out->target_a, sizeof(out->target_default) - 1);
    }

    return found;
}

#pragma GCC diagnostic pop
