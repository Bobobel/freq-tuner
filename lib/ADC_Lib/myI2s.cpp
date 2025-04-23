/*******************************************
 * prepare I2S using I2S_NUM_0 
 * for fast DMA ADC reading
********************************************/
#include <Arduino.h>
#include <stdlib.h>
#include <driver/i2s.h>
#include <driver/adc.h>
#include "soc/soc.h"
#include "soc/syscon_reg.h"
#include "soc/syscon_struct.h"    // mainly the same


/**********************************************
 * From https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-reference/peripherals/i2s.html
Please follow these steps to prevent data lost:

  1. Determine the interrupt interval. Generally, when data lost happened, the interval should be the bigger the better, it can help to reduce the interrupt times, i.e., dma_buf_len should be as big as possible while the DMA buffer size won’t exceed its maximum value 4092. The relationships are:

    interrupt_interval(unit: sec) = dma_buf_len / sample_rate
    dma_buffer_size = dma_buf_len * slot_num * data_bit_width / 8 <= 4092

  2. Determine the dma_buf_count. The dma_buf_count is decided by the max time of i2s_read polling cycle, all the received data are supposed to be stored between two i2s_read. This cycle can be measured by a timer or an outputting gpio signal. The relationship is:

    dma_buf_count > polling_cycle / interrupt_interval

  3. Determine the receiving buffer size. The receiving buffer that offered by user in i2s_read should be able to take all the data in all dma buffers, that means it should be bigger than the total size of all the dma buffers:

    recv_buffer_size > dma_buf_count * dma_buffer_size

  To check whether there are data lost, you can offer an event queue handler to the driver during installation:

    QueueHandle_t evt_que;
    i2s_driver_install(i2s_num, &i2s_config, 10, &evt_que);

    You will receive I2S_EVENT_RX_Q_OVF event when there are data lost.

/// @brief  Configure the I2S bus 0
/// @param[IN]  rate  : sample rate in kHz
******************************************/

void configure_i2s(int rate, int ADC_Chan) {
  /*keep in mind: https://docs.espressif.com/projects/esp-idf/en/release-v4.4/esp32/api-reference/peripherals/i2s.html?highlight=i2s_adc_enable
      interrupt_interval(unit: sec) = dma_buf_len / sample_rate
      dma_buf_count > max_time(I2S_read_polling_cycle) / interrupt_interval
      all the received data are supposed to be stored between two i2s_read
      dma_buffer_size = dma_buf_len * dma_buf_count * bits_per_sample/8 <= 4092
      In our case : sample_rate=1000000, I2S_BITS_PER_SAMPLE_16BIT=16
      but polling cycle is unknown! Sprite_draw ca. 37 ms, data analysis ca 37 ms, mayby polling_cycle=80ms?
      ==> dma_buf_len = 4092(==dma_buffer_size)/2 = 2046 (max!)
      ==> interrupt_interval = 2046/1000000 = 2.046ms
      ==> dma_buf_count > 80/2.046 ca. 40
      real overall buffer size recommended: recv_buffer_size > dma_buf_count * dma_buffer_size
      If all i2s_read are performed in sequence, polling_cycle<1ms, thus a few dma_buffers suffice.
      Measured: 620ms for all of 48000 samples, thus 13ms for each i2s_read. Maybe 15=dma_buf_count?
  */
  i2s_config_t i2s_config =
  {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),  // I2S receive mode with ADC
    .sample_rate = (uint32_t)rate,                                                 // sample rate
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,                                 // 16 bit I2S
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,      // only the left channel originally:I2S_CHANNEL_FMT_ALL_LEFT will give half sample frequency!!!
    //.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),   // I2S format
    // here is a difference in doc: I2S_COMM_FORMAT_I2S_MSB=1 but I2S_COMM_FORMAT_STAND_MSB=2
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S | I2S_COMM_FORMAT_STAND_MSB),  // updated @JB
    .intr_alloc_flags = 0, //1, // 1 is not defined! ESP_INTR_FLAG_LEVEL1==1<<1=2, // 0: default. ESP_INTR_FLAG_LEVEL1,  in examples changed from 1 @JB       // none
    .dma_buf_count = 8,                                                           // number of DMA buffers. @JB changed to 8
    .dma_buf_len = 1024,                                                   // number of samples (samples, see bits_per_sample)
    .use_apll = 0, //true,                                                              //@JB was 0=no Audio PLL, @JB but which clock?
    //.fixed_mclk =  0                                                      
  };
  
  if(ESP_OK != adc1_config_channel_atten((adc1_channel_t)ADC_Chan, ADC_ATTEN_DB_12)){
    Serial.printf("Error setting up ADC attenuation. Halt!");
    while(1);
  }
  if(ESP_OK != adc1_config_width(ADC_WIDTH_BIT_12)){ // Configure ADC1 capture width, meanwhile enable output invert for ADC1
    Serial.printf("Error setting ADC bit width. Halt!");
    while(1);
  }
  if(ESP_OK != i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL)){
    Serial.printf("Error installing I2S. Halt!");
    while(1);
  }

  Serial.println("I2S driver installed!");

  if(ESP_OK != i2s_set_adc_mode(ADC_UNIT_1, (adc1_channel_t)ADC_Chan)) {  // ??? In this mode, the ADC maximum sampling rate is 150KHz !!!
    Serial.printf("Error setting up ADC mode. Halt!");
    while(1);
  }
  
  /* from original, but seems to be irrelevant:
  //SET_PERI_REG_MASK(SYSCON_SARADC_CTRL2_REG, SYSCON_SARADC_SAR1_INV);   Instead using https://esp32.com/viewtopic.php?t=15849 :
  SYSCON.saradc_ctrl2.sar1_inv = 1;	    //SAR ADC samples are inverted by default
  //SYSCON.saradc_ctrl.sar1_patt_len = 0; //Use only the first entry of the pattern table
  
  //SYSCON.saradc_sar1_patt_tab[0] = 0x5C0F0F0F; //Only MSByte for channel 0 : ch_sel:3, bit_width:3?, atten:2? in one byte
  // also in details: ...=((ADC1_CHANNEL_0 << 4) | (ADC_WIDTH_BIT_12 << 2) | ADC_ATTEN_DB_11) << 24
  // ***IMPORTANT*** enable continuous adc sampling
  SYSCON.saradc_ctrl2.meas_num_limit = 0;   // ref above claims, that this is ineffective
  */

  //Serial.printf("I2S ADC setup OK.\n");
  vTaskDelay(1000/portTICK_PERIOD_MS); //required for stability of ADC  , see ref above
  
  // start ADC sampling
  if(ESP_OK != i2s_adc_enable(I2S_NUM_0)){
    Serial.printf("Error enabling ADC. Halt!");
    while(1);
  }
}



void set_sample_rate(uint32_t rate, int ADC_Chan) {   // rate [Hz]
  i2s_driver_uninstall(I2S_NUM_0);
  configure_i2s(rate, ADC_Chan);
}
