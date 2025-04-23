void characterize_adc(void);
#ifdef CALIBRATE_ADC
void cal_read_ADC(int32_t num);
#endif
size_t ADC_Sampling(uint16_t *buff, uint32_t len);