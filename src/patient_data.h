#ifndef PATIENT_DATA_H
#define PATIENT_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

typedef struct {
    char name[64];
    char id[32];
    int age;
    float weight_kg;
    float height_cm;
    char gender[16];
    char diagnosis[128];
    char hospital_id[32];
    char attending_physician[64];
    char notes[512];
    char admission_date[64];
} patient_info_t;

/**
 * @brief Initialize global patient data with default baseline values.
 */
void patient_data_init(void);

/**
 * @brief Get pointer to current active patient information.
 */
patient_info_t * patient_data_get(void);

/**
 * @brief Update patient data with new info.
 */
void patient_data_update(const patient_info_t * new_info);

/**
 * @brief Calculate Body Mass Index (BMI) = weight (kg) / (height (m))^2
 */
float patient_data_calculate_bmi(float weight_kg, float height_cm);

/**
 * @brief Get qualitative text status for BMI value (e.g. Underweight, Normal, Overweight, Obese)
 */
const char * patient_data_get_bmi_status(float bmi);

#ifdef __cplusplus
}
#endif

#endif /* PATIENT_DATA_H */
