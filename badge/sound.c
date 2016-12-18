/* sound.c - monotonic music for the badge */
#include "ch.h"
#include "hal.h"
#include "tpm.h"
#include "notes.h"
#include "sound.h"

#define note4  50 /*400*/ /* mS per quarter note @ 140bpm */
#define note16 12 /*100*/ /* 16th note */
#define note32 5 /*50*/  /* 32nd note */

static const PWM_NOTE soundGalaga0[] = {
  { 67, 54 },
  { 69, 36 },
  { 72, 18 },
  { 71, 54 },
  { 67, 54 },
  { 72, 54 },
  { 74, 36 },
  { 77, 18 },
  { 76, 54 },
  { 74, 54 },
  { 75, 54 },
  { 74, 36 },
  { 72, 18 },
  { 70, 54 },
  { 75, 54 },
  { 82, 54 },
  { 79, 36 },
  { 75, 18 },
  { 74, 54 },
  { 79, 36 },
  { 76, 18 },
  { 0, PWM_DURATION_END }
};

static const PWM_NOTE soundGalaga1[] = {
  { 76, 36 },
  { 67, 18 },
  { 74, 36 },
  { 69, 18 },
  { 72, 36 },
  { 67, 18 },
  { 69, 36 },
  { 72, 18 },
  { 76, 36 },
  { 67, 18 },
  { 74, 36 },
  { 69, 18 },
  { 72, 36 },
  { 67, 18 },
  { 72, 36 },
  { 79, 18 },
  { 80, 36 },
  { 79, 18 },
  { 77, 36 },
  { 75, 18 },
  { 74, 36 },
  { 72, 18 },
  { 70, 36 },
  { 74, 18 },
  { 75, 36 },
  { 77, 18 },
  { 75, 36 },
  { 72, 18 },
  { 77, 18 },
  { 74, 18 },
  { 69, 18 },
  { 74, 18 },
  { 71, 18 },
  { 69, 18 },
  { 0, PWM_DURATION_END }
};

static const PWM_NOTE soundGalaga2[] = {
  { 67, 36 },
  { 72, 18 },
  { 74, 36 },
  { 77, 18 },
  { 76, 36 },
  { 72, 18 },
  { 74, 36 },
  { 81, 18 },
  { 79, 36 },
  { 72, 18 },
  { 74, 36 },
  { 77, 18 },
  { 76, 36 },
  { 72, 18 },
  { 79, 36 },
  { 83, 18 },
  { 84, 36 },
  { 82, 18 },
  { 80, 36 },
  { 79, 18 },
  { 77, 36 },
  { 75, 18 },
  { 74, 36 },
  { 70, 18 },
  { 82, 36 },
  { 84, 18 },
  { 82, 36 },
  { 79, 18 },
  { 81, 18 },
  { 77, 18 },
  { 74, 18 },
  { 79, 18 },
  { 76, 18 },
  { 74, 18 },
  { 0, PWM_DURATION_END }
};

static const PWM_NOTE soundMsPacman0[] = {
  { PWM_NOTE_OFF, 36 },
  { 72, 11 },
  { PWM_NOTE_OFF, 1 },
  { 74, 11 },
  { PWM_NOTE_OFF, 1 },
  { 76, 11 },
  { PWM_NOTE_OFF, 1 },
  { 77, 35 },
  { PWM_NOTE_OFF, 1 },
  { 81, 35 },
  { PWM_NOTE_OFF, 1 },
  { 79, 35 },
  { PWM_NOTE_OFF, 1 },
  { 82, 35 },
  { PWM_NOTE_OFF, 1 },
  { 81, 17 },
  { PWM_NOTE_OFF, 1 },
  { 82, 17 },
  { PWM_NOTE_OFF, 1 },
  { 84, 17 },
  { PWM_NOTE_OFF, 1 },
  { 81, 17 },
  { PWM_NOTE_OFF, 1 },
  { 79, 35 },
  { PWM_NOTE_OFF, 1 },
  { 82, 35 },
  { PWM_NOTE_OFF, 1 },
  { 81, 17 },
  { PWM_NOTE_OFF, 1 },
  { 82, 17 },
  { PWM_NOTE_OFF, 1 },
  { 84, 17 },
  { PWM_NOTE_OFF, 1 },
  { 81, 17 },
  { PWM_NOTE_OFF, 1 },
  { 82, 17 },
  { PWM_NOTE_OFF, 1 },
  { 84, 17 },
  { PWM_NOTE_OFF, 1 },
  { 86, 17 },
  { PWM_NOTE_OFF, 1 },
  { 88, 17 },
  { PWM_NOTE_OFF, 1 },
  { 89, 35 },
  { PWM_NOTE_OFF, 1 },
  { 88, 35 },
  { PWM_NOTE_OFF, 1 },
  { 89, 35 },
  { 0, PWM_DURATION_END }
};

static const PWM_NOTE soundMsPacman1[] = {
  { PWM_NOTE_OFF, 72 },
  { 41, 35 },
  { PWM_NOTE_OFF, 37 },
  { 36, 35 },
  { PWM_NOTE_OFF, 37 },
  { 41, 35 },
  { PWM_NOTE_OFF, 37 },
  { 43, 17 },
  { PWM_NOTE_OFF, 1 },
  { 45, 17 },
  { PWM_NOTE_OFF, 1 },
  { 46, 17 },
  { PWM_NOTE_OFF, 1 },
  { 43, 17 },
  { PWM_NOTE_OFF, 1 },
  { 45, 17 },
  { PWM_NOTE_OFF, 1 },
  { 43, 17 },
  { PWM_NOTE_OFF, 1 },
  { 41, 17 },
  { PWM_NOTE_OFF, 1 },
  { 45, 17 },
  { PWM_NOTE_OFF, 1 },
  { 43, 17 },
  { PWM_NOTE_OFF, 1 },
  { 41, 17 },
  { PWM_NOTE_OFF, 1 },
  { 40, 17 },
  { PWM_NOTE_OFF, 1 },
  { 43, 17 },
  { PWM_NOTE_OFF, 1 },
  { 41, 35 },
  { PWM_NOTE_OFF, 1 },
  { 36, 35 },
  { PWM_NOTE_OFF, 1 },
  { 41, 35 },
  { 0, PWM_DURATION_END }
};

#ifdef notdef
static const PWM_NOTE soundStart[] = {
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

static const PWM_NOTE soundAttacked[] = {
  { NOTE_A6, note16 },
  { NOTE_A7, note16 },
  { PWM_NOTE_PAUSE, note16 * 2 },
  { NOTE_A6, note16 },
  { NOTE_A7, note16 },
  { PWM_NOTE_PAUSE, note16 * 2 },
  { NOTE_A6, note16 },
  { NOTE_A7, note16 },
  { PWM_NOTE_PAUSE, note16 * 2 },
  { NOTE_A6, note16 },
  { NOTE_A7, note16 },
  { PWM_NOTE_PAUSE, note16 * 2 },
  { NOTE_A6, note16 },
  { NOTE_A7, note16 },
  { PWM_NOTE_PAUSE, note16 * 2 },
  { NOTE_A6, note16 },
  { NOTE_A7, note16 },
  { PWM_NOTE_PAUSE, note16 * 2 },
  { NOTE_A6, note16 },
  { NOTE_A7, note16 },  
  { 0, PWM_DURATION_END }
};

static const PWM_NOTE soundVictory[] = {
  { NOTE_D4, note16 },
  { NOTE_FS4, note16 },
  { NOTE_A4, note16 },
  { NOTE_D5, note16 },
  { PWM_NOTE_PAUSE, note16 * 2 },
  { NOTE_D4, note16 * 2 },
  { NOTE_D5, note16 * 3 },
  { 0, PWM_DURATION_END }
};

static const PWM_NOTE soundDodge[] = {
  { 0, PWM_DURATION_END }
};

static const PWM_NOTE soundDefeat[] = {
  { 0, PWM_DURATION_END }
};

void playTone(uint16_t freq, uint16_t duration) {
  pwmToneStart(freq);
  chThdSleepMilliseconds (duration);
  pwmToneStop();
}

void playStartupSong(void) {
  /* the galaga game start sound */
  pwmChanThreadPlay (soundGalaga0, soundGalaga1, soundGalaga2);
}

void playMsPacman(void) {
  pwmChanThreadPlay (soundMsPacman0, soundMsPacman1, NULL);
}

void playHardFail(void) {
  /* sad panda tone */
  pwmThreadPlay (soundSadPanda);
}

void playAttacked(void) {
  /* played when you're getting attacked by another player */
  pwmThreadPlay (soundAttacked);
}

void playVictory(void) {
  /* played when you win */
  pwmThreadPlay (soundVictory);
}

void playDefeat(void) {
  /* played when you lose */
  pwmThreadPlay (soundDefeat);
}

void playDodge(void) {
  /* played when you dodge an attack */
  pwmThreadPlay (soundDodge);
}

void playHit(void) {
  /* played when you're hit */
  
}
