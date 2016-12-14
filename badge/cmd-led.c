/*
  cmd-led.c

  change the state of the led's
  J. Adams <jna@retina.net>
*/

#include "ch.h"
#include "hal.h"
#include "led.h"
#include "shell.h"
#include "chprintf.h"

#include <strings.h>
#include <string.h>
#include <stdlib.h>

#include "orchard.h"
#include "orchard-shell.h"

#include "userconfig.h"

extern struct FXENTRY fxlist[];

static void led_stop(BaseSequentialStream *chp) {
  effectsStop();
  chprintf(chp, "Off.\r\n");
}

static void led_run(BaseSequentialStream *chp, int argc, char *argv[]) {

  uint8_t pattern;
  userconfig *config = getConfig();
    
  if (argc != 2) {
    chprintf(chp, "No pattern specified\r\n");
    return;
  }

  pattern = strtoul(argv[1], NULL, 0);
  if ((pattern < 1) || (pattern > LED_PATTERN_COUNT)) {
    chprintf(chp, "Invaild pattern #!\r\n");
    return;
  }

  config->led_pattern = pattern-1;
  ledResetPattern();
  
  chprintf(chp, "pattern changed to %s...\r\n", fxlist[pattern-1].name);
  configSave(config);
}

static void led_dim(BaseSequentialStream *chp, int argc, char *argv[]) {

  int8_t level;
  userconfig *config = getConfig();

  if (argc != 2) {
    chprintf(chp, "level?\r\n");
    return;
  }

  level = strtoul(argv[1], NULL, 0);

  if ((level < 0) || (level > 7)) {
    chprintf(chp, "Invaild level. Must be 0 to 7.\r\n");
    return;
  }

  ledSetBrightness(level);
  chprintf(chp, "level now %d.\r\n", level);

  config->led_shift = level;
  configSave(config);

}


static void led_list(BaseSequentialStream *chp) {
  chprintf(chp, "\r\nAvailable LED Patterns\r\n");

  for (int i=0; i < LED_PATTERN_COUNT; i++) {
    chprintf(chp, "%2d) %s\r\n", i+1, fxlist[i].name);
  }
}

static void cmd_led(BaseSequentialStream *chp, int argc, char *argv[]) {

  if (argc == 0) {
    chprintf(chp, "led commands:\r\n");
    chprintf(chp, "   list             list animations available\r\n");
    chprintf(chp, "   dim n            dimmer level (0-7) 0=brightest\r\n");
    chprintf(chp, "   run n            run pattern #n\r\n");
    chprintf(chp, "   stop             stop and blank LEDs\r\n");
    return;
  }

  if (!strcasecmp(argv[0], "list")) {
    led_list(chp);
    return;
  }

  if (!strcasecmp(argv[0], "dim")) {
    led_dim(chp, argc, argv);
    return;
  }

  if (!strcasecmp(argv[0], "run"))
    led_run(chp, argc, argv);
  else
    if (!strcasecmp(argv[0], "stop"))
      led_stop(chp);
    else
      chprintf(chp, "Unrecognized LED command\r\n");
}

orchard_command("led", cmd_led);