#include "stm32h7xx_hal.h" // STM32H7 HAL for STM32H723ZG

/* 
 * Configures ADC1 and ADC2 on STM32H723ZG in double-interleaved mode (7.2 MSPS, 16-bit)
 * to sample HP141T signals: horizontal (V_x), vertical (V_y), pen-lift (V_z).
 * System clock: 480 MHz, ADC clock: 36 MHz, sampling rate: 7.2 MSPS.
 * DMA2 transfers interleaved data to a buffer, processed for spectrogram display.
 */

#define ADCx_MASTER                     ADC1
#define ADCx_SLAVE                      ADC2
#define ADCx_CLK_ENABLE()               do { __HAL_RCC_ADC12_CLK_ENABLE(); } while(0)
#define DMAx_CLK_ENABLE()               __HAL_RCC_DMA2_CLK_ENABLE()
#define ADCx_CHANNEL_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOF_CLK_ENABLE()

#define ADCx_FORCE_RESET()              __HAL_RCC_ADC12_FORCE_RESET()
#define ADCx_RELEASE_RESET()            __HAL_RCC_ADC12_RELEASE_RESET()

/* GPIO Pins for ADC Channels */
#define ADCx_CHANNEL_PIN                GPIO_PIN_10 // V_x, PF10 (ADC1_INP8)
#define ADCx_CHANNEL_PIN2               GPIO_PIN_9  // V_y, PF9 (ADC1_INP7)
#define ADCx_CHANNEL_PIN3               GPIO_PIN_8  // V_z, PF8 (ADC1_INP6)
#define ADCx_CHANNEL_GPIO_PORT          GPIOF

/* ADC Channels */
#define ADCx_CHANNEL                    ADC_CHANNEL_8  // V_x
#define ADCx_CHANNEL2                   ADC_CHANNEL_7  // V_y
#define ADCx_CHANNEL3                   ADC_CHANNEL_6  // V_z

/* DMA Configuration */
#define ADCx_DMA_CHANNEL                DMA_REQUEST_ADC1 // Master ADC1
#define ADCx_DMA_STREAM                 DMA2_Stream0
#define ADCx_DMA_IRQn                   DMA2_Stream0_IRQn
#define ADCx_DMA_IRQHandler             DMA2_Stream0_IRQHandler

ADC_HandleTypeDef    AdcHandleMaster;
ADC_HandleTypeDef    AdcHandleSlave;

#define ADC_BUF_SIZE 8192 // 8192 samples (~1.14 ms at 7.2 MSPS, 2730 samples/channel)

volatile uint16_t adc_buffer[ADC_BUF_SIZE] = {0}; // 16-bit for interleaved data

volatile int adc_error_flag = 0;

/**
 * @brief  Handles DMA interrupt request for ADC1.
 * @param  None
 * @retval None
 */
void ADCx_DMA_IRQHandler(void) {
    HAL_DMA_IRQHandler(AdcHandleMaster.DMA_Handle);
}

/**
 * @brief  Error handler for ADC initialization or configuration failures.
 * @param  None
 * @retval None
 */
void adc_error_handler(void) {
    adc_error_flag = 1;
    while(1) {} // Add debug output in production
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
 * @brief  Initializes ADC1/ADC2 in double-interleaved mode and DMA.
 * @param  None
 * @retval 0 on success, -1 on failure
 */
int adc_init(void) {
    ADC_ChannelConfTypeDef sConfig;
    ADC_MultiModeTypeDef multiMode;

    /* Configure ADC1 (Master) */
    AdcHandleMaster.Instance = ADCx_MASTER;
    AdcHandleMaster.Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV4; // ~36 MHz
    AdcHandleMaster.Init.Resolution = ADC_RESOLUTION_16B; // 16-bit
    AdcHandleMaster.Init.ScanConvMode = ENABLE;
    AdcHandleMaster.Init.ContinuousConvMode = ENABLE;
    AdcHandleMaster.Init.DiscontinuousConvMode = DISABLE;
    AdcHandleMaster.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    AdcHandleMaster.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    AdcHandleMaster.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    AdcHandleMaster.Init.NbrOfConversion = 3; // V_x, V_y, V_z
    AdcHandleMaster.Init.DMAContinuousRequests = ENABLE;
    AdcHandleMaster.Init.EOCSelection = ADC_EOC_SEQ_CONV;
    AdcHandleMaster.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;

    if (HAL_ADC_Init(&AdcHandleMaster) != HAL_OK) {
        adc_error_handler();
        return -1;
    }

    /* Configure ADC2 (Slave) */
    AdcHandleSlave.Instance = ADCx_SLAVE;
    AdcHandleSlave.Init = AdcHandleMaster.Init; // Same config as master
    if (HAL_ADC_Init(&AdcHandleSlave) != HAL_OK) {
        adc_error_handler();
        return -1;
    }

    /* Configure double-interleaved mode */
    multiMode.Mode = ADC_DUALMODE_INTERL_FAST; // Fast interleaved mode
    multiMode.DualModeData = ADC_DUALMODEDATAFORMAT_32_10_BITS;
    multiMode.TwoSamplingDelay = ADC_TWOSAMPLINGDELAY_1CYCLE;
    if (HAL_ADCEx_MultiModeConfigChannel(&AdcHandleMaster, &multiMode) != HAL_OK) {
        adc_error_handler();
        return -1;
    }

    /* Configure channels for ADC1 */
    sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5; // Fast sampling for 7.2 MSPS
    sConfig.Offset = 0;

    /* V_x (Horizontal) */
    sConfig.Channel = ADCx_CHANNEL;
    sConfig.Rank = 1;
    if (HAL_ADC_ConfigChannel(&AdcHandleMaster, &sConfig) != HAL_OK) {
        adc_error_handler();
        return -1;
    }

    /* V_y (Vertical) */
    sConfig.Channel = ADCx_CHANNEL2;
    sConfig.Rank = 2;
    if (HAL_ADC_ConfigChannel(&AdcHandleMaster, &sConfig) != HAL_OK) {
        adc_error_handler();
        return -1;
    }

    /* V_z (Pen-lift) */
    sConfig.Channel = ADCx_CHANNEL3;
    sConfig.Rank = 3;
    if (HAL_ADC_ConfigChannel(&AdcHandleMaster, &sConfig) != HAL_OK) {
        adc_error_handler();
        return -1;
    }

    /* Start ADC with DMA (master only) */
    if (HAL_ADCEx_MultiModeStart_DMA(&AdcHandleMaster, (uint32_t*)adc_buffer, ADC_BUF_SIZE) != HAL_OK) {
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
    for (int j = 0; j < ADC_BUF_SIZE/6; ++j) {
        uint32_t x_val = adc_buffer[j*3];     // V_x
        uint32_t y_val = adc_buffer[j*3+1];   // V_y
        uint32_t z_val = adc_buffer[j*3+2];   // V_z
        x_val = (x_val * adc_spectrogram->npoints) / 65536; // 16-bit scaling
        y_val = adc_spectrogram->size_y - (adc_spectrogram->size_y * y_val) / 65536;
        z_val = z_val > 32768 ? 1 : 0; // Threshold at mid-scale (~1.65 V)
        if (x_val < 5) prev_x = 0;
        if (x_val > prev_x && z_val == 0) {
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
    for (int j = ADC_BUF_SIZE/6; j < ADC_BUF_SIZE/3; ++j) {
        uint32_t x_val = adc_buffer[j*3];
        uint32_t y_val = adc_buffer[j*3+1];
        uint32_t z_val = adc_buffer[j*3+2];
        x_val = (x_val * adc_spectrogram->npoints) / 65536;
        y_val = adc_spectrogram->size_y - (adc_spectrogram->size_y * y_val) / 65536;
        z_val = z_val > 32768 ? 1 : 0;
        if (x_val < 5) prev_x = 0;
        if (x_val > prev_x && z_val == 0) {
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

    if (hadc->Instance == ADCx_MASTER) {
        /* Enable clocks */
        ADCx_CLK_ENABLE();
        ADCx_CHANNEL_GPIO_CLK_ENABLE();
        DMAx_CLK_ENABLE();

        /* Configure GPIO pins */
        GPIO_InitStruct.Pin = ADCx_CHANNEL_PIN | ADCx_CHANNEL_PIN2 | ADCx_CHANNEL_PIN3;
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(ADCx_CHANNEL_GPIO_PORT, &GPIO_InitStruct);

        /* Configure DMA */
        hdma_adc.Instance = ADCx_DMA_STREAM;
        hdma_adc.Init.Request = ADCx_DMA_CHANNEL;
        hdma_adc.Init.Direction = DMA_PERIPH_TO_MEMORY;
        hdma_adc.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_adc.Init.MemInc = DMA_MINC_ENABLE;
        hdma_adc.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD; // 16-bit
        hdma_adc.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
        hdma_adc.Init.Mode = DMA_CIRCULAR;
        hdma_adc.Init.Priority = DMA_PRIORITY_HIGH;
        hdma_adc.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

        if (HAL_DMA_Init(&hdma_adc) != HAL_OK) {
            adc_error_handler();
        }

        __HAL_LINKDMA(hadc, DMA_Handle, hdma_adc);

        /* Configure NVIC */
        HAL_NVIC_SetPriority(ADCx_DMA_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(ADCx_DMA_IRQn);
    }
}

/**
 * @brief  ADC MSP De-Initialization.
 * @param  hadc: ADC handle pointer
 * @retval None
 */
void HAL_ADC_MspDeInit(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADCx_MASTER) {
        ADCx_FORCE_RESET();
        ADCx_RELEASE_RESET();
        HAL_GPIO_DeInit(ADCx_CHANNEL_GPIO_PORT, ADCx_CHANNEL_PIN | ADCx_CHANNEL_PIN2 | ADCx_CHANNEL_PIN3);
    }
}