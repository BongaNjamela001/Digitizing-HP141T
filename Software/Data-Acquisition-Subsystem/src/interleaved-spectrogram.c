#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "stm32h7xx_hal.h" 
#include "GUI.h"
#include "spectrogram.h"
#include "adc.h"
#include "options.h"

/**
 * @brief Allocates and initializes a spectrogram structure for double-interleaved ADC sampling.
 * @return Pointer to initialized spectrogram_t structure
 */
spectrogram_t *spectrogram_default(void) {
    spectrogram_t *s = malloc(sizeof(spectrogram_t));
    if (!s) return NULL; // Modified: Null check for allocation

    s->pos_x = 30;          // X position on display
    s->pos_y = 5;           // Y position on display
    s->graticules_nx = 10;  // 10 horizontal graticules
    s->graticules_ny = 8;   // 8 vertical graticules
    s->graticule_start_y = 0; // Y-axis start (dB)
    s->graticule_step_y = -10; // 10 dB per graticule
    s->size_x = 801;        // Modified: 801 points (UR01)
    s->size_y = 225;        // Display height in pixels
    s->history_readings = 10; // Modified: Reduced to 10 for memory efficiency
    s->history_position = 0;
    s->npoints = s->size_x; // 801 points for x-axis

    spectrogram_init_data(s);

    return s;
}

/* Colour mapping adapted from MATLAB jet colormap */
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

double red(double gray) {
    return base(gray - 0.5);
}

double green(double gray) {
    return base(gray);
}

double blue(double gray) {
    return base(gray + 0.5);
}

#define LUT_ELEMENTS 64
uint32_t lut[LUT_ELEMENTS] = {0};

/**
 * @brief Initializes the color lookup table for 16-bit ADC data.
 */
void lut_init(void) {
    for (int i = 0; i < LUT_ELEMENTS; ++i) {
        float gray = (float)i / (float)(LUT_ELEMENTS - 1); // Modified: Normalize to 0-1
        uint32_t colour =
            ((uint8_t)(red(gray) * 255)) |
            ((uint8_t)(green(gray) * 255) << 8) |
            ((uint8_t)(blue(gray) * 255) << 16);
        lut[i] = colour;
    }
}

/**
 * @brief Looks up color for a 16-bit ADC value.
 * @param v: ADC value (0-65535)
 * @return Color code
 */
uint32_t lut_lookup(uint16_t v) {
    return lut[v * (LUT_ELEMENTS - 1) / 65535]; // Modified: Scale 16-bit to LUT
}

/**
 * @brief Initializes data buffers for interleaved ADC sampling.
 * @param s: Spectrogram structure
 */
void spectrogram_init_data(spectrogram_t *s) {
    lut_init();

    s->data_x = malloc(s->npoints * sizeof(uint16_t)); // Modified: V_x (horizontal)
    s->data_y = malloc(s->npoints * sizeof(uint16_t)); // Modified: V_y (vertical)
    s->data_z = malloc(s->npoints * sizeof(uint8_t));  // Modified: V_z (pen-lift)
    s->data_normal = malloc(s->npoints * sizeof(int16_t)); // Normalization data

    if (!s->data_x || !s->data_y || !s->data_z || !s->data_normal) {
        // Handle allocation failure
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
        s->data_history[i] = malloc(s->npoints * sizeof(uint16_t)); // Only V_y
        if (!s->data_history[i]) return;
        memset(s->data_history[i], 0, s->npoints * sizeof(uint16_t));
    }
}

/**
 * @brief Draws the spectrogram with interleaved ADC data.
 * @param s: Spectrogram structure
 */
void spectrogram_draw(spectrogram_t *s) {
    if (option_get_selection(OPTION_ID_VIEW_WATERFALL) == OPTION_ID_VIEW_WATERFALL_ON) {
        // Store current V_y data in history for waterfall
        memcpy(s->data_history[s->history_position], s->data_y, s->npoints * sizeof(uint16_t));
        s->history_position = (s->history_position + 1) % s->history_readings;

        // Draw waterfall
        for (int i = 0; i < s->history_readings; ++i) {
            for (int j = 0; j < s->npoints; j += 3) { // Subsample for performance
                if (s->data_z[j] == 0) { // Plot only if pen is down
                    GUI_SetColor(lut_lookup(s->data_history[i][j]));
                    int i_up = (i - s->history_position + s->history_readings) % s->history_readings;
                    GUI_FillRect(
                        s->pos_x + j,
                        s->pos_y + i_up * (s->size_y / s->history_readings),
                        s->pos_x + j + 3,
                        s->pos_y + (i_up + 1) * (s->size_y / s->history_readings)
                    );
                }
            }
        }
    }

    // Draw graticules
    GUI_SetColor(GUI_DARKGRAY);
    for (int i = 0; i < s->graticules_nx; ++i) {
        GUI_DrawVLine(s->pos_x + i * s->size_x / s->graticules_nx, s->pos_y, s->pos_y + s->size_y);
        for (int k = 0; k < 5; ++k) {
            GUI_DrawVLine(
                s->pos_x + i * s->size_x / s->graticules_nx + k * 0.2 * s->size_x / s->graticules_nx,
                s->pos_y + s->size_y / 2 - 3,
                s->pos_y + s->size_y / 2 + 3
            );
        }
    }

    for (int i = 0; i < s->graticules_ny; ++i) {
        GUI_DrawHLine(s->pos_y + i * s->size_y / s->graticules_ny, s->pos_x, s->pos_x + s->size_x);
        for (int k = 0; k < 5; ++k) {
            GUI_DrawHLine(
                s->pos_y + i * s->size_y / s->graticules_ny + k * 0.2 * s->size_y / s->graticules_ny,
                s->pos_x + s->size_x / 2 - 3,
                s->pos_x + s->size_x / 2 + 3
            );
        }
    }

    // Draw frame
    GUI_SetColor(GUI_WHITE);
    GUI_DrawRoundedFrame(s->pos_x, s->pos_y, s->pos_x + s->size_x, s->pos_y + s->size_y, 3, 2);

    // Draw graph if waterfall is off
    if (option_get_selection(OPTION_ID_VIEW_WATERFALL) == OPTION_ID_VIEW_WATERFALL_OFF) {
        GUI_SetColor(GUI_YELLOW);
        for (int i = 0; i < s->npoints - 1; ++i) {
            if (s->data_z[i] == 0) { // Plot only if pen is down
                int x0 = s->pos_x + i;
                int y0 = s->pos_y + (s->size_y - (s->data_y[i] * s->size_y / 65535));
                int x1 = s->pos_x + i + 1;
                int y1 = s->pos_y + (s->size_y - (s->data_y[i + 1] * s->size_y / 65535));
                GUI_DrawLine(x0, y0, x1, y1);
            }
        }
    }

    // Draw graticule labels
    GUI_SetColor(GUI_WHITE);
    GUI_SetFont(&GUI_Font8_ASCII);
    char buf[16];
    for (int i = 0; i <= s->graticules_ny; ++i) {
        snprintf(buf, sizeof(buf), "%d", s->graticule_start_y + i * s->graticule_step_y);
        GUI_SetTextAlign(GUI_TA_RIGHT | GUI_TA_VCENTER);
        GUI_DispStringAt(buf, s->pos_x - 3, s->pos_y + i * s->size_y / s->graticules_ny);
    }
}

/**
 * @brief Generates fake interleaved ADC data for testing.
 * @param s: Spectrogram structure
 */
void spectrogram_fake_data(spectrogram_t *s) {
    for (int i = 0; i < s->npoints; ++i) {
        // V_x: Sawtooth (0 to 3.3 V, 500 Hz)
        s->data_x[i] = (uint16_t)((i % 801) * 65535 / 801);
        // V_y: Sine wave (300 kHz, 0 to 3.3 V)
        float t = i / 7200000.0f; // Time at 7.2 MSPS
        s->data_y[i] = (uint16_t)((sinf(2 * M_PI * 300000 * t) * 0.5 + 0.5) * 65535);
        // V_z: Binary pen-lift (0 or 1, toggles every 400 points)
        s->data_z[i] = (i / 400) % 2 ? 1 : 0;
    }
}
