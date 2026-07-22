#ifndef USB_DETECT_H
#define USB_DETECT_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool is_connected;
    char dev_name[64];       /* e.g., "sdb1" or "/dev/sdb1" */
    char mount_point[256];   /* e.g., "/media/user/PENDRIVE" */
} usb_drive_info_t;

/**
 * @brief Initialize USB detector module
 */
void usb_detect_init(void);

/**
 * @brief Scan system for attached USB mass storage pendrives.
 * @param info Output structure containing connection state and drive info
 * @return true if at least one USB pendrive is detected, false otherwise
 */
bool usb_detect_check(usb_drive_info_t * info);

#ifdef __cplusplus
}
#endif

#endif /* USB_DETECT_H */
