#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "stm32h7xx_hal.h"
#include "GUI.h"
#include "spectrogram.h"
#include "adc.h"
#include "options.h"

// Global pause flag, set by pause_recording function
volatile int pause_flag = 0;

void pause_recording(void) {
    pause_flag = 1;
}

/**
 * @brief Allocates and initializes a spectrogram structure for interleaved ADC sampling.
 * @return Pointer to initialized spectrogram_t structure
 */
spectrogram_t *spectrogram_default(void) {
    spectrogram_t *s = malloc(sizeof(spectrogram_t));
    if (!s) return NULL;

    s->pos_x = 30;
    s->pos_y = 5;
    s->graticules_nx = 10;
    s->graticules_ny = 8;
    s->graticule_start_y = 0;
    s->graticule_step_y = -10;
    s->size_x = 801;
    s->size_y = 225;
    s->history_readings = 10;
    s->history_position = 0;
    s->npoints = s->size_x;

    spectrogram_init_data(s);

    return s;
}

/* Colour mapping (retained for potential future use) */
double interpolate(double val, double y0, double x0, double y1, double x1) { ... }
double base(double val) { ... }
double red(double gray) { ... }
double green(double gray) { ... }
double blue(double gray) { ... }

#define LUT_ELEMENTS 64
uint32_t lut[LUT_ELEMENTS] = {0};

void lut_init(void) { ... }
uint32_t lut_lookup(uint16_t v) { ... }

/**
 * @brief Initializes data buffers for interleaved ADC sampling.
 * @param s: Spectrogram structure
 */
void spectrogram_init_data(spectrogram_t *s) {
    lut_init();

    s->data_x = malloc(s->npoints * sizeof(uint16_t));
    s->data_y = malloc(s->npoints * sizeof(uint16_t));
    s->data_z = malloc(s->npoints * sizeof(uint8_t));
    s->data_normal = malloc(s->npoints * sizeof(int16_t));

    if (!s->data_x || !s->data_y || !s->data_z || !s->data_normal) {
        free(s->data_x);
        free(s->data_y);
        free(s->data_z);
        free(s->data_normal);
        return;
    }

    memset(s->data_x, 0, s->npoints * sizeof(uint16_t));
    memset(s->data_y, 0, s->npoints * sizeof(uint16_t));
    memset(s->data_z, 0, s->npoints * sizeof(uint8_t));
    memset(s->data_normal, 0, s->npoints * sizeof(int16_t));

    s->data_history = malloc(s->history_readings * sizeof(uint16_t *));
    if (!s->data_history) return;

    for (int i = 0; i < s->history_readings; ++i) {
        s->data_history[i] = malloc(s->npoints * sizeof(uint16_t));
        if (!s->data_history[i]) return;
        memset(s->data_history[i], 0, s->npoints * sizeof(uint16_t));
    }
}

/**
 * @brief Saves interleaved ADC data to multiple CSV files with up to 500 points each.
 * @param s: Spectrogram structure
 * @param base_filename: Base filename (e.g., "spectrogram_data_")
 * @return 0 on success, -1 on failure
 */
int spectrogram_save_csv(spectrogram_t *s, const char *base_filename) {
    if (!s || !base_filename) return -1;

    const double sample_period_ms = 0.000138889; // 1 / 7.2 MSPS
    int file_index = 0;
    int point_count = 0;
    char filename[64];
    FILE *fp = NULL;

    for (int i = 0; i < s->npoints && !pause_flag; ++i) {
        if (point_count == 0) {
            // Open new file
            snprintf(filename, sizeof(filename), "%s%d.csv", base_filename, file_index);
            fp = fopen(filename, "w");
            if (!fp) return -1;
            fprintf(fp, "Time (ms),V_x (V),V_y (V),V_z (V)\n");
        }

        double time_ms = i * sample_period_ms;
        float vx = s->data_x[i] * 3.3f / 65535.0f;
        float vy = s->data_y[i] * 3.3f / 65535.0f;
        float vz = s->data_z[i] * 3.3f;

        fprintf(fp, "%.8f,%.8f,%.8f,%.8f\n", time_ms, vx, vy, vz);

        point_count++;
        if (point_count >= 500 || i == s->npoints - 1) {
            fclose(fp);
            point_count = 0;
            file_index++;
        }
    }

    return 0;
}

/**
 * @brief Generates fake interleaved ADC data for testing.
 * @param s: Spectrogram structure
 */
void spectrogram_fake_data(spectrogram_t *s) {
    for (int i = 0; i < s->npoints; ++i) {
        s->data_x[i] = (uint16_t)((i % 801) * 65535 / 801);
        float t = i / 7200000.0f;
        s->data_y[i] = (uint16_t)((sinf(2 * M_PI * 300000 * t) * 0.5 + 0.5) * 65535);
        s->data_z[i] = (i / 400) % 2 ? 1 : 0;
    }
}
