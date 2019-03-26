#include <driver/adc.h>
#include "esp_adc_cal.h"
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_VREF 3300

void setup_adc1(adc1_channel_t);
void power_off_adc(void);
esp_adc_cal_characteristics_t* carat_adc (adc_unit_t, adc_atten_t);
uint32_t read_voltage (adc_channel_t, uint32_t);

esp_adc_cal_characteristics_t *adc_chars=NULL;
//https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/adc.html
//ESP32 DevKitC: GPIO 0 cannot be used due to external auto program circuits.
//ESP-WROVER-KIT: GPIO 0, 2, 4 and 15 cannot be used due to external connections for different purposes.

void setup_adc1(adc1_channel_t ch){
    esp_err_t ret_err=ESP_OK;
    adc_power_on();
    ret_err |= adc1_config_width(ADC_WIDTH_BIT_12); 
    ret_err |= adc1_config_channel_atten(ch, ADC_ATTEN_DB_11);
    carat_adc (ADC_UNIT_1, ADC_ATTEN_DB_11);
    printf("\nSetup ADC %s\n",(ret_err==ESP_OK)?"OK":"FAILED");
    return;
}

void power_off_adc(){
    adc_power_off();
}

esp_adc_cal_characteristics_t* carat_adc (adc_unit_t unit, adc_atten_t atten){
     //Characterize ADC at particular atten
    if (adc_chars!=NULL) free(adc_chars);
    adc_chars = (esp_adc_cal_characteristics_t *) calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
    //Check type of calibration value used to characterize ADC
    printf("****\nCaractheristic of ADC unit=%d, atten=%d, bit width=%d\n", adc_chars->adc_num,  adc_chars->atten,  adc_chars->bit_width);
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("\neFuse Vref=%d",adc_chars->vref);
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("\nTwo Point a=%d, b=%d",adc_chars->coeff_a,adc_chars->coeff_b);
    } else if (val_type == ESP_ADC_CAL_VAL_DEFAULT_VREF) {
        printf("Default Vref");
    } else {
        printf("Default");
    }
    return adc_chars;
}

/*Reads an ADC and converts the reading to a voltage in mV.
This function reads an ADC then converts the raw reading to a voltage in mV
based on the characteristics provided. The ADC that is read is also determined by the characteristics.*/

uint32_t read_voltage (adc_channel_t channel,  uint32_t *voltage) {
    esp_adc_cal_get_voltage(channel, adc_chars , voltage);
    printf("\nVoltage on ch=%d is V=%f\n",channel, *voltage/1000.);
    return *voltage;
}