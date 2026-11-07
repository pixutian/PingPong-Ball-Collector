/*
 * adc.c
 * -----------------------------------------------------------------------------
 * 电池电压采样。
 *
 * ADC1 独立工作，12 位、右对齐、单次转换、软件触发。采样前配置规则组通道
 * 与采样时间（480 周期，兼顾精度与速度），转换完成读 DR 即可。
 */

#include "adc.h"
#include "board.h"
#include "app_config.h"

void bat_adc_init(void)
{
    GPIO_InitTypeDef       gpio;
    ADC_CommonInitTypeDef  common;
    ADC_InitTypeDef        adc;

    RCC_AHB1PeriphClockCmd(BAT_GPIO_CLK, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    gpio.GPIO_Pin  = BAT_GPIO_PIN | GPIO_Pin_1;   /* PB0 电池，PB1 备用模拟输入 */
    gpio.GPIO_Mode = GPIO_Mode_AN;
    gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(BAT_GPIO_PORT, &gpio);

    RCC_APB2PeriphResetCmd(RCC_APB2Periph_ADC1, ENABLE);
    RCC_APB2PeriphResetCmd(RCC_APB2Periph_ADC1, DISABLE);

    common.ADC_Mode             = ADC_Mode_Independent;
    common.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
    common.ADC_DMAAccessMode    = ADC_DMAAccessMode_Disabled;
    common.ADC_Prescaler        = ADC_Prescaler_Div6;
    ADC_CommonInit(&common);

    adc.ADC_Resolution           = ADC_Resolution_12b;
    adc.ADC_ScanConvMode         = DISABLE;
    adc.ADC_ContinuousConvMode   = DISABLE;
    adc.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
    adc.ADC_DataAlign            = ADC_DataAlign_Right;
    adc.ADC_NbrOfConversion      = 1;
    ADC_Init(BAT_ADC, &adc);

    ADC_Cmd(BAT_ADC, ENABLE);
}

uint16_t bat_adc_raw(void)
{
    ADC_RegularChannelConfig(BAT_ADC, BAT_SAMPLE_CHANNEL, 1, ADC_SampleTime_480Cycles);

    ADC_SoftwareStartConv(BAT_ADC);
    while (!ADC_GetFlagStatus(BAT_ADC, ADC_FLAG_EOC)) {
        /* 等待转换完成 */
    }

    return ADC_GetConversionValue(BAT_ADC);
}

float bat_adc_voltage(void)
{
    float raw = (float)bat_adc_raw();

    return raw / BAT_ADC_FULL_SCALE * BAT_ADC_REF_VOLT * BAT_DIVIDER_RATIO;
}
