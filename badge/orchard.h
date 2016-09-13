#ifndef __ORCHARD_H__
#define __ORCHARD_H__

/*extern const char *gitversion;*/
extern struct evt_table orchard_events;

void halt(void);

#define ORCHARD_OS_VERSION_MAJOR      1
#define ORCHARD_OS_VERSION_MINOR      0

#define serialDriver                  (&SD1)
#define stream_driver                 ((BaseSequentialStream *)serialDriver)
extern void *stream;

#define i2cDriver                     (&I2CD2)
#define radioDriver                   (&KRADIO1)
#define bleDriver                     (&BLE1)
#define accelAddr                     0x1c
#define gpioxAddr                     0x43
#define ggAddr                        0x55
#define touchAddr                     0x5a
#define chargerAddr                   0x6b

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))
#endif

#define LAYOUT_BM                     1 /* Burning Man hardware */
#define LAYOUT_BC1                    2 /* EVT1B, bootcamp hardware */

#ifndef KEY_LAYOUT
#error "Must define a keylayout.  Set KEY_LAYOUT in Makefile."
#endif

#endif /* __ORCHARD_H__ */
