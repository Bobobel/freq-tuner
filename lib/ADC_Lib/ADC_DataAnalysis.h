/****************************************************
 * @file ADC_DataAnalysis.h
 * @brief Data analysis of uint16_t ADC data 
 * @note Noise and low singal amplitude will irritate algorithm
 * @note Buffer length d_len should hold at least 3 signal periods.
 *    So for sFreq given we have a deltaTime of 1/sFreq.
 *    Keep buffer length > 3*period/deltaTime = 3*sFreq/signalFreq !
 *    E.g. sFreq=22000, deltaTime=45.45µs.
 *    In order to analyse signals down to 30Hz, periode=33333µs
 *    buffer length>2200. Even more for higher sFreq!
 * @note Do not use signals above samplerate/3 !!!
 *    Bit depth of ADC data is irrelevant between 8 and 16bit
 *    Use a preamp to have a large amplitude. 
*****************************************************/

#ifndef ADCDATAANALYSIS_H
#define ADCDATAANALYSIS_H

// #define SAMPLERATE (96000)  // default sample rate in Hz
// #define BUFF_SIZE (48000)  // default buffer length
// for high frequency signals you must NOT mean-filter data!!!
//#define FILTERDATA

// maximum small signal difference of ADC readings
#define MAXADCDIFF (8)
// span around mean for digital segmentation. For example 10 <--> (mean-min)/10 for lower threshold. /3 gives middle third of ranges for sin/cos
#define ANASPANDIV (3)
// minimum sample item difference. >2 according to Nyquist but should be even!
#define MINTICDIFF (4)
// length of pos buffer (uint32_t) in calcFreqAnalog on stack = maximum side changes recognized for mean period calculation and quality.
// For higher notes (c5 approx 280 periods) results from classic frequency and reciprocal mean period differ, due to more irregular periods
// thus do not use less than 100 here. 200 worked for me and small signals between ADC 1359 and 1397 for good c4 identification.
#define MAXSIDECHANGES (200)


// A structure to hold ADC data buffer and results
struct sADCData {
  uint16_t *data;       // buffer for ADC DMA data input
  uint32_t d_len;       // length of data buffer
  uint32_t d_sFreq;     // used sample frequency for ADC reading [Hz resp. 1/s]
  float d_deltaTime;    // == 1/d_sFreq in s 
  // first four setup by caller, who provided data buffer
  // next are calculated by my algorithms
  uint16_t d_mean;      // mean value overall   
  uint16_t d_max;       // max
  uint16_t d_min;       // min value overall
  float d_freqClassic; // classic frequency overall: number of periodes/time span [Hz]
  uint16_t d_numCP;     // number of periods counted for d_freqClassic
  float d_periode;     // mean value over maximum MAXSIDECHANGES periodes [s]
  uint16_t d_numPeriodes; // number of periods <= MAXSIDECHANGES used for mean calculation
  float d_quality;     // standard deviation over all periodes if >2       [s]        
};

/*  
  @brief Calculates min, max and mean from (mean filtered, see NOFILTER) ADC data
  @note call peak_mean first and setup vars in sAD with these results before calling calcFreqAnalog
*/
void peak_mean(struct sADCData *, uint16_t *, uint16_t *, uint16_t *);
/*
  @brief Calculates frequency of analog signal (classical method) based on first five variables in sAD
*/
int calcFreqAnalog(struct sADCData *);

#endif
