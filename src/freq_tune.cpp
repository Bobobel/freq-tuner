/*******************************************************************
 @brief Audio frequency analysis of uint16_t ADC data    
 @file freq_tune.cpp
 @author Juergen Boehm
 @date 2025 April 21
 @include freq_tune.h
 @note All data is uint16_t resp. float between 0 and 4095.
 @note Using 'u's for microseconds
 @implements new calculation for audio frequency
 @uses external lib: TFT_eSPI version 2.5.43
 
 Stand alone routine using a TFT display bar graph (sprite) 
 and an ADC sample freq of 30000 (or 48000).
 Implementing the classical wave counting over a long time periode.
 AND: using mean of periods, than quality is measured by taking 
 standard deviation from periode times
 (reciprocal method of frequency calculation without interrupts)

 @Problem: 400Hz standard reads +12 cent 
 with 440Hz.ogg, XFM2, and keyboard independent of sample freq!
 @Solution: -12cent correction here after freq calculation.
 @Problem: From c4 onward results become "orange".
 @Analysis: for h5 period=124 and stdev=17 with N=498 due to the fact
 that there are only 3 to 4 samples for each period.
 Increasing to sFreq=48000 gave somehow reduced Q=10.
 In any case classic F and 1/period where almost the same!
 @Solution: Reduce quality limit to 15% 

 Copyright (C) <2025>  <Juergen Boehm>
********************************************************************/
#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint-gcc.h>
#include <driver/i2s.h>
#include <TFT_eSPI.h>
#include <esp_log.h>
#include <float.h>

#include "main.h"
// includes for ADC-DMA reading
#include "adc.h"
#include "myI2S.h"
#ifdef DEBUG_BUF
#include "debug_routines.h"
#endif
// special includes 
#include "ADC_DataAnalysis.h"
#include "AFrequencies.h"

#define I2S_NUM         (0)   // I2S channel used with ADC reading
#define MINFREQQUALITY  (0.15f)  // set minimum value of stdev/periode
#define MINFREQDIFF     (0.1f)  // relative difference between two frequences, resp. 10 cent
// Tuning of 400Hz. Deviation in cent:
#define TUNINGCENT (-12)
// In order to improve speed: (TWELFTH_SQ2 from AFrequencies.h) Attention : -TUNINGCENT only when TUNINGCENT<0   !
#define TUNINGFACTOR (((TWELFTH_SQ2-1.0f)*(float)(-TUNINGCENT)/100.0f) + 1.0f)


/* Structure plan
// 1. Setup ADC
// 2. Input ADC data stream (printout for debugging)
// 3. Mean filter and converted into rectangular data: not used!
// 4. get all upstep (or downstep) positions "pos"
// 5. last minus first pos divided by time used gives classical frequency
		With 96000Hz sampling we have a time accuracy above 11µs reqp. 1.1% at 1kHz
		c3 is at 1046.502Hz and cis3 at 1108,731Hz, thus one cent is 0.62228Hz resp 0.06% much lower
		But with a sampling time length of 1s we get approx 91 pos, so the 11µs have to be divided by sqrt(91) {approx. 9.5} 
		so we are in the range of 1µs so approx. 1.5 cent. 
		With 48000/s sampling accuracy is only 3 cent.
// 6. Mean of periode times gives reciprocal frequency. Accuracy as above if stdev is near zero.
// 7. stdev of periode times gives quality of freq. calc.
// 8. printout results and display in widget meter, then repeat with 2.
*/
const char TAG[] = "FreqTune";

TFT_eSPI tft = TFT_eSPI();         // Invoke custom library
TFT_eSprite barGraph = TFT_eSprite(&tft);

// structure to hold ADC data, parameters and results. Defined in ADC_DataAnalysis.h
struct sADCData gsAD;


// =========================================================================
// General heap info.
// heap_caps_get_info only gives 0 with PlatformIO
// heap_caps_check_integrity_all then runs forever
// =========================================================================
void showHeapInfo(void) {
  size_t freeDMAHeap, freeHeap, freeLargest;

  freeHeap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
  freeDMAHeap = heap_caps_get_free_size(MALLOC_CAP_DMA);
  freeLargest = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
  ESP_LOGI(TAG,"Free heap: %u  free DMA heep: %u  largest block: %u", freeHeap, freeDMAHeap, freeLargest);
  return;
  /* NO FUNCTION with platformio/espressif32@^6.10.0
  multi_heap_info_t info;
  heap_caps_get_info(&info, (0UL | MALLOC_CAP_DEFAULT | MALLOC_CAP_8BIT | MALLOC_CAP_DMA | MALLOC_CAP_IRAM_8BIT));
  log_i("Total free bytes: %u", info.total_free_bytes);
  log_i("Total allocates bytes: %u", info.total_allocated_bytes);
  log_i("Total No. blocks: %u", info.total_blocks);
  log_i("No. allocated blocks: %u", info.allocated_blocks); 
  log_i("No. free blocks: %u", info.free_blocks); 
  log_i("Total size of all: %u", heap_caps_get_total_size(0UL | MALLOC_CAP_DEFAULT | MALLOC_CAP_8BIT | MALLOC_CAP_DMA | MALLOC_CAP_IRAM_8BIT));
  
  heap_caps_check_integrity_all(true);
  heap_caps_print_heap_info(MALLOC_CAP_8BIT | MALLOC_CAP_DMA | MALLOC_CAP_IRAM_8BIT);   // gives all 0 and not two lines for each caps!!
  */
} /* showHeapInfo */

/*********************************************
 * @brief Inits a bar graph of size bgHight, bgWidth
 * at bgX, bgY. Main tuning display.
 * Units -50 .. +50 spanning over 202 pixels 
 * with marks at +-10 , +-25 , +-50.
 * Gives a red bar from -25 to +25 for invalid.
 * Green bar when quality is good, else orange.
 * Reserves space for note name below bar.
**********************************************/
void initBarGraph(void) {

#define BGHEIGHT (120)  // height of sprite
#define BGWIDTH  (280)  // width of sprite (landscape display mode)
#define BGX (20)
#define BGY (60)
#define BGBARW (202)    // total width of bar
#define BGBARW2 (101)   // half width
#define BGCENTER (BGWIDTH/2)
#define BGBARX  (BGCENTER - BGBARW2 +1)   // -50 position
#define BGBARXE (BGCENTER + BGBARW2 -1)   // +50 position
#define BGBARXRED (BGCENTER - 50)   // left end of red bar
#define BGMARKH (10)  // pixel height of markings
#define BGBARY (26)   // upper position of bars
#define BGBARH (40)   // Bar height
#define BGNOTEY (BGBARY+BGBARH+BGMARKH+4) // vertical Position for NoteName

  void *pSprite = nullptr;

  // create sprite
  pSprite = barGraph.createSprite(BGWIDTH, BGHEIGHT);
  if(!pSprite)  ESP_LOGE(TAG, "Could not create barGraph!");
   // set color depth in advance to save allocation of 16bit mem!
  pSprite = barGraph.setColorDepth(8);  // sorry for recreating/reallocating
  if(!pSprite)  ESP_LOGE(TAG, "Could not set ColorDepth of barGraph!");
  barGraph.fillSprite(TFT_BLACK);

  // plot the markings. 0 is at center at BGCENTER, -50 at BGBARX, +50 at BGBARXE
  barGraph.setTextDatum(TC_DATUM);
  barGraph.setTextColor(TFT_WHITE);
  barGraph.setTextFont(2);
  barGraph.drawString(String("-50"), BGBARX, 0);
  barGraph.drawFastVLine(BGBARX, 15, BGMARKH, TFT_WHITE);
  barGraph.drawString(String("-25"), BGCENTER-52, 0);
  barGraph.drawFastVLine(BGCENTER-50, 15, BGMARKH, TFT_WHITE);
  barGraph.drawString(String("-10"), BGCENTER-22, 0);
  barGraph.drawFastVLine(BGCENTER-20, 15, BGMARKH, TFT_WHITE);
  barGraph.drawString(String("0"), BGCENTER, 0);
  barGraph.drawFastVLine(BGCENTER, 15, 2*BGMARKH+BGBARH, TFT_WHITE);
  barGraph.drawString(String("10"), BGCENTER+20, 0);
  barGraph.drawFastVLine(BGCENTER+20, 15, BGMARKH, TFT_WHITE);
  barGraph.drawString(String("25"), BGCENTER+50, 0);
  barGraph.drawFastVLine(BGCENTER+50, 15, BGMARKH, TFT_WHITE);
  barGraph.drawString(String("50"), BGBARXE, 0);
  barGraph.drawFastVLine(BGBARXE, 15, BGMARKH, TFT_WHITE);

  // draw red centered bar to show "invalid"
  barGraph.fillRect(BGBARXRED, BGBARY, BGBARW2, BGBARH, TFT_RED);

  barGraph.pushSprite(BGX, BGY);
  
} /* initBarGraph */

/******************************************
 * @brief: Update tuning bar and note name
 * @param[in] valid: are cent and cNote valid results? 
 *            valid == false will show red bar and no note name.
 * @param[in] bGreen: green bar when true, else orange
 * @param[in] cent: 1 cent is 1% to next note
 * @param[in] cNote: a note name not longer than 4 letters!
*******************************************/
void updateBarGraph(bool valid, bool bGreen, int16_t cent, char *cNote)	{
	static int16_t myCent=0;
  bool bReDrawEq = false;
  static bool gBGvalid = false;  // frequency reading is invalid at startup
  uint32_t uColor = TFT_GREEN;

  // is cent value valid?
  if(valid) {
    ESP_LOGD(TAG, "Note=%s, cent=%d", cNote, ent);
    if(!gBGvalid)  {
      // clear the red bar
      barGraph.fillRect(BGBARXRED, BGBARY, BGBARW2, BGBARH, TFT_BLACK);
      gBGvalid = true;
      bReDrawEq = true;
    }
  
    if((myCent != cent) || bReDrawEq) 
    {
      // redraw bar
      if(myCent<0)  barGraph.fillRect(BGBARX, BGBARY, BGBARW2, BGBARH, TFT_BLACK);
      else barGraph.fillRect(BGCENTER+1, BGBARY, BGBARW2, BGBARH, TFT_BLACK);
      if(cent>50) cent = 51;
      else if(cent<-50) cent = -51;
      myCent = cent;
      // clear note name
      barGraph.fillRect(BGCENTER-40, BGNOTEY, 80, 25, TFT_BLACK);
      // replace white VLine
      barGraph.drawFastVLine(BGCENTER, BGBARY, BGBARH, TFT_WHITE);
      // draw green or orange bar
      if(!bGreen) uColor = TFT_ORANGE;
      if(cent>0) barGraph.fillRect(BGCENTER+1, BGBARY, cent*2, BGBARH, uColor);
      else if(cent<0) {
        cent = -2*cent; // make it positive and double
        barGraph.fillRect(BGCENTER-cent-1, BGBARY, cent, BGBARH, uColor);
      }
      // OK, when cent==0

      barGraph.setTextDatum(TC_DATUM);
      barGraph.setTextColor(TFT_YELLOW);
      barGraph.setTextFont(4);
      barGraph.drawString(String(cNote), BGCENTER, BGNOTEY);
    } // redraw

  } // valid
  else  
  { // no valid results to display
    
  ESP_LOGD(TAG, "Note not valid");
    if(gBGvalid)  
    { 
      gBGvalid = false;
      // clear all green bar
      barGraph.fillRect(BGBARX, BGBARY, BGBARW, BGBARH, TFT_BLACK);
      // display red bar
      barGraph.fillRect(BGBARXRED, BGBARY, BGBARW2, BGBARH, TFT_RED);
      // clear note name
      barGraph.fillRect(BGCENTER-40, BGNOTEY, 80, 25, TFT_BLACK);
    }
    // or still not valid
  }

  barGraph.pushSprite(BGX, BGY);

} /* updateBarGraph */


/**********************************************************
 * @brief: Main routine of this app.
 * Read from ADC channel 0 into gBuf.
 * Get frequency from ADC_DataAnalysis.
 * Gets notename and cent value from AFrequencies.
 * Calls updateBarGraph with results.
 * 
 * @return is always 0
 * @note one valid evaluation take approx. 87ms
 * @uses gsAD ADC structure ,
 *      i2s_start, i2s_stop from driver/i2s.h
 *      and my ADC_Sampling, which is simply i2s_read
***********************************************************/
int getFreqNoteName() {
  size_t retSamples;
  int retval;
  char noteName[5];
  int cent;
  bool bGreen=true;   // usually we display a green bar, but when quality is bad, bar will be drawn in orange
  bool bValid = true; // noteName valid
  float freqRelDiff;
  //uint32_t udt_a, udt_e;  // measure timing
  uint16_t max, min, mean;
  uint32_t newLen=0;  // new usable data buffer length after oversampling
  uint16_t *pbn, *pbo;

  if(!gsAD.data) goto INVALID;

    //udt_a = esp_cpu_get_ccount();
  i2s_start(I2S_NUM_0);
  retSamples = ADC_Sampling(gsAD.data, gsAD.d_len);
    /*udt_e = esp_cpu_get_ccount(); 
    if(udt_e > udt_a)   udt_e -= udt_a;
    else udt_e += (0xFFFFFFFF - udt_a) +1;
    Serial.printf("TIMING: ADC_Sampling %d [µs]\n", udt_e/240);
    */
  i2s_stop(I2S_NUM_0);
  

  pbn = pbo = gsAD.data;
  
  for(int i=0; i<gsAD.d_len;)  {
    /* something wrong, mayby compiler optimization?
    mean = (*pbo + *(++pbo))/2;
    *(++pbn) = *pbn = mean;
    pbn++;
    pbo++;
    */
    /* mean for both. Slowly, but reliable:
    mean = (gsAD.data[i+1] + gsAD.data[i])/2;
    gsAD.data[i+1] = gsAD.data[i] = mean;
    i+=2;
    */
    // use higher word first, so switch readings
    mean = gsAD.data[i+1];
    gsAD.data[i+1] = gsAD.data[i];
    gsAD.data[i] = mean;
    i += 2;
  }
      /*
      // debug: printout data
      Serial.println("ADC buffer (300 items)");
      for(int i=0; i<300; i++) Serial.printf("%u\n", gsAD.data[i]);
      */
  
  if(retSamples != gsAD.d_len)  goto INVALID;

  // prepare data analysis
    //udt_a = esp_cpu_get_ccount();
  peak_mean(&gsAD, &max, &min, &mean);
    /*udt_e = esp_cpu_get_ccount(); 
    if(udt_e > udt_a)   udt_e -= udt_a;
    else udt_e += (0xFFFFFFFF - udt_a) +1;
    Serial.printf("TIMING: peak_mean %d [µs]\n", udt_e/240);
    //Serial.printf("signal's max=%u min=%u mean=%u\n", max, min, mean);
    */
  gsAD.d_max = max;
  gsAD.d_min = min;
  gsAD.d_mean = mean;

  // get frequency and periode
    //udt_a = esp_cpu_get_ccount();
  retval = calcFreqAnalog(&gsAD);
    /*udt_e = esp_cpu_get_ccount(); 
    if(udt_e > udt_a)   udt_e -= udt_a;
    else udt_e += (0xFFFFFFFF - udt_a) +1;
    Serial.printf("TIMING: calcFreqAnalog %d [µs]\n", udt_e/240);
    */
  if(retval<0) {
    ESP_LOGD(TAG, "calcFreqAnalog returned code %d\n", retval);
    goto INVALID;
  }
  ESP_LOGD(TAG, "Classic F=%7.1f[Hz](NC=%u) mean periode=%7.1f[us](Fp=%7.1f) N=%u  quality=stdev=%7.1f[us]\n", 
      gsAD.d_freqClassic, gsAD.d_numCP, gsAD.d_periode*ONEM, 1.0f/gsAD.d_periode, gsAD.d_numPeriodes, gsAD.d_quality*ONEM);
  
  // in case off freq==0
  if((gsAD.d_freqClassic < FLT_MIN) || (gsAD.d_periode > gsAD.d_len))  goto INVALID;

  // if quality is worse, plot orange bar
  if(gsAD.d_quality/gsAD.d_periode > MINFREQQUALITY) bGreen = false;

  // if d_freqClassic and (1.0f/gsAD.d_periode) differ too much use mean value only if d_numPeriodes is large enough!
  freqRelDiff = fabs(gsAD.d_freqClassic - (1.0f/gsAD.d_periode))/gsAD.d_freqClassic;
  ESP_LOGD(TAG, "Frequency relative difference = %g", freqRelDiff);
  if(freqRelDiff > MINFREQDIFF) {
    bGreen = false;  
    // alternative: use mean value:
    // gsAD.d_freqClassic = (gsAD.d_freqClassic + (1.0f/gsAD.d_periode))/2.0f;
  }

  // Tuning for 440Hz signals. 100cent = factor TWELFTH_SQ2
  /* 
  if(TUNINGCENT < 0) gsAD.d_freqClassic /= (((TWELFTH_SQ2-1.0f)*(-TUNINGCENT)/100) + 1.0f);
  if(TUNINGCENT > 0) gsAD.d_freqClassic *= (((TWELFTH_SQ2-1.0f)*(TUNINGCENT)/100) + 1.0f);
  Tuned version :   
  */
  if(TUNINGCENT < 0) gsAD.d_freqClassic /= TUNINGFACTOR;
  if(TUNINGCENT > 0) gsAD.d_freqClassic *= TUNINGFACTOR;

  ESP_LOGD(TAG, "Freq after tuning=%7.1f[Hz]", gsAD.d_freqClassic);

  // find note name and cent difference
    //udt_a = esp_cpu_get_ccount(); 
  retval = findNearestNoteDiff(gsAD.d_freqClassic, noteName, &cent);
    /*
    udt_e = esp_cpu_get_ccount(); 
    if(udt_e > udt_a)   udt_e -= udt_a;
    else udt_e += (0xFFFFFFFF - udt_a) +1;
    Serial.printf("TIMING: findNearestNoteDiff %d [µs]\n", udt_e/240);
    */
  if(retval < -50) bValid = false;

UPDATEGRAPH:

  // update bar grap / sprite
    //udt_a = esp_cpu_get_ccount(); 
  updateBarGraph(bValid, bGreen, cent, noteName);
    /*udt_e = esp_cpu_get_ccount(); 
    if(udt_e > udt_a)   udt_e -= udt_a;
    else udt_e += (0xFFFFFFFF - udt_a) +1;
    Serial.printf("TIMING: updateBarGraph %d [µs]\n", udt_e/240);
    */
  return 0;

INVALID:
  bValid = false;
  goto UPDATEGRAPH;

} /* getFreqNoteName */


void setup(void) 
{
  tft.init();
  tft.setRotation(3);   // using landscape
  Serial.begin(115200); // For debug
  delay(100);
  //Serial.println("Booting...");
  
  //showHeapInfo();

  // setup ADC buffer
  gsAD.data = (uint16_t *)heap_caps_malloc(BUFF_SIZE*sizeof(uint16_t), MALLOC_CAP_DMA);
  if(!gsAD.data)  ESP_LOGE(TAG,"Could not allocate ADC buffer!");
  gsAD.d_len = BUFF_SIZE; 
  gsAD.d_sFreq = SAMPLERATE;  // [Hz]
  gsAD.d_deltaTime = 1.0f/SAMPLERATE;   // [s] !!

  tft.fillScreen(TFT_NAVY);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_YELLOW);
  tft.setTextFont(4);
  tft.drawString(String("Frequency Tuner"), TFT_HEIGHT/2, 15);

  initBarGraph();

  // setup I2S for ADC-DMA mode
  configure_i2s(SAMPLERATE, ADC_CHANNEL);    // call own i2s.cpp
  
  // getFreqNoteName();     // one run just for testing

}	/* setup */

void loop() 
{
  static int d = 0;
  static uint32_t updateTime = 0, updT = 0;  
  const uint32_t uTime = 5000;
  
  
    //if(millis() > updT) {
  	  getFreqNoteName();
    //  updT = millis() + uTime;
    //}
  } 	/* loop */

