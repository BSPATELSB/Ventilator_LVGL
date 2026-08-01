#include "patient_data.h"
#include <stdio.h>
#include <string.h>

static patient_info_t g_patient_data;
static bool g_initialized = false;

void patient_data_init(void)
{
    if (g_initialized) return;

    snprintf(g_patient_data.name, sizeof(g_patient_data.name), "John Doe");
    snprintf(g_patient_data.id, sizeof(g_patient_data.id), "12345678");
    g_patient_data.age = 45;
    g_patient_data.weight_kg = 70.0f;
    g_patient_data.height_cm = 175.0f;
    snprintf(g_patient_data.gender, sizeof(g_patient_data.gender), "Male");
    snprintf(g_patient_data.diagnosis, sizeof(g_patient_data.diagnosis), "Acute Respiratory Failure");
    snprintf(g_patient_data.hospital_id, sizeof(g_patient_data.hospital_id), "HOSP-2024-05876");
    snprintf(g_patient_data.attending_physician, sizeof(g_patient_data.attending_physician), "Dr. Sarah Johnson");
    snprintf(g_patient_data.admission_date, sizeof(g_patient_data.admission_date), "19 May 2024\n08:15 AM");
    snprintf(g_patient_data.notes, sizeof(g_patient_data.notes),
             "• Patient is stable on current ventilator settings.\n"
             "• Monitor for signs of improvement in oxygenation.\n"
             "• Plan: Weaning assessment in next 24-48 hours.\n"
             "• Family updated.");

    g_initialized = true;
}

patient_info_t * patient_data_get(void)
{
    if (!g_initialized) {
        patient_data_init();
    }
    return &g_patient_data;
}

void patient_data_update(const patient_info_t * new_info)
{
    if (!new_info) return;
    patient_data_init();

    if (new_info->name[0] != '\0') snprintf(g_patient_data.name, sizeof(g_patient_data.name), "%s", new_info->name);
    if (new_info->id[0] != '\0') snprintf(g_patient_data.id, sizeof(g_patient_data.id), "%s", new_info->id);
    if (new_info->age > 0) g_patient_data.age = new_info->age;
    if (new_info->weight_kg > 0.0f) g_patient_data.weight_kg = new_info->weight_kg;
    if (new_info->height_cm > 0.0f) g_patient_data.height_cm = new_info->height_cm;
    if (new_info->gender[0] != '\0') snprintf(g_patient_data.gender, sizeof(g_patient_data.gender), "%s", new_info->gender);
    if (new_info->diagnosis[0] != '\0') snprintf(g_patient_data.diagnosis, sizeof(g_patient_data.diagnosis), "%s", new_info->diagnosis);
    if (new_info->hospital_id[0] != '\0') snprintf(g_patient_data.hospital_id, sizeof(g_patient_data.hospital_id), "%s", new_info->hospital_id);
    if (new_info->attending_physician[0] != '\0') snprintf(g_patient_data.attending_physician, sizeof(g_patient_data.attending_physician), "%s", new_info->attending_physician);
    if (new_info->notes[0] != '\0') snprintf(g_patient_data.notes, sizeof(g_patient_data.notes), "%s", new_info->notes);
    if (new_info->admission_date[0] != '\0') snprintf(g_patient_data.admission_date, sizeof(g_patient_data.admission_date), "%s", new_info->admission_date);
}

float patient_data_calculate_bmi(float weight_kg, float height_cm)
{
    if (height_cm <= 0.0f) return 0.0f;
    float height_m = height_cm / 100.0f;
    return weight_kg / (height_m * height_m);
}

const char * patient_data_get_bmi_status(float bmi)
{
    if (bmi < 18.5f) return "Underweight";
    if (bmi < 25.0f) return "Normal";
    if (bmi < 30.0f) return "Overweight";
    return "Obese";
}
