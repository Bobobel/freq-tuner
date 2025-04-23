/**********************************************************
 @brief Data analysis of uint16_t ADC data    
 @file ADC_DataAnalysis.c
 @author Juergen Boehm
 @date 2025, April 14
 @include filter.h  [not actually, see below !]
 @note Compiler: GCC under Win32 resp. Espressif
 @note  All data is uint16_t (bit depth is irrelevant. 
        I use 12 bit for data between 0 and 4095 resp. 2^bitsize-1) .
 @note Using 'u's for microseconds
 @implements Private algorithm counting upstep side changes of signal.
 @note: Frequency calculation: (from first pos sidechange to last)/time used.A0
        You may prefer d_periode with appropriate MAXSIDECHANGES
        in order to avoid scanning through all data, if d_len is large.
        mean filtering smoothes high frequency signals too much!!!

 Copyright (C) <2025>  <Juergen Boehm>
***********************************************************/
#ifdef _WIN32
#include <stdio.h>
#include <stdlib.h>
#elif defined ESP32
#include <Arduino.h>
#endif

#include <stdint-gcc.h>
#include <math.h>   // sqrtf
#include <float.h>    // FLT_MIN
#include <stdbool.h>

#include "ADC_DataAnalysis.h"

#ifdef FLTERDATA
#include "filter.h"
#endif

/*********************************************************
 * @brief Calculates min, max and mean from (mean filtered) ADC data
 * @param[in] sAD: pointer to global ADC structure populated with a data buffer, its length and sample frequency
 * @param[out] *max_value   
 * @param[out] *min_value
 * @param[out] *mean_value 
**********************************************************/
void peak_mean(struct sADCData *sAD, uint16_t *max_value, uint16_t *min_value, uint16_t *mean_value) {
    uint16_t value;
    uint32_t mean;   // bufferlength*(2^16) should fit within 32 bit (for 16 bit AC data)!
    uint16_t *pb;
    
    pb = sAD->data;
    mean = (int32_t)pb[0]; 
    *max_value = pb[0];
    *min_value = pb[0];
#ifdef FLTERDATA
    mean_filter_init(5, mean);
#endif
  
    for (uint32_t i = 1; i < sAD->d_len; i++) {
#ifdef FLTERDATA 
      value = (uint16_t)mean_filter((int32_t)pb[i]);   // no risk with uint16_t data !
#else 
      value = pb[i];
#endif
      if (value > *max_value)       *max_value = value;
      else if (value < *min_value)  *min_value = value;
      mean += (uint32_t)value;
    }
    mean /= sAD->d_len;  // as a result this should be again between 0 and 2^bitsize
    // mean = to_voltage((uint16_t)mean);
    *mean_value = (uint16_t)mean;
  } /* peak_mean */

  /************************************************************************
 * @brief Calculates frequency of analog signal (from periods, not the classical method)
 *        based on sample_frequency in sAD
 * @cond Signal frequency < samplerate/3 !!!
 * @param[in] sAD: pointer to global ADC structure populated with a data buffer, its length, sample frequency, and deltaTime=1/sampleFreq
 * @param all others witin sAD:
 * @param[in] d_mean          statistics for filtered ADC data
 * @param[in] d_max         "   " 
 * @param[in] d_min         "   "
 * @param[out] d_freqClassic     signal's approximated frequency [Hz]
 * @param[out] d_numCP      number of periods counted for d_freqClassic
 * @param[out] d_periode        mean of a (reduced) number of periods
 * @param[out] d_numPeriodes    count of periods used for quality measure 
 * @param[out] d_quality   standard deviation of signal's periode length
 * @return <0 for errors
*************************************************************************/
int calcFreqAnalog(struct sADCData *sAD) {
    uint16_t sideChanges = 0, allPeriods = 0;    // counts sign changes
    uint16_t lower_wc, upper_wc;    // center band limits
    uint16_t *pb;
    uint32_t sFreq, len, firstPeriodeStart=-1;
    uint16_t max_v, min_v, temp, minticdiff2 = MINTICDIFF >>1;
    float dTime;
    bool signal_side = false; // does the signal lie beyond threshold (true) or not?
    uint32_t pos[MAXSIDECHANGES], mean=0, lastPos, utemp;   // stack overflow ???   
    float ftemp, stdev=0.0f;    
    int32_t iValue;

    // check input
    if(!sAD)  return -3;
    pb = sAD->data;
    if(!pb)  return -4;
    sFreq = sAD->d_sFreq;
    if(!sFreq)  return -5;
    len = sAD->d_len;
    if(!len) return -7;
    dTime = sAD->d_deltaTime;
    if(dTime <= FLT_MIN) return -6;

    // check constant data signal
    max_v = sAD->d_max; 
    min_v = sAD->d_min;
    if( max_v - min_v <= MAXADCDIFF) {
        sAD->d_freqClassic = 0.0f;
        sAD->d_numCP = 0;
        sAD->d_numPeriodes = 0;
        sAD->d_quality = sAD->d_periode = FLT_MAX;
        return -1;
    }
    
    // calculate center limits depending on data
    lower_wc = sAD->d_mean - (sAD->d_mean - min_v)/ANASPANDIV;
    if(lower_wc <= min_v + MAXADCDIFF) lower_wc = sAD->d_mean - MAXADCDIFF/2;  // for non-symmetric data
    
    upper_wc = sAD->d_mean + (max_v - sAD->d_mean)/ANASPANDIV;   
    if(upper_wc >= max_v - MAXADCDIFF) upper_wc = sAD->d_mean + MAXADCDIFF/2;  // for non-symmetric data

    /* *** data segmentation : *** */

    // Get initial signal relative to upper_wc (uphill detection). 
    temp = 0;
#ifdef FLTERDATA
    mean_filter_init(5, (int32_t)pb[0]);
#endif
    if(pb[0] > upper_wc) temp++;
    for (int i = 0 ; i <MINTICDIFF; i++) {
#ifdef FLTERDATA
        iValue = mean_filter((int32_t)pb[i]);  // should work!
#else 
        iValue = (int32_t)pb[i];
#endif
        if(iValue > (int32_t)upper_wc) temp++;
    }
    if (temp > minticdiff2 ) {      // strict control
        signal_side=true;
    }

#ifdef FLTERDATA
    // init mean filter, maybe inconstistent median versus mean?
    iValue = 0;
    for (uint32_t i = 0 ; i <MINTICDIFF; i++) {
        iValue += (int32_t)pb[i];
    }
    mean_filter_init(5, (iValue/MINTICDIFF));
#endif

    // loop over data
    for (uint32_t i = MINTICDIFF ; i < len; i++) {
#ifdef FLTERDATA
        iValue = mean_filter((int32_t)pb[i]);
#else
        iValue = (int32_t)pb[i];
#endif
        //  if signal_side=true
        if(signal_side) {
            // has signaldropped out of non lower region?
            if(iValue <= (int32_t)lower_wc) signal_side=false;   // hysterisis !
            continue;
        }
        else {  // signal_side == false
            // check if data is above threshold
            if(iValue > (int32_t)upper_wc) {
                signal_side=true;
                // remember side change
                if(sideChanges < MAXSIDECHANGES-1)  {
                    pos[sideChanges] = i;            
                    sideChanges++;  // count them starting with 1
                }
                // call periods anyway for classical frquency calculation. Else you may quit here.
                lastPos = i;
                    //Serial.printf("calcFA: period %u pos %u\n", allPeriods, lastPos);
                allPeriods++;
            }
            continue;
        }
    }   /* for */

    /* *** evaluation *** */
    sAD->d_numPeriodes = sideChanges-1;
    // mean periode is now last saved periode start minus first periode start divided by number of periodes in between
    if(sideChanges<=1)    {
        sAD->d_freqClassic = 0.0f;
        sAD->d_numCP = allPeriods-1;
        sAD->d_quality = sAD->d_periode = FLT_MAX;
        return -2;
    }
    
    for(uint16_t i=0; i< sideChanges-1; i++) {
        utemp = pos[i+1] - pos[i];
        mean += utemp;
    }
    // remember: real time is step in data times dTime:
    sAD->d_periode = (float)mean*dTime/(float)(sideChanges-1);
    // Classical frequency calculation may have a different result: 
    sAD->d_freqClassic = (float)(allPeriods-1)/(float)(lastPos - pos[0])/dTime;       
        /*
        Serial.printf("Debug: firstPos=%lu lastPos=%lu allPeriods=%lu F=%g mean=%u meanP=%g\n", 
            pos[0], lastPos, allPeriods, sAD->d_freqClassic, mean/(sideChanges-1), sAD->d_periode);
        */
    sAD->d_numCP = allPeriods-1;

    // get a quality measure from standard deviation of periods
    // with two sideChanges I could only evaluate one periode, thus for stdev we must have sideChanges>=3
    if(sideChanges>=3)  {
        ftemp = 0.0f;
        for(uint16_t i=0; i< sideChanges-1; i++) {
            ftemp = (float)(pos[i+1] - pos[i])*dTime - sAD->d_periode; 
            stdev += (float)ftemp*ftemp;
        }
        stdev /=  (float)(sideChanges-2);
        sAD->d_quality = sqrtf(stdev);
    }
    else sAD->d_quality = 0.0f;     // no hint for user, that the result depends only on one periode. Introduced sAD->d_numPeriodes and d_numCP.

    return 0;

} /* calcFreqAnalog*/
  
