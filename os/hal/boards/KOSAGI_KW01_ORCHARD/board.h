/*
    ChibiOS - Copyright (C) 2006..2015 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#ifndef _BOARD_H_
#define _BOARD_H_

/*
 * Setup for Kosagi Orchard board.
 */

#ifdef notdef
/*
 * Board identifier.
 */
#define BOARD_KOSAGI_ORCHARD
#define BOARD_NAME                  "Kosagi Orchard"

/* External 8 MHz crystal with PLL for 48 MHz core/system clock. */
#define KINETIS_SYSCLK_FREQUENCY    48000000UL 
#define KINETIS_MCG_MODE            KINETIS_MCG_MODE_PEE
#endif

/*
 * Board identifier.
 */
#define BOARD_KOSAGI_ORCHARD
#define BOARD_NAME                  "Freescale/NXP FRDM-KW019032 Development"

/* Freescale/NXP Freedom KW019032 board uses FEI mode. */

#define KINETIS_SYSCLK_FREQUENCY    20971520UL
#define KINETIS_MCG_MODE            KINETIS_MCG_MODE_FEI


/*
 * Check for board revision specification.
 */
#define ORCHARD_REV_EVT1            1
#define ORCHARD_REV_EVT1B           2

#if !defined(ORCHARD_BOARD_REV)
#error "Must define an orchard board revision"
#endif

#if !defined(_FROM_ASM_)
#ifdef __cplusplus
extern "C" {
#endif

  extern uint32_t __storage_start__[];
  extern uint32_t __storage_size__[];
  extern uint32_t __storage_end__[];

  void boardInit(void);
#ifdef __cplusplus
}
#endif
#endif /* _FROM_ASM_ */

#endif /* _BOARD_H_ */
