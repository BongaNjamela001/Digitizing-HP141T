#include "adc.h"
#include "stm32h7xx_hal.h" // Modified: Explicitly include STM32H7 HAL for STM32H723ZG

/* Modified: Updated comment for STM32H723ZG context
 * This code configures ADC3 and DMA2 on the STM32H723ZG to continuously convert
 * three analog signals (horizontal, vertical, pen-lift) from the HP141T spectrum
 * analyzer, storing data in a spectrogram for display. The ADC clock is 27 MHz
 * (APB2/4, system clock 216 MHz), with 12-bit resolution and a sampling rate of
 * ~675 kSPS per channel, suitable for the 300 kHz vertical output.
 */

#define ADCx                            ADC3
#define ADCx_CLK_ENABLE()               __HAL_RCC_ADC3_CLK_ENABLE()
#define DMAx_CLK_ENABLE()               __HAL_RCC_DMA2_CLK_ENABLE()
#define ADCx_CHANNEL_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOF_CLK_ENABLE()

#define ADCx_FORCE_RESET()              __HAL_RCC_ADC_FORCE_RESET()
#define ADCx_RELEASE_RESET()            __HAL_RCC_ADC_RELEASE_RESET()

/* Definition for ADCx Channel Pins */
#define ADCx_CHANNEL_PIN                GPIO_PIN_10 // Horizontal (V_x), PF10
#define ADCx_CHANNEL_PIN2               GPIO_PIN_9  // Vertical (V_y), PF9
#define ADCx_CHANNEL_PIN3               GPIO_PIN_8  // Modified: Added for pen-lift (V_z), PF8
#define ADCx_CHANNEL_GPIO_PORT          GPIOF

/* Definition for ADCx's Channels */
#define ADCx_CHANNEL                    ADC_CHANNEL_8  // Horizontal
#define ADCx_CHANNEL2                   ADC_CHANNEL_7  // Vertical
#define ADCx_CHANNEL3                   ADC_CHANNEL_6  // Modified: Pen-lift

/* Definition for ADCx's DMA */
#define ADCx_DMA_CHANNEL                DMA_CHANNEL_2
#define ADCx_DMA_STREAM                 DMA2_Stream0

/* Definition for ADCx's NVIC */
#define ADCx_DMA_IRQn                   DMA2_Stream0_IRQn
#define ADCx_DMA_IRQHandler             DMA2_Stream0_IRQHandler

ADC_HandleTypeDef    AdcHandle;

#define ADC_BUF_SIZE 3072 // Modified: Increased to 3072 (1024 samples/channel for 3 channels)

volatile uint32_t adc_buffer[ADC_BUF_SIZE] = {0};

int adc_last_conversion_complete = 0;

/* Modified: Added error flag for better error handling */
volatile int adc_error_flag = 0;

/**
 * @brief  This function handles DMA interrupt request.
 * @param  None
 * @retval None
 */
void ADCx_DMA_IRQHandler(void) {
  HAL_DMA_IRQHandler(AdcHandle.DMA_Handle);
}

/**
 * @brief  Error handler for ADC initialization or configuration failures.
 * @param  None
 * @retval None
 */
void adc_error_handler(void) {
    adc_error_flag = 1; // Modified: Set error flag instead of infinite loop
    // Optionally add debug output (e.g., UART) in production
    while(1) {} // Retain halt for critical errors
}

spectrogram_t *adc_spectrogram = 0;

/**
 * @brief  Sets the spectrogram structure for data storage.
 * @param  s: Pointer to spectrogram structure
 * @retval None
 */
void adc_set_spectrogram(spectrogram_t *s) {
    adc_spectrogram = s;
}

/**
 * @brief  Initializes ADC3 and DMA for continuous conversion on three channels.
 * @param  None
 * @retval 0 on success, -1 on failure
 */
int adc_init(void) {
    ADC_ChannelConfTypeDef sConfig;

    AdcHandle.Instance = ADCx;
    AdcHandle.Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV4; // 27 MHz ADC clock
    AdcHandle.Init.Resolution = ADC_RESOLUTION_12B;
    AdcHandle.Init.ScanConvMode = ENABLE; // Scan multiple channels
    AdcHandle.Init.ContinuousConvMode = ENABLE;
    AdcHandle.Init.DiscontinuousConvMode = DISABLE;
    AdcHandle.Init.NbrOfDiscConversion = 0;
    AdcHandle.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    AdcHandle.Init.ExternalTrigConv = ADC_SOFTWARE_START; // Modified: Use software trigger
    AdcHandle.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    AdcHandle.Init.NbrOfConversion = 3; // Modified: Three channels (V_x, V_y, V_z)
    AdcHandle.Init.DMAContinuousRequests = ENABLE;
    AdcHandle.Init.EOCSelection = DISABLE;

    if (HAL_ADC_Init(&AdcHandle) != HAL_OK) {
        adc_error_handler();
        return -1;
    }

    /* Configure ADC channel for horizontal (V_x) */
    sConfig.Channel = ADCx_CHANNEL;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_28CYCLES; // ~1.48 µs conversion time
    sConfig.Offset = 0;
    if (HAL_ADC_ConfigChannel(&AdcHandle, &sConfig) != HAL_OK) {
        adc_error_handler();
        return -1;
    }

    /* Configure ADC channel for vertical (V_y) */
    sConfig.Channel = ADCx_CHANNEL2;
    sConfig.Rank = 2;
    sConfig.SamplingTime = ADC_SAMPLETIME_28CYCLES;
    sConfig.Offset = 0;
    if (HAL_ADC_ConfigChannel(&AdcHandle, &sConfig) != HAL_OK) {
        adc_error_handler();
        return -1;
    }

    /* Modified: Configure ADC channel for pen-lift (V_z) */
    sConfig.Channel = ADCx_CHANNEL3;
    sConfig.Rank = 3;
    sConfig.SamplingTime = ADC_SAMPLETIME_28CYCLES;
    sConfig.Offset = 0;
    if (HAL_ADC_ConfigChannel(&AdcHandle, &sConfig) != HAL_OK) {
        adc_error_handler();
        return -1;
    }

    /* Start ADC with DMA */
    if (HAL_ADC_Start_DMA(&AdcHandle, (uint32_t*)adc_buffer, ADC_BUF_SIZE) != HAL_OK) {
        adc_error_handler();
        return -1;
    }

    return 0;
}

volatile int prev_x = 0;

extern int pending_normalization;

/**
 * @brief  Callback for half buffer DMA transfer complete.
 * @param  AdcHandle: ADC handle
 * @retval None
 */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* AdcHandle) {
    for (int j = 0; j < ADC_BUF_SIZE/6; ++j) { // Modified: Adjust for 3 channels
        uint32_t x_val = adc_buffer[j*3];     // Horizontal (V_x)
        uint32_t y_val = adc_buffer[j*3+1];   // Vertical (V_y)
        uint32_t z_val = adc_buffer[j*3+2];   // Modified: Pen-lift (V_z)
        x_val = (x_val * adc_spectrogram->npoints) / 4096;
        y_val = adc_spectrogram->size_y - (adc_spectrogram->size_y * y_val) / 4096;
        z_val = z_val > 2048 ? 1 : 0; // Modified: Threshold pen-lift at mid-scale (~1.65 V)
        if (x_val < 5) prev_x = 0;
        if (x_val > prev_x && z_val == 0) { // Modified: Only plot if pen is down
            if (pending_normalization) {
                adc_spectrogram->data_normal[x_val] = adc_spectrogram->size_y/2 - y_val;
            }
            adc_spectrogram->data[x_val] = y_val + adc_spectrogram->data_normal[x_val];
            prev_x = x_val;
        }
    }
}

/**
 * @brief  Callback for full buffer DMA transfer complete.
 * @param  AdcHandle: ADC handle
 * @retval None
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* AdcHandle) {
    for (int j = ADC_BUF_SIZE/6; j < ADC_BUF_SIZE/3; ++j) { // Modified: Adjust for 3 channels
        uint32_t x_val = adc_buffer[j*3];     // Horizontal (V_x)
        uint32_t y_val = adc_buffer[j*3+1];   // Vertical (V_y)
        uint32_t z_val = adc_buffer[j*3+2];   // Modified: Pen-lift (V_z)
        x_val = (x_val * adc_spectrogram->npoints) / 4096;
        y_val = adc_spectrogram->size_y - (adc_spectrogram->size_y * y_val) / 4096;
        z_val = z_val > 2048 ? 1 : 0; // Modified: Threshold pen-lift at mid-scale (~1.65 V)
        if (x_val < 5) prev_x = 0;
        if (x_val > prev_x && z_val == 0) { // Modified: Only plot if pen is down
            if (pending_normalization) {
                adc_spectrogram->data_normal[x_val] = adc_spectrogram->size_y/2 - y_val;
            }
            adc_spectrogram->data[x_val] = y_val + adc_spectrogram->data_normal[x_val];
            prev_x = x_val;
        }
    }
    if (pending_normalization) {
        --pending_normalization;
    }
}

/**
 * @brief  ADC MSP Initialization.
 * @param  hadc: ADC handle pointer
 * @retval None
 */
void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    static DMA_HandleTypeDef hdma_adc;

    /* Enable clocks */
    ADCx_CLK_ENABLE();
    ADCx_CHANNEL_GPIO_CLK_ENABLE();
    DMAx_CLK_ENABLE();

    /* Configure GPIO pins for ADC channels */
    GPIO_InitStruct.Pin = ADCx_CHANNEL_PIN | ADCx_CHANNEL_PIN2 | ADCx_CHANNEL_PIN3; // Modified: Add pin3
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(ADCx_CHANNEL_GPIO_PORT, &GPIO_InitStruct);

    /* Configure DMA */
    hdma_adc.Instance = ADCx_DMA_STREAM;
    hdma_adc.Init.Request = DMA_REQUEST_ADC3; // Modified: Use DMA_REQUEST for STM32H7
    hdma_adc.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    hdma_adc.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    hdma_adc.Init.Mode = DMA_CIRCULAR;
    hdma_adc.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_adc.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

    if (HAL_DMA_Init(&hdma_adc) != HAL_OK) {
        adc_error_handler();
    }

    __HAL_LINKDMA(hadc, DMA_Handle, hdma_adc);

    /* Configure NVIC for DMA */
    HAL_NVIC_SetPriority(ADCx_DMA_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(ADCx_DMA_IRQn);
}

/**
 * @brief  ADC MSP De-Initialization.
 * @param  hadc: ADC handle pointer
 * @retval None
 */
void HAL_ADC_MspDeInit(ADC_HandleTypeDef *hadc) {
    ADCx_FORCE_RESET();
    ADCx_RELEASE_RESET();
    HAL_GPIO_DeInit(ADCx_CHANNEL_GPIO_PORT, ADCx_CHANNEL_PIN | ADCx_CHANNEL_PIN2 | ADCx_CHANNEL_PIN3); // Modified: Add pin3
}