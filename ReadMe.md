# Frequency Tuner

![Small 3" display with concert pitch 'a1'](/FreqTunerDisplay_small.jpg)

## What is it?
This is a **musical** frequency tuning software for ESP32.   
It displays the note name (german notation C to c6) and the deviation in cent    
(1 cent is 1% distance to next seminote) on a 3" screen.

I uses this software in order to tune my analog synth.

> [!NOTE]
> Do not confuse it with [FREQtune](https://github.com/Kirellos-Safwat/FREQtune) !

## My libs for Win32, ESP32, ...(?) and a main file for ESP32 

The two attached libraries may be used in your own tuning device.   
ADC_DataAnalysis amd AFrequencies should work on any hardware with minimum modifications.   
> [!IMPORTANT]
> Keep in mind, that AFrequencies uses german note names.

My ADC_sampling coding is really old school. Espressif has a much better API with 5.4.

> [!NOTE]
> For testing purpose you may use ADC_Sim even on your home computer.

All standard c (no c++, as far as I understand from gcc) on level 11 (sorry in case of error).

## Installation

I used VS-Code, gcc 13.1.0 under Platformio 6.1.18 with espressif32 6.10.0 under the Arduino framework.   
The hardware was an ESP32 devkit (V4) and a noname 3" screen with ILI9341 (did not use touch).   
You have to include Bodmer's nice library **TFT_eSPI** @ 2.5.43 [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) (thank you very much!)   
or change tft.init, initBarGraph, and updateBarGraph in freq_tune.cpp

Setup and loop and the main routine are within src/freq_tune.cpp.

## Modifications

Change platformio.ini when you use other displays and/or other pins. Do not use "User_Setup.h" in TFT_eSPI.

There are several #defines, that you may change ad libitum.

> [!NOTE]
> As this software is provided as it is, so I will not help you with modifications.

Good luck!

## Other similar programs

In github I found nothing comparable for ESP32 use.  
For desktop there are (today):  
[FMIT](https://github.com/gillesdegottex/fmit)  
[SparTuner](https://github.com/imsparsh/SparTuner/blob/master/README.md) : python using pyaudio real stream input  
[lingot](https://github.com/ibancg/lingot)  
[FastTune](https://github.com/FastTune/FastTune)  


## Credits and license

- This work uses Bodmer's TFT_eSPI with version 2.5.43   
- It took advantage of the useful PlatformIO 6.1.18 extension of VS-Code, see   
[VSCode](https://code.visualstudio.com/download) ,    
[PlatformIO extension](https://docs.platformio.org/en/latest/integration/ide/vscode.html)       
- Licensed under GPL v2 [GPL V3](https://www.gnu.org/licenses/gpl-3.0.html.en)
> All links without any accountability! Use it on your own authority.
