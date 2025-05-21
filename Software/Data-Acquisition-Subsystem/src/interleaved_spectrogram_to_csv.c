#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h> // Added for FILE operations
#include "stm32h7xx_hal.h"
#include "GUI.h"
#include "spectrogram.h"
#include "adc.h"
#include "options.h"

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
    s->size_x = 801; // 801 points (UR01)
    s->size_y = 225;
    s->history_readings = 10;
    s->history_position = 0;
    s->npoints = s->size_x;

    spectrogram_init_data(s);

    return s;
}

/* Colour mapping (retained for potential future use) */
double interpolate(double val, double y0, double x0, double y1, double x1) {
    return (val - x0) * (y1 - y0) / (x1 - x0) + y0;
}

double base(double val) {
    if (val <= -0.75) return 0;
    else if (val <= -0.25) return interpolate(val, 0.0, -0.75, 1.0, -0.25);
    else if (val <= 0.25) return 1.0;
    else if (val <= 0.75) return interpolate(val, 1.0, 0.25, 0.0, 0.75);
    else return 0.0;
}

double red(double gray) { return base(gray - 0.5); }
double green(double gray) { return base(gray); }
double blue(double gray) { return base(gray + 0.5); }

#define LUT_ELEMENTS 64
uint32_t lut[LUT_ELEMENTS] = {0};

void lut_init(void) {
    for (int i = 0; i < LUT_ELEMENTS; ++i) {
        float gray = (float)i / (float)(LUT_ELEMENTS - 1);
        uint32_t colour =
            ((uint8_t)(red(gray) * 255)) |
            ((uint8_t)(green(gray) * 255) << 8) |
            ((uint8_t)(blue(gray) * 255) << 16);
        lut[i] = colour;
    }
}

uint32_t lut_lookup(uint16_t v) {
    return lut[v * (LUT_ELEMENTS - 1) / 65535];
}

/**
 * @brief Initializes data buffers for interleaved ADC sampling.
 * @param s: Spectrogram structure
 */
void spectrogram_init_data(spectrogram_t *s) {
    lut_init();

    s->data_x = malloc(s->npoints * sizeof(uint16_t)); // V_x (horizontal)
    s->data_y = malloc(s->npoints * sizeof(uint16_t)); // V_y (vertical)
    s->data_z = malloc(s->npoints * sizeof(uint8_t));  // V_z (pen-lift)
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
 * @brief Saves interleaved ADC data to a CSV file in PicoScope 7 format.
 * @param s: Spectrogram structure
 * @param filename: Output CSV file path
 * @return 0 on success, -1 on failure
 */
int spectrogram_save_csv(spectrogram_t *s, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) return -1;

    // Write CSV header
    fprintf(fp, "Time (ms),V_x (V),V_y (V),V_z (V)\n");

    // Sampling period: 1 / 7.2 MSPS = 0.138889 µs = 0.000138889 ms
    const double sample_period_ms = 0.000138889;

    // Write data points
    for (int i = 0; i < s->npoints; ++i) {
        // Timestamp: t = i * sample_period_ms
        double time_ms = i * sample_period_ms;

        // Convert ADC values to voltages (0-65535 → 0-3.3 V)
        float vx = s->data_x[i] * 3.3f / 65535.0f;
        float vy = s->data_y[i] * 3.3f / 65535.0f;
        float vz = s->data_z[i] * 3.3f; // Binary: 0 or 3.3 V

        // Write to CSV with high precision
        fprintf(fp, "%.8f,%.8f,%.8f,%.8f\n", time_ms, vx, vy, vz);
    }

    fclose(fp);
    return 0;
}

/**
 * @brief Generates fake interleaved ADC data for testing.
 * @param s: Spectrogram structure
 */
void spectrogram_fake_data(spectrogram_t *s) {
    for (int i = 0; i < s->npoints; ++i) {
        // V_x: Sawtooth (500 Hz, 0-3.3 V)
        s->data_x[i] = (uint16_t)((i % 801) * 65535 / 801);
        // V_y: Sine wave (300 kHz, 0-3.3 V)
        float t = i / 7200000.0f; // Time at 7.2 MSPS
        s->data_y[i] = (uint16_t)((sinf(2 * M_PI * 300000 * t) * 0.5 + 0.5) * 65535);
        // V_z: Binary pen-lift (toggles every 400 points)
        s->data_z[i] = (i / 400) % 2 ? 1 : 0;
    }
}
