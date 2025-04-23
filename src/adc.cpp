/*********************************************************************************************
 * Specific ADC routines
 * 1) Calibration from eso_adc_cal
 * 2) my calibration for 1x/10x measuring probe
 * 3) my screen transform of ADC values directly into screen coordinates 
 * 4) 
**********************************************************************************************/
#include <Arduino.h>
//#include <stddef.h> // within Arduino.h
#include <esp_adc_cal.h>
#include <esp_log.h>
#include <driver/i2s.h>
#include "main.h"

esp_adc_cal_characteristics_t adc_chars;
const char TAG[] = "ADC";


/******************************************
 * ESP calibration and setup of ADC
*******************************************/
void characterize_adc() {
  esp_adc_cal_value_t retval;
  esp_err_t ret;
  bool cali_enable = false;

  ret = esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP);
  //Serial.printf("esp_adc_cal_check_efuse returned for TP (0==OK): %d\n", ret);
  ESP_LOGI(TAG, "esp_adc_cal_check_efuse returned for TP (0==OK): %d", ret);
  ret = esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF);
  //Serial.printf("esp_adc_cal_check_efuse returned for VREF (0==OK): %d\n", ret);
  ESP_LOGI(TAG, "esp_adc_cal_check_efuse returned for VREF (0==OK): %d", ret);

  if (ret == ESP_ERR_NOT_SUPPORTED) {
      ESP_LOGW(TAG, "Calibration scheme not supported, skip software calibration");
  } else if (ret == ESP_ERR_INVALID_VERSION) {
      ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
  } else if (ret == ESP_OK) {
      cali_enable = true;

    retval = esp_adc_cal_characterize(
              (adc_unit_t)ADC_UNIT_1,     
              //(adc_atten_t)ADC_CHANNEL,   // this should be an attenuation, not a channel!!!
              (adc_atten_t)ADC_ATTEN_DB_12,
              (adc_bits_width_t)ADC_WIDTH_BIT_12,
              1100,
              &adc_chars);
    //Serial.printf("esp_adc_cal_characterize returned: %d\n", retval);
    ESP_LOGI(TAG,   "esp_adc_cal_characterize returned: %d", retval);

    /*
    enum esp_adc_cal_value_t
      Type of calibration value used in characterization.
      Values:
      enumerator ESP_ADC_CAL_VAL_EFUSE_VREF
          Characterization based on reference voltage stored in eFuse
      enumerator ESP_ADC_CAL_VAL_EFUSE_TP
          Characterization based on Two Point values stored in eFuse
      enumerator ESP_ADC_CAL_VAL_DEFAULT_VREF
          Characterization based on default reference voltage
      enumerator ESP_ADC_CAL_VAL_EFUSE_TP_FIT
          Characterization based on Two Point values and fitting curve coefficients stored in eFuse
      enumerator ESP_ADC_CAL_VAL_MAX
      enumerator ESP_ADC_CAL_VAL_NOT_SUPPORTED
    */
    ESP_LOGI(TAG, "Calibration with Vref enabled. esp_adc_cal_characterize returned: %d but yet not used.", retval);
  } else {
    ESP_LOGE(TAG, "Invalid arg for esp_adc_cal_check_efuse!");
  }
}


/***************************************
 * Sampling len ADC values into buff
 * Returns: number of 16bit samples read
 * of -esp_err_t in case of error.
****************************************/
size_t ADC_Sampling(uint16_t *buff, uint32_t len) 
{
  size_t bytes_read;  //, num2=NUM_SAMPLES*sizeof(uint16_t);
  esp_err_t ret;

  // read all at once:
  ret = i2s_read(I2S_NUM_0, buff,  len* sizeof(uint16_t), &bytes_read, 150); 
  if(ret != ESP_OK) {
    //Serial.printf("i2s_read returned error %d\n", ret);
    ESP_LOGE(TAG, "i2s_read returned error %d", ret);
    return (-ret);
  }
  else {
    ESP_LOGD(TAG, "i2s_read %d Bytes", bytes_read);    // check next time, if return should be size_t
    return (bytes_read>>1);   // number of samples returned, not no bytes!
  }
} /* ADC_Sampling */


#ifdef CALIBRATE_ADC
/**********************************************
 * Read ADC raw values for calibration curve
 * Compare to voltage at inport!
 * num: numer of samples for any mean reading
***********************************************/
void cal_read_ADC(int32_t num)
{
  uint32_t read_num;
  uint32_t averaged_reading;
  uint64_t read_sum = 0;
  uint16_t val, minv, maxv;

  if(num<=5) return;

  read_num = ADC_Sampling(i2s_buff, num);
  if(read_num == num)  {
    minv=0xFFFF;  maxv=0;
    for(int32_t i=0; i<read_num; i++) {
      val = i2s_buff[i] & 0x0FFF;
      if(val>maxv)  maxv = val;
      if(val<minv)  minv = val;
      read_sum += val;
    }
    averaged_reading = read_sum / read_num;
    Serial.printf("averaged_reading = %d =%5.3g[V] z=%d over %d samples. min=%u, max=%u\n", averaged_reading, to_voltage(averaged_reading), to_scale(averaged_reading), read_num, minv, maxv); // Print with additional info
  } 
  else Serial.printf("ADC_Sampling returned %d\n", read_num);
} /* cal_read_ADC */
#endif


