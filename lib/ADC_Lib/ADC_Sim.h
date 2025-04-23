/*************************************************
 @brief Simulate an ADC reading
 @file ADC_Sim.h
 @note uses ADC_DataAnlysis.h
*************************************************/
#ifndef ADCSIM_H
#define ADCSIM_H

#include "ADC_DataAnalysis.h"

/******************************************
 * Important defines 
 * When changeing these change random calculation in ADC_Sim as well !
*******************************************/
#define MAXNOISE (600)
#define MAXADCVALUE (4095)

int ADC_Sim(struct sADCData *, int , float , uint16_t );

#endif
