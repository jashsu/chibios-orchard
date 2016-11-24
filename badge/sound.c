/* sound.c - monotonic music for the badge */
#include "ch.h"
#include "hal.h"
#include "tpm.h"
#include "notes.h"
#include "sound.h"

#define note4  400 /* mS per quarter note @ 140bpm */
#define note16 100 /* 16th note */
#define note32 50  /* 32nd note */

static const PWM_NOTE soundGalaga[] = {
  { NOTE_A5, note32 * 6 },
  { NOTE_D5, note32 },
  { NOTE_E5, note32 * 6 },
  { NOTE_G5, note32 },
  { NOTE_FS5, note32 * 6 },
  { NOTE_D5, note32 },
  { NOTE_E5, note32 * 6 },
  { NOTE_B6, note32 },
  { NOTE_A6, note32 * 6 },
  { NOTE_D5, note32 },
  { NOTE_E5, note32 * 6 },
  { NOTE_G5, note32 },
  { NOTE_FS5, note32 * 6 },
  { NOTE_D5, note32 },
  { NOTE_A6, note32 * 6 },
  { NOTE_CS6, note32 },
  { NOTE_D6, note32 * 6 },
  { NOTE_C6, note32 },
  { NOTE_AS5, note32 * 6 },
  { NOTE_A5, note32 },
  { NOTE_G5, note32 * 6 },
  { NOTE_F5, note32 },
  { NOTE_E5, note32 * 6 },
  { NOTE_C5, note32 },
  { NOTE_C6, note32 * 6 },
  { NOTE_D6, note32 },
  { NOTE_C6, note32 * 6 },
  { NOTE_A6, note32 * 2 },
  { NOTE_B6, note32 * 2 },
  { NOTE_G5, note32 * 2 },
  { NOTE_E5, note32 * 2 },
  { NOTE_A6, note32 * 2 },
  { NOTE_FS5, note32 * 2 },
  { NOTE_E5, note32 * 2 },
  { 0, PWM_DURATION_END }
};

#ifdef notdef
static const PWM_NOTE soundStart[] = {
  { PWM_NOTE_OFF, note4 * 3 },
  { NOTE_FS3, note32 * 3 },
  { PWM_NOTE_PAUSE, note32 * 2 },
  { NOTE_CS5, note32 * 2 },
  { NOTE_D5, note32 * 2 },
  { NOTE_A4, note32 * 2 },
  { NOTE_FS5, note32 * 5 },
  { 0, PWM_DURATION_END }
};
#endif

static const PWM_NOTE soundSadPanda[] = {
  /* dissonant failure sound */
  { NOTE_GS3, note16 },
  { NOTE_F3, note16 },
  { NOTE_E3, note4 },
  { 0, PWM_DURATION_END }
};
    
void playTone(uint16_t freq, uint16_t duration) {
  pwmToneStart(freq);
  chThdSleepMilliseconds (duration);
  pwmToneStop();
}

void playStartupSong(void) {
  /* the galaga level up sound */
  pwmThreadPlay (soundGalaga);
}

void playHardFail(void) {
  /* the galaga level up sound */
  pwmThreadPlay (soundSadPanda);
}

