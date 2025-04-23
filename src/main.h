/**************************
 * main.h
***************************/

#define SAMPLERATE (30000)  // sample rate in Hz as suggested in ADC_DataAnalysis.h
//#define DEBUG_BUF           // DEBUG_BUF : stand alone routine that gives raw ADC data to console

// Width and height of TFT screen // redundand, may be read from TFT object
#define WIDTH  (320)    
#define HEIGHT (240)
#define HEIGHT1 (HEIGHT - 1)

#define BUFF_SIZE (2000)    // as suggested in ADC_DataAnalysis.h

#define ADC_CHANNEL   (0)  // 0 == GPIO36
#define ONEM (1000000)      // 1 Mio

// main routine 
int getFreqNoteName(void);
