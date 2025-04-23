/***************************************************************************
    @brief  Finds the name of musical notes between deep C and high c6
            using german notation. c6=8372.018 Hz one octave beyond a 88 keys piano.
            English Notation "C9" for my c6 as far as I know.
    @file AFrequencies.cpp
    @author Juergen Boehm
    @date 2025 April 12
    @note 
    @param[in] freq:    audio frequency in Hz (1/s)
    @param[in] noteName: provide space for the note's name and one digit for the range, thus >=5
    @param[out] noteName: name of the note with no error return. e.g. "fis3" for approx. 1480 Hz. 
        Range 0 will just give c, cis,... (to save ram space)
        Range 1 will give c0, cis0,... according to list above and common style
    @return <0 for errors and >=0 for range, starting at deep C (65.4Hz, engl. C2)
****************************************************************************/
#ifndef FINDNOTES
#define FINDNOTES

#define AUDIO_FREQ
#define TONE_A1 (440.0f)
#define TONE_C1 (261.625565300588f)
// 12th root of 2 is one half-tone (second, semitone) :
#define TWELFTH_SQ2 (1.0594630943593f)
#define HALFNOTEFACTOR (1.029302236643492f)

// Finds the name of musical notes between deep C and high c6
int findNearestNote(float freq, char *noteName);
// Same, but also gives difference from noteName in cent (1 cent = 1/100 of a semitone)
int findNearestNoteDiff(float freq, char *noteName, int *diffCent);

// internal use:
int findNoteRange(float freq);
int findNoteInRange(float freq, int range, char *noteName);

#endif //FINDNOTES
