/************************************************************************
 @brief Audio frequency analysis for musical instruments  
 @file AFrequencies.cpp
 @author Juergen Boehm
 @date 2025 April 13
 @include AFrequencies.h
 @note Compiler: GCC under Win32 resp. Espressif
 @note Only using float and default int
 @note 52+28+4 bytes of global memory (DRAM).
 @note Implementation is used with small modifications for Arduino/ESP32

  Copyright (C) <2025>  <Juergen Boehm>
*************************************************************************/
#ifdef _WIN32
    #include <stdio.h>
    #include <stdlib.h>
#elif defined ARDUINO_ARCH_ESP32   || defined ESP32
    #include <Arduino.h>
#endif

#include <stddef.h>
#include <stdint-gcc.h>
#include <math.h>
#include <string.h> 
#include <float.h>

#include "AFrequencies.h"


#ifdef ARDUINO_ARCH_ESP32
const char TAG[] = "FreqTune";
#endif

// *** Globals ***

// I prefer the german notation with h and b. English:                             "bb"   "b"  
char noteNames[13][4] = {"c", "cis", "d", "dis", "e", "f", "fis", "g", "gis", "a", "b", "h", "c"};

/* note's ranges start up from
	C	65.4063913251401 
	c   130.812782650287
	c1	261.625565300588
	c2	523.251130601197
	c3	1046.5022612024
	c4	2093.00452240479
	c5	4186.009045
*/
float noteCstart[] = {65.4063913251401, 130.812782650287, 261.625565300588, 523.251130601197, 1046.5022612024, 2093.00452240479, 4186.009045};
int noteCstartCount = sizeof(noteCstart)/sizeof(float);

/* *** public functions *** */

/**************************************
    @brief  Finds the name of nearest musical notes between deep C and high c5
    @author Juergen Boehm
    @date 2025 April 12
    @note 
    @param[in] freq:    audio frequency in Hz (1/s)
    @param[in] noteName: provide space for the note's name and one digit for the range, thus >=5
    @param[out] noteName: name of the note with no error return. e.g. "fis3" for approx. 1480 Hz. 
    @return <0 for errors and >=0 for range, starting at deep C (65.4Hz)
          Range 0 will just give c, cis,...
          Range 1 will give c0, cis0,... according to list above and common style
***************************************/
int findNearestNote(float freq, char *noteName) {
    int retVal;
    int noteRange;

    retVal = findNoteRange(freq);
    if(retVal<0) return retVal;
    noteRange = retVal;

    retVal = findNoteInRange(freq, noteRange, noteName);
    if(retVal<-50) return retVal;
    else return noteRange;

}   /* findNearestNote */

/**************************************
    @brief Same as findNearestNote, but also gives difference from noteName in cent (1 cent = 1/100 of a semitone) 
    @param[out] *diffCent
    @return <=-9999 for errors or noteRange as with findNearestNote
***************************************/
int findNearestNoteDiff(float freq, char *noteName, int *diffCent) {
  int retVal;
  int noteRange;

  retVal = findNoteRange(freq);
  if(retVal<0) return retVal-9999;
  noteRange = retVal;

  retVal = findNoteInRange(freq, noteRange, noteName);
  if(retVal < -50) return retVal;

  *diffCent = retVal;

  return noteRange;

}   /* findNearestNoteDiff */



/*** private functions ***/

/********************************************
 * Finds range of frequency
 * returns -1 for frequencies above c5
 * returns -2 for frequencies below C 
 * returns 0 .. 6 as index into noteCstart
*********************************************/
int findNoteRange(float freq) {
  int retVal = -1;
  
  // allow 50 cent above c6:
  if(freq > 2.0f * noteCstart[noteCstartCount-1] * HALFNOTEFACTOR)    return -1;
  if(freq < noteCstart[0]) return -2;

  if(freq < noteCstart[1])    {
      retVal = 0;
  }
  else if(freq < noteCstart[2])    {
      retVal = 1;
  }
  else if(freq < noteCstart[3])    {
      retVal = 2;
  }
  else if(freq < noteCstart[4])    {
      retVal = 3;
  }
  else if(freq < noteCstart[5])    {
      retVal = 4;
  }
  else if(freq < noteCstart[6])    {
      retVal = 5;
  }
  else retVal = 6;  // used up to c6
  
  return retVal;
} /* findNoteRange */

/********************************************
 * Finds note within a given range 0..6.
 * returns -9999 for errors
 * returns -50 .. 50 for differnece to note in cent
 * 2025, April 21: Tuning correction introduced.
*********************************************/
int findNoteInRange(float freq, int range, char *noteName)  {

    float dist[13], noteFreq, freqMinDist;
#ifdef _WIN32
    float minDist = FLT_MAX; 
#elif ARDUINO_ARCH_ESP32
    float minDist = FLT_MAX;
#endif
    int i, idxMin, iCent;
    char num[3];

    if(range<0 || range>6) return -9999;


    noteFreq = noteCstart[range];
    // generate frequency distances within range and find index of minimum
    for(i=0; i<13; i++) {
        dist[i] = freq - noteFreq;
        if(fabs(dist[i]) < minDist) {
            minDist = dist[i];
            idxMin = i;
            freqMinDist = noteFreq;
        }
        noteFreq *= TWELFTH_SQ2;
    }

    strcpy(noteName, noteNames[idxMin]);

    // pay attention with upper limit in range:
    if(idxMin == 12)  { // next higher c is nearest!, e.g.c2, when in c1 range
        itoa(range, num, 10);
        strncat(noteName, num, 1);
    }
    else if(range>0) {
        itoa(range-1, num, 10);
        strncat(noteName, num, 1);
    }

    // calculate cent from minDist
    noteFreq = (TWELFTH_SQ2 - 1.0f) * freqMinDist / 100.0f;
        //printf("+++ Debug: freqMinDist=%g minDist=%g  freq=%g oneCent=%g idxMin=%d\n", freqMinDist, minDist, freq, noteFreq, idxMin);
    if(fabs(noteFreq)<0.03f)    // minimum cent with C is 0.0367Hz
        return 0;
    else {
        if(minDist < 0.0f)  iCent = (int)(minDist/noteFreq - 0.5f);
        else iCent = (int)(minDist/noteFreq + 0.5f);
        return iCent;
    }
    
}   /* findNoteInRange */






