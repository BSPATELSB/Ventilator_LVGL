#include "usb_detect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

void usb_detect_init(void)
{
    /* Ready for initialization if needed */
}

/* Helper to check if a block device in /sys/class/block/ is a removable USB device */
static bool is_usb_removable_device(const char * dev_name)
{
    char path[256];
    snprintf(path, sizeof(path), "/sys/class/block/%s/removable", dev_name);

    FILE * f = fopen(path, "r");
    if(!f) return false;

    char buf[16] = {0};
    if(fgets(buf, sizeof(buf), f)) {
        fclose(f);
        if(buf[0] == '1') {
            /* Verify if device link points to usb subsystem or storage */
            char sys_dev_path[256];
            snprintf(sys_dev_path, sizeof(sys_dev_path), "/sys/class/block/%s/device", dev_name);
            char target_path[512] = {0};
            ssize_t len = readlink(sys_dev_path, target_path, sizeof(target_path)-1);
            if(len > 0) {
                target_path[len] = '\0';
                /* Check if "usb" or "usb-storage" is part of the bus path */
                if(strstr(target_path, "usb") != NULL || strstr(target_path, "target") != NULL) {
                    return true;
                }
            }
            return true; /* Removable disk */
        }
    } else {
        fclose(f);
    }

    return false;
}

/* Check /proc/mounts for mount path of device */
static bool get_mount_point(const char * dev_name, char * mount_out, size_t max_len)
{
    FILE * f = fopen("/proc/mounts", "r");
    if(!f) return false;

    char line[512];
    char device[128], mount[256];

    while(fgets(line, sizeof(line), f)) {
        if(sscanf(line, "%127s %255s", device, mount) == 2) {
            if(strstr(device, dev_name) != NULL) {
                snprintf(mount_out, max_len, "%s", mount);
                fclose(f);
                return true;
            }
        }
    }

    fclose(f);
    return false;
}

bool usb_detect_check(usb_drive_info_t * info)
{
    if(!info) return false;

    memset(info, 0, sizeof(usb_drive_info_t));

    DIR * dir = opendir("/sys/class/block");
    if(!dir) return false;

    struct dirent * entry;
    while((entry = readdir(dir)) != NULL) {
        if(entry->d_name[0] == '.') continue;

        /* Filter block device names like sdb, sdc, sdb1, sdc1, etc. */
        if(strncmp(entry->d_name, "sd", 2) == 0 || strncmp(entry->d_name, "nvme", 4) == 0 || strncmp(entry->d_name, "mmcblk", 6) == 0) {
            if(is_usb_removable_device(entry->d_name)) {
                snprintf(info->dev_name, sizeof(info->dev_name), "/dev/%s", entry->d_name);
                info->is_connected = true;

                if(!get_mount_point(entry->d_name, info->mount_point, sizeof(info->mount_point))) {
                    snprintf(info->mount_point, sizeof(info->mount_point), "Unmounted (%s)", info->dev_name);
                }

                closedir(dir);
                return true;
            }
        }
    }

    closedir(dir);
    info->is_connected = false;
    return false;
}
