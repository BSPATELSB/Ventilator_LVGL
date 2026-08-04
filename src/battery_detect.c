#include "battery_detect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>

static char cached_bat_path[256] = {0};
static char cached_ac_path[256] = {0};

/* Helper to trim trailing whitespace/newlines from a string */
static void trim_string(char * str)
{
    if(!str) return;
    int len = strlen(str);
    while(len > 0 && (str[len-1] == '\n' || str[len-1] == '\r' || isspace((unsigned char)str[len-1]))) {
        str[len-1] = '\0';
        len--;
    }
}

/* Find path to primary battery device in /sys/class/power_supply/ */
static bool find_battery_path(char * out_path, size_t max_len)
{
    if(cached_bat_path[0] != '\0') {
        if(access(cached_bat_path, F_OK) == 0) {
            snprintf(out_path, max_len, "%s", cached_bat_path);
            return true;
        }
        cached_bat_path[0] = '\0';
    }

    DIR * dir = opendir("/sys/class/power_supply");
    if(!dir) return false;

    struct dirent * entry;
    while((entry = readdir(dir)) != NULL) {
        if(entry->d_name[0] == '.') continue;

        char path[256];
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s", entry->d_name);

        /* Check if device type is "Battery" or if "capacity" file exists */
        char cap_path[280];
        snprintf(cap_path, sizeof(cap_path), "%s/capacity", path);
        if(access(cap_path, R_OK) == 0) {
            snprintf(cached_bat_path, sizeof(cached_bat_path), "%s", path);
            snprintf(out_path, max_len, "%s", path);
            closedir(dir);
            return true;
        }

        /* Check type file */
        char type_path[280];
        snprintf(type_path, sizeof(type_path), "%s/type", path);
        FILE * ftype = fopen(type_path, "r");
        if(ftype) {
            char type_buf[32] = {0};
            if(fgets(type_buf, sizeof(type_buf), ftype)) {
                trim_string(type_buf);
                if(strcasecmp(type_buf, "Battery") == 0) {
                    fclose(ftype);
                    snprintf(cached_bat_path, sizeof(cached_bat_path), "%s", path);
                    snprintf(out_path, max_len, "%s", path);
                    closedir(dir);
                    return true;
                }
            }
            fclose(ftype);
        }
    }

    closedir(dir);
    return false;
}

/* Find path to primary AC / Mains supply in /sys/class/power_supply/ */
static bool find_ac_path(char * out_path, size_t max_len)
{
    if(cached_ac_path[0] != '\0') {
        if(access(cached_ac_path, F_OK) == 0) {
            snprintf(out_path, max_len, "%s", cached_ac_path);
            return true;
        }
        cached_ac_path[0] = '\0';
    }

    DIR * dir = opendir("/sys/class/power_supply");
    if(!dir) return false;

    struct dirent * entry;
    while((entry = readdir(dir)) != NULL) {
        if(entry->d_name[0] == '.') continue;

        char path[256];
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s", entry->d_name);

        char online_path[280];
        snprintf(online_path, sizeof(online_path), "%s/online", path);
        if(access(online_path, R_OK) == 0) {
            /* Verify if it is AC / Mains */
            if(strncasecmp(entry->d_name, "AC", 2) == 0 || strncasecmp(entry->d_name, "ADP", 3) == 0 || strncasecmp(entry->d_name, "MAINS", 5) == 0) {
                snprintf(cached_ac_path, sizeof(cached_ac_path), "%s", path);
                snprintf(out_path, max_len, "%s", path);
                closedir(dir);
                return true;
            }
        }
    }

    closedir(dir);
    return false;
}

void battery_detect_init(void)
{
    cached_bat_path[0] = '\0';
    cached_ac_path[0] = '\0';
}

bool battery_detect_read(battery_info_t * info)
{
    if(!info) return false;

    memset(info, 0, sizeof(battery_info_t));
    info->capacity = 100;
    info->is_charging = false;
    info->is_present = false;
    snprintf(info->status_str, sizeof(info->status_str), "Unknown");

    char bat_dir[256];
    if(!find_battery_path(bat_dir, sizeof(bat_dir))) {
        /* No battery found (e.g. desktop environment without battery) */
        info->is_present = false;
        info->is_charging = true;
        snprintf(info->status_str, sizeof(info->status_str), "AC Powered");
        return false;
    }

    info->is_present = true;

    /* Read capacity */
    char cap_file[280];
    snprintf(cap_file, sizeof(cap_file), "%s/capacity", bat_dir);
    FILE * fcap = fopen(cap_file, "r");
    if(fcap) {
        int cap = 0;
        if(fscanf(fcap, "%d", &cap) == 1) {
            if(cap < 0) cap = 0;
            if(cap > 100) cap = 100;
            info->capacity = cap;
        }
        fclose(fcap);
    } else {
        /* Fallback: try charge_now / charge_full */
        char now_file[280], full_file[280];
        snprintf(now_file, sizeof(now_file), "%s/charge_now", bat_dir);
        snprintf(full_file, sizeof(full_file), "%s/charge_full", bat_dir);

        FILE * fnow = fopen(now_file, "r");
        FILE * ffull = fopen(full_file, "r");

        if(!fnow || !ffull) {
            snprintf(now_file, sizeof(now_file), "%s/energy_now", bat_dir);
            snprintf(full_file, sizeof(full_file), "%s/energy_full", bat_dir);
            if(fnow) fclose(fnow);
            if(ffull) fclose(ffull);
            fnow = fopen(now_file, "r");
            ffull = fopen(full_file, "r");
        }

        if(fnow && ffull) {
            long long val_now = 0, val_full = 0;
            if(fscanf(fnow, "%lld", &val_now) == 1 && fscanf(ffull, "%lld", &val_full) == 1 && val_full > 0) {
                int cap = (int)((val_now * 100) / val_full);
                if(cap < 0) cap = 0;
                if(cap > 100) cap = 100;
                info->capacity = cap;
            }
        }
        if(fnow) fclose(fnow);
        if(ffull) fclose(ffull);
    }

    /* Read battery status string */
    char status_file[280];
    snprintf(status_file, sizeof(status_file), "%s/status", bat_dir);
    FILE * fstatus = fopen(status_file, "r");
    if(fstatus) {
        if(fgets(info->status_str, sizeof(info->status_str), fstatus)) {
            trim_string(info->status_str);
        }
        fclose(fstatus);
    }

    /* Determine charging state */
    if(strcasecmp(info->status_str, "Charging") == 0) {
        info->is_charging = true;
    } else if(strcasecmp(info->status_str, "Full") == 0) {
        info->is_charging = true;
    } else {
        /* Check AC online status */
        char ac_dir[256];
        if(find_ac_path(ac_dir, sizeof(ac_dir))) {
            char ac_online_file[280];
            snprintf(ac_online_file, sizeof(ac_online_file), "%s/online", ac_dir);
            FILE * fac = fopen(ac_online_file, "r");
            if(fac) {
                int ac_online = 0;
                if(fscanf(fac, "%d", &ac_online) == 1 && ac_online == 1) {
                    /* AC is connected */
                    if(strcasecmp(info->status_str, "Discharging") != 0) {
                        info->is_charging = true;
                    }
                }
                fclose(fac);
            }
        }
    }

    return true;
}

void battery_update_label(lv_obj_t * label)
{
    if(!label) return;

    battery_info_t info;
    battery_detect_read(&info);

    char buf[32];
    lv_color_t color;

    if(!info.is_present) {
        snprintf(buf, sizeof(buf), "%s AC", LV_SYMBOL_CHARGE);
        color = lv_color_hex(0x00E676); /* Green */
    } else if(info.is_charging) {
        snprintf(buf, sizeof(buf), "%s %d%%", LV_SYMBOL_CHARGE, info.capacity);
        color = lv_color_hex(0x00C2FF); /* Cyan / Bright accent when charging */
    } else {
        const char * icon;
        if(info.capacity >= 90) {
            icon = LV_SYMBOL_BATTERY_FULL;
        } else if(info.capacity >= 65) {
            icon = LV_SYMBOL_BATTERY_3;
        } else if(info.capacity >= 35) {
            icon = LV_SYMBOL_BATTERY_2;
        } else if(info.capacity >= 15) {
            icon = LV_SYMBOL_BATTERY_1;
        } else {
            icon = LV_SYMBOL_BATTERY_EMPTY;
        }

        snprintf(buf, sizeof(buf), "%s %d%%", icon, info.capacity);

        if(info.capacity > 20) {
            color = lv_color_hex(0x00E676); /* Normal Green */
        } else if(info.capacity > 10) {
            color = lv_color_hex(0xFFB300); /* Warning Amber / Yellow */
        } else {
            color = lv_color_hex(0xFF3B30); /* Critical Red */
        }
    }

    /* Update text if changed */
    const char * cur_text = lv_label_get_text(label);
    if(!cur_text || strcmp(cur_text, buf) != 0) {
        lv_label_set_text(label, buf);
    }

    lv_obj_set_style_text_color(label, color, 0);
}
