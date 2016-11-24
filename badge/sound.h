#ifndef __SOUND_H__
#define __SOUND_H__

extern void playTone(uint16_t freq, uint16_t duration);
extern void playStartupSong(void);
extern void playHardFail(void);
extern void playAttacked(void);
extern void playVictory(void);
extern void playDefeat(void);
extern void playDodge(void);
extern void playHit(void);

#endif /* __SOUND_H__ */
