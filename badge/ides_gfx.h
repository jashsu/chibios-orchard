#ifndef __IDES_GFX_H__
#define __IDEX_GFX_H__
#define ALERT_DELAY 1500               // how long alerts (screen_alert) stay on the screen.

/* ides_gfx.h
 *
 * Shared Graphics Routines
 * J. Adams <1/2017>
 */

extern const GWidgetStyle RedButtonStyle;

/* Graphics */
extern void drawProgressBar(coord_t x, coord_t y, coord_t width, coord_t height, int32_t maxval, int32_t currentval, uint8_t use_leds, uint8_t reverse);
extern int putImageFile(char *name, int16_t x, int16_t y);
extern void blinkText (coord_t x, coord_t y,coord_t cx, coord_t cy, char *text, font_t font, color_t color, justify_t justify, uint8_t times, int16_t delay);
extern char *getAvatarImage(int ptype, char *imgtype, uint8_t frameno, uint8_t reverse);
extern void screen_alert_draw(uint8_t clear, char *msg);

// draws an alert if we have airplane mode enabled
extern uint8_t airplane_mode_check(void);
#endif
