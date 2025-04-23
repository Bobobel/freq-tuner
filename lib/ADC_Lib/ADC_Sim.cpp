/*************************************************
 * @brief Simulate an ADC reading
 * @file ADC_Sim.cpp
 * @author Juergen Boehm
 * @date 2025, April 16
 * 
  Copyright (C) <2025>  <Juergen Boehm>
**************************************************/
#ifdef _WIN32
  #include <stdio.h>
  #include <stdlib.h>
#elif defined ESP32
  #include <Arduino.h>
#endif

#include <stdint-gcc.h>   // uint...
#include <math.h>     // sinf, M_PI, sqrtf
#include <float.h>    // FLT_MIN

#include "ADC_Sim.h"
#include "ADC_DataAnalysis.h"

#ifdef _WIN32
  #define PRINT printf
#elif defined ESP32
  #define PRINT Serial.printf
#endif

/*************************************************
 @brief Simulate an ADC reading
 @param[in] sAD: pointer to global ADC structure populated with a data buffer, its length and sample frequency
 @param[in] type: 0=sin, 1=upstep ramp, 2=downstep ramp
 @param[in] freq: signal frequency < samplefrequency/3 !!!
 @param[in] noise: if >0, if <= MAXNOISE : add random +/-noise/2 to clean signal
 @return <0 for errors
 @returns waveform data in sAD.data within range 0..MAXADCVALUE
**************************************************/
int ADC_Sim(struct sADCData *sAD, int type, float freq, uint16_t noise) {
  
    uint16_t ampli, mean, n2;    
    float dTime, dPeriode, periodeTime;  // delta time and periode of signal (freq) in data array. Duration of one periode
    uint32_t sFreq;
    uint16_t *pb;
    static int ic=0;
    uint32_t len;
  
    if(type>2 || type<0) return -1;
    if(freq <= FLT_MIN) return -2;
    if(!sAD)  return -3;
    pb = sAD->data;
    if(!pb)  return -4;
    sFreq = sAD->d_sFreq;
    if(!sFreq)  return -5;
    if(freq >= sFreq/2) return -6;  // Nyquist !
    len = sAD->d_len;
    if(!len) return -7;
    if(noise > MAXNOISE) return -8;
    n2 = noise/2;
  
    // calculate a randomly changed amplitude and mean value within range 0..MAXADCVALUE
    // RAND_MAX is 0x7fff here
    ampli = rand()%(MAXADCVALUE/20) + MAXADCVALUE*2/5  ;
    mean = rand()%(MAXADCVALUE/10) + MAXADCVALUE*2/5;   // thus fmean+ampli <= 1800+400 + 1600+200 = 4000 < MAXADCVALUE
    dTime = 1.0f/sFreq;   // each data entry in pb is of that timestep
    
    /* a periode of 1/freq [s] must match with a spacing of dTime in pb
      e.g. signal of 1000Hz and sFreq=96000Hz:
      The signal shoould fit 1000 times in a buffer of length 96000, thus it's periode would be 96 for any buffer length
      Signal periode within buffer = sFreq/freq.
      dPeriode is float, so take care not toround it for e.g. ramp signals
    */
    dPeriode = (float)sFreq/freq;   // Periode in buffer
    if(dPeriode < FLT_MIN) return -9;
  
    if(type==0) { 
      float dFreq = 2.0f*M_PI/dPeriode;
      int sNoise, signal, signedNoise=0;
        
        // PRINT("sin data with sinF=%g A=%u mean=%u noise=%u length=%lu\n", dFreq, ampli, mean, noise, len);
        //PRINT("Noise: \n");
      for (uint32_t ix=0; ix<len; ix++)  {
        if(noise) {
          sNoise = rand()%noise;
          signedNoise = sNoise - n2;   // -n2 .. +n2
            //PRINT("%d,", signedNoise);
        }
  
        signal = (int)(ampli*sinf(dFreq*ix)) + mean + signedNoise;
        if(signal < 0) signal=0;
        else if(signal > MAXADCVALUE) signal = MAXADCVALUE;
        *pb = signal;  
        pb++;
      } /* for */
        // PRINT("\nEnd noise\n");
        //PRINT("pp at %p\n", pb);
    } /* type 0 */
  
    else { //only type 1 or 2
      float sTime = 0.0f;  // signal time in pb
      int sNoise, signal, signedNoise=0, ampli2;
  
      periodeTime = dTime*dPeriode;
      sTime = periodeTime/2.0f;     // to have a regular start
      ampli2 = 2*ampli;   // twice the sin amplitude is better
  
        PRINT("ramp data with periode %g A=%u mean=%u length=%lu deltaTime=%g\n", periodeTime, ampli, mean, len, dTime);
      
      for (uint32_t ix=0; ix<len; ix++)  {
        if(noise) {
          sNoise = rand()%noise;
          signedNoise = sNoise - n2;
        }
        if(type==1) signal = (int)((sTime/periodeTime) * ampli2) + 100 + signedNoise;
        else signal = (int)(((periodeTime-sTime)/periodeTime) * ampli2) + 100 + signedNoise;
        if(signal < 0) signal=0;
        else if(signal > MAXADCVALUE) signal = MAXADCVALUE;
        *pb = signal;  
        pb++;
        sTime += dTime;
        if(sTime>periodeTime) sTime -= periodeTime;
          //PRINT("sTime=%g\n", sTime);
      } /* for */
    } /* type 1 or 2 */
  
    return 0;
  
  } /* ADC_Sim */
