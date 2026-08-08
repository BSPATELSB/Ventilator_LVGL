#ifndef VENTILATOR_DEVICE_INFO_SCREEN_H
#define VENTILATOR_DEVICE_INFO_SCREEN_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Structure holding device information read from version_properties.json
 */
typedef struct {
    char firmware_version[64];
    char application_version[64];
    char application_name[128];
    char developer_name[128];
    char technology[256];
    char build_date[64];
    char hardware_revision[64];
    char serial_number[64];
    char features[16][160];
    int feature_count;
    bool json_read_success;
} ventilator_device_info_t;

/**
 * @brief Read device version properties from version_properties.json file.
 * @param info Pointer to ventilator_device_info_t structure to populate.
 * @return true if json file was read successfully, false otherwise.
 */
bool read_version_properties_json(ventilator_device_info_t * info);

/**
 * @brief Initialize and display the Device Information screen (1280x800).
 */
void create_ventilator_device_info_screen(void);

#ifdef __cplusplus
}
#endif

#endif /* VENTILATOR_DEVICE_INFO_SCREEN_H */
