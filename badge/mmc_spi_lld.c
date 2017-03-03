/*-----------------------------------------------------------------------*/
/* MMCv3/SDv1/SDv2 Controls via Kinetis SPI module                       */
/*-----------------------------------------------------------------------*/

/*
 * This module implements a FatFs SDIO driver for ChibiOS using the
 * Freescale/NXP Kinetis ARM processor SPI interface. It's based on the
 * mmc_avr_sdio.c sample provided as a supplement to the FatFs source
 * code.
 *
 * The KW01 chip has two SPI channels. Channel 0 is tied directly to
 * the built-in radio chip, so we use channel 1 for SDIO.
 *
 * The SD card that we're targeting doesn't have pin connections for
 * insertion detection or write protect signals, so those are hard
 * wired (card detect always true, write protect always false).
 *
 * Chip select is currently set to port B, pin 0. This is wired to
 * the blue segment of the tri-color LED on the Freescale KW019032 board,
 * which means we can use it to indicate disk activity.
 *
 * SPI channel 2 is set to default to a baud rate divisor of 2, which
 * yields a clock speed of 24MHz. This seems to the maximum clock rate
 * an SD card in SPI mode can sustain.
 */

/*
/  Copyright (C) 2016, ChaN, all right reserved.
/
/ * This software is a free software and there is NO WARRANTY.
/ * No restriction on use. You can use, modify and redistribute it for
/   any purpose as you like UNDER YOUR RESPONSIBILITY.
/ * Redistributions of source code must retain the above copyright notice.
/
/-------------------------------------------------------------------------*/

#include "ch.h"
#include "hal.h"
#include "spi.h"
#include "pal.h"
#include "pit_lld.h"
#include "diskio.h"
#include "mmc.h"

#ifndef UPDATER
#include "dma_lld.h"
#endif

/* Peripheral controls (Platform dependent) */
#define CS_LOW() 		/* Set MMC_CS = low */		\
	palClearPad (MMC_CHIP_SELECT_PORT, MMC_CHIP_SELECT_PIN)
#define	CS_HIGH()		/* Set MMC_CS = high */		\
	palSetPad (MMC_CHIP_SELECT_PORT, MMC_CHIP_SELECT_PIN)
#define MMC_CD		1			/* Test if card detected.   yes:true, no:false, default:true */
#define MMC_WP		0			/* Test if write protected. yes:true, no:false, default:false */
#define	FCLK_SLOW()				/* Set SPI slow clock (100-400kHz) */
#define	FCLK_FAST()				/* Set SPI fast clock (20MHz max) */


/*--------------------------------------------------------------------------

   Module Private Functions

---------------------------------------------------------------------------*/

/* Definitions for MMC/SDC command */
#define CMD0	(0)		/* GO_IDLE_STATE */
#define CMD1	(1)		/* SEND_OP_COND (MMC) */
#define	ACMD41	(0x80+41)	/* SEND_OP_COND (SDC) */
#define CMD8	(8)		/* SEND_IF_COND */
#define CMD9	(9)		/* SEND_CSD */
#define CMD10	(10)		/* SEND_CID */
#define CMD12	(12)		/* STOP_TRANSMISSION */
#define ACMD13	(0x80+13)	/* SD_STATUS (SDC) */
#define CMD16	(16)		/* SET_BLOCKLEN */
#define CMD17	(17)		/* READ_SINGLE_BLOCK */
#define CMD18	(18)		/* READ_MULTIPLE_BLOCK */
#define CMD23	(23)		/* SET_BLOCK_COUNT (MMC) */
#define	ACMD23	(0x80+23)	/* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24	(24)		/* WRITE_BLOCK */
#define CMD25	(25)		/* WRITE_MULTIPLE_BLOCK */
#define CMD32	(32)		/* ERASE_ER_BLK_START */
#define CMD33	(33)		/* ERASE_ER_BLK_END */
#define CMD38	(38)		/* ERASE */
#define	CMD48	(48)		/* READ_EXTR_SINGLE */
#define	CMD49	(49)		/* WRITE_EXTR_SINGLE */
#define CMD55	(55)		/* APP_CMD */
#define CMD58	(58)		/* READ_OCR */


static volatile
DSTATUS Stat = STA_NOINIT;	/* Disk status */

static volatile
BYTE Timer1, Timer2;	/* 100Hz decrement timer */

static
BYTE CardType;			/* Card type flags (b0:MMC, b1:SDv1, b2:SDv2, b3:Block addressing) */



/*-----------------------------------------------------------------------*/
/* Power Control  (Platform dependent)                                   */
/*-----------------------------------------------------------------------*/
/* When the target system does not support socket power control, there   */
/* is nothing to do in these functions and chk_power always returns 1.   */

static
void power_on (void)
{
	/* Nothing to do for our board */
}


static
void power_off (void)
{
	/* Nothing to do for our board */
}



/*-----------------------------------------------------------------------*/
/* Transmit/Receive data from/to MMC via SPI  (Platform dependent)       */
/*-----------------------------------------------------------------------*/

/* Exchange a byte */
static
BYTE xchg_spi (		/* Returns received data */
	BYTE dat		/* Data to be sent */
)
{
	while ((SPI1->S & SPIx_S_SPTEF) == 0)
		;
	SPI1->DL = dat;
	while ((SPI1->S & SPIx_S_SPRF) == 0)
		;
	dat = SPI1->DL;

	return (dat);
}

/* Receive a data block fast */
static
void rcvr_spi_multi (
	BYTE *p,	/* Data read buffer */
	UINT cnt	/* Size of data block */
)
{
	/*
	 * When compiling this module for the firmware updater,
	 * use the slow but sure synchronous transfer mode. When
	 * building for the firmware itself, use the faster DMA mode.
	 */

#ifdef UPDATER
	UINT i;

	for (i = 0; i < cnt; i++) {
		SPI1->DL = 0xFF;
		while ((SPI1->S & SPIx_S_SPRF) == 0)
			;
		p[i] = SPI1->DL;
	}
#else
	/*
	 * Stop and restart the SPI channel. This right here? This is
	 * magic. If the DMA controller has already been used in
	 * non-DMA mode, you have to restart it in order to get it
	 * to work in DMA mode. This isn't particularly clear from
	 * the manual. If you don't do this, transfers return
	 * garbage.
	 */

	SPI1->C1 &= ~SPIx_C1_SPE;
	SPI1->C1 |= SPIx_C1_SPE;

	/*
	 * DMA transfers cause the SD card to overrun the receiver
	 * and result in corrupted reads unless we enable the FIFO.
	 */

	SPI1->C3 |= SPIx_C3_FIFOMODE;

	/* Set up the DMA source and destination addresses */

	DMA->ch[0].DSR_BCR = cnt;
	DMA->ch[0].SAR = (uint32_t)p;
	DMA->ch[1].DSR_BCR = cnt;
	DMA->ch[1].DAR = (uint32_t)p;

	/*
	 * When performing a block read, we need to have the
	 * MOSI pin pulled high in order for the SD card to
	 * send us data. This is usually accomplished by transmitting
	 * a buffer of all 1s at the same time we're receiving. But
	 * to do that, we need to do a memset() to populate the
	 * transfer buffer with 1 bits, and that wasts CPU cycles.
	 * To avoid this, we temporarily turn the MOSI pin back into
	 * a GPIO and force it high. We still need to perform a TX
	 * DMA transfer (otherwise the SPI controller won't toggle
	 * the clock pin), but the contents of the buffer don't
	 * matter: to the SD card it appears we're always sending
	 * 1 bits.
	 */

	palSetPadMode (GPIOE, 1, PAL_MODE_OUTPUT_PUSHPULL);
	palSetPad (GPIOE, 1);

	/*
	 * Start the transfer and wait for it to complete.
	 * We will be woken up by the DMA interrupt handler when
	 * the transfer completion interrupt triggers.
	 * Note: the whole block below is a critical section and
	 * must be guarded with syslock/sysunlock. We must ensure
	 * the interrupt service routine doesn't fire until
	 * osalThreadSuspendS() has saved a reference to the
	 * current thread to the dmaThread1 pointer, but we have
	 * to call osalThreadSuspendS()  after initiating the DMA
	 * transfer. There is a chance the DMA completion interrupt
	 * might trigger before dmaThread1 is updated, in which
	 * case the interrupt service routine will fail to wake
	 * us up. Initiating the transfer after calling osalSysLock()
	 * closes this race condition window: it masks off interrupts
	 * until osalThreadSuspendS() atomically puts this thread to
	 * sleep and then unmasks them again.
	 */

	osalSysLock ();

	SPI1->C2 |= SPIx_C2_RXDMAE;
	DMA->ch[1].DCR |= DMA_DCRn_ERQ;
	SPI1->C2 |= SPIx_C2_TXDMAE;
	DMA->ch[0].DCR |= DMA_DCRn_ERQ;

	osalThreadSuspendS (&dma1Thread);

	osalSysUnlock ();

	/* Release the MOSI pin. */

	palSetPadMode (GPIOE, 1, PAL_MODE_ALTERNATIVE_2);

	/* Disable SPI DMA mode and turn off the FIFO. */

	SPI1->C2 &= ~(SPIx_C2_TXDMAE|SPIx_C2_RXDMAE);
	SPI1->C3 &= ~SPIx_C3_FIFOMODE;
#endif
	return;
}


/* Send a data block fast */
static
void xmit_spi_multi (
	const BYTE *p,	/* Data block to be sent */
	UINT cnt		/* Size of data block */
)
{
	UINT i;

	for (i = 0; i < cnt; i++) {
		while ((SPI1->S & SPIx_S_SPTEF) == 0)
			;
		SPI1->DL = p[i];
		while ((SPI1->S & SPIx_S_SPRF) == 0)
			;
		(void)SPI1->DL;
	}

	return;
}



/*-----------------------------------------------------------------------*/
/* Wait for card ready                                                   */
/*-----------------------------------------------------------------------*/

static
int wait_ready (	/* 1:Ready, 0:Timeout */
	UINT wt			/* Timeout [ms] */
)
{
	BYTE d;


	Timer2 = wt / 10;
	do
		d = xchg_spi(0xFF);
	while (d != 0xFF && Timer2);

	return (d == 0xFF) ? 1 : 0;
}



/*-----------------------------------------------------------------------*/
/* Deselect the card and release SPI bus                                 */
/*-----------------------------------------------------------------------*/

static
void deselect (void)
{
	CS_HIGH();		/* Set CS# high */
	xchg_spi(0xFF);	/* Dummy clock (force DO hi-z for multiple slave SPI) */
}



/*-----------------------------------------------------------------------*/
/* Select the card and wait for ready                                    */
/*-----------------------------------------------------------------------*/

static
int select (void)	/* 1:Successful, 0:Timeout */
{
	CS_LOW();		/* Set CS# low */
	xchg_spi(0xFF);	/* Dummy clock (force DO enabled) */
	if (wait_ready(500)) return 1;	/* Wait for card ready */

	deselect();
	return 0;	/* Timeout */
}



/*-----------------------------------------------------------------------*/
/* Receive a data packet from MMC                                        */
/*-----------------------------------------------------------------------*/

static
int rcvr_datablock (
	BYTE *buff,			/* Data buffer to store received data */
	UINT btr			/* Byte count (must be multiple of 4) */
)
{
	BYTE token;


	Timer1 = 20;
	do {							/* Wait for data packet in timeout of 200ms */
		token = xchg_spi(0xFF);
	} while ((token == 0xFF) && Timer1);
	if (token != 0xFE) return 0;	/* If not valid data token, retutn with error */

	rcvr_spi_multi(buff, btr);		/* Receive the data block into buffer */
	xchg_spi(0xFF);					/* Discard CRC */
	xchg_spi(0xFF);

	return 1;						/* Return with success */
}



/*-----------------------------------------------------------------------*/
/* Send a data packet to MMC                                             */
/*-----------------------------------------------------------------------*/

#if	_USE_WRITE
static
int xmit_datablock (
	const BYTE *buff,	/* 512 byte data block to be transmitted */
	BYTE token			/* Data/Stop token */
)
{
	BYTE resp;


	if (!wait_ready(500)) return 0;

	xchg_spi(token);					/* Xmit data token */
	if (token != 0xFD) {	/* Is data token */
		xmit_spi_multi(buff, 512);		/* Xmit the data block to the MMC */
		xchg_spi(0xFF);					/* CRC (Dummy) */
		xchg_spi(0xFF);
		resp = xchg_spi(0xFF);			/* Reveive data response */
		if ((resp & 0x1F) != 0x05)		/* If not accepted, return with error */
			return 0;
	}

	return 1;
}
#endif



/*-----------------------------------------------------------------------*/
/* Send a command packet to MMC                                          */
/*-----------------------------------------------------------------------*/

static
BYTE send_cmd (		/* Returns R1 resp (bit7==1:Send failed) */
	BYTE cmd,		/* Command index */
	DWORD arg		/* Argument */
)
{
	BYTE n, res;


	if (cmd & 0x80) {	/* ACMD<n> is the command sequense of CMD55-CMD<n> */
		cmd &= 0x7F;
		res = send_cmd(CMD55, 0);
		if (res > 1) return res;
	}

	/* Select the card and wait for ready except to stop multiple block read */
	if (cmd != CMD12) {
		deselect();
		if (!select()) return 0xFF;
	}

	/* Send command packet */
	xchg_spi(0x40 | cmd);				/* Start + Command index */
	xchg_spi((BYTE)(arg >> 24));		/* Argument[31..24] */
	xchg_spi((BYTE)(arg >> 16));		/* Argument[23..16] */
	xchg_spi((BYTE)(arg >> 8));			/* Argument[15..8] */
	xchg_spi((BYTE)arg);				/* Argument[7..0] */
	n = 0x01;							/* Dummy CRC + Stop */
	if (cmd == CMD0) n = 0x95;			/* Valid CRC for CMD0(0) + Stop */
	if (cmd == CMD8) n = 0x87;			/* Valid CRC for CMD8(0x1AA) Stop */
	xchg_spi(n);

	/* Receive command response */
	if (cmd == CMD12) xchg_spi(0xFF);		/* Skip a stuff byte when stop reading */
	n = 10;								/* Wait for a valid response in timeout of 10 attempts */
	do
		res = xchg_spi(0xFF);
	while ((res & 0x80) && --n);

	return res;			/* Return with the response value */
}



/*--------------------------------------------------------------------------

   Public Functions

---------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/

DSTATUS mmc_disk_initialize (void)
{
	BYTE n, cmd, ty, ocr[4];

	power_off();						/* Turn off the socket power to reset the card */
	pitEnable (&PIT1, 0);
	for (Timer1 = 10; Timer1; ) ;		/* Wait for 100ms */
	pitDisable (&PIT1, 0);
	if (Stat & STA_NODISK) return Stat;	/* No card in the socket? */

	spiAcquireBus (&SPID2);
	SPI1->C1 &= ~SPIx_C1_SPIE;
	pitEnable (&PIT1, 0);
	power_on();							/* Turn on the socket power */
	FCLK_SLOW();
	for (n = 10; n; n--) xchg_spi(0xFF);	/* 80 dummy clocks */

	ty = 0;
	if (send_cmd(CMD0, 0) == 1) {			/* Put the card SPI mode */
		Timer1 = 100;						/* Initialization timeout of 1000 msec */
		if (send_cmd(CMD8, 0x1AA) == 1) {	/* Is the card SDv2? */
			for (n = 0; n < 4; n++) ocr[n] = xchg_spi(0xFF);	/* Get trailing return value of R7 resp */
			if (ocr[2] == 0x01 && ocr[3] == 0xAA) {				/* The card can work at vdd range of 2.7-3.6V */
				while (Timer1 && send_cmd(ACMD41, 1UL << 30));	/* Wait for leaving idle state (ACMD41 with HCS bit) */
				if (Timer1 && send_cmd(CMD58, 0) == 0) {		/* Check CCS bit in the OCR */
					for (n = 0; n < 4; n++) ocr[n] = xchg_spi(0xFF);
					ty = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;	/* Check if the card is SDv2 */
				}
			}
		} else {							/* SDv1 or MMCv3 */
			if (send_cmd(ACMD41, 0) <= 1) 	{
				ty = CT_SD1; cmd = ACMD41;	/* SDv1 */
			} else {
				ty = CT_MMC; cmd = CMD1;	/* MMCv3 */
			}
			while (Timer1 && send_cmd(cmd, 0));			/* Wait for leaving idle state */
			if (!Timer1 || send_cmd(CMD16, 512) != 0)	/* Set R/W block length to 512 */
				ty = 0;
		}
	}
	CardType = ty;
	deselect();

	if (ty) {			/* Initialization succeded */
		Stat &= ~STA_NOINIT;		/* Clear STA_NOINIT */
		FCLK_FAST();
	} else {			/* Initialization failed */
		power_off();
	}

	pitDisable (&PIT1, 0);
	SPI1->C1 |= SPIx_C1_SPIE;
	spiReleaseBus (&SPID2);

	return Stat;
}



/*-----------------------------------------------------------------------*/
/* Get Disk Status                                                       */
/*-----------------------------------------------------------------------*/

DSTATUS mmc_disk_status (void)
{
	return Stat;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT mmc_disk_read (
	BYTE *buff,			/* Pointer to the data buffer to store read data */
	DWORD sector,		/* Start sector number (LBA) */
	UINT count			/* Sector count (1..128) */
)
{
	BYTE cmd;


	if (!count) return RES_PARERR;
	if (Stat & STA_NOINIT) return RES_NOTRDY;

	if (!(CardType & CT_BLOCK)) sector *= 512;	/* Convert to byte address if needed */

	cmd = count > 1 ? CMD18 : CMD17;			/*  READ_MULTIPLE_BLOCK : READ_SINGLE_BLOCK */
	spiAcquireBus(&SPID2);
	SPI1->C1 &= ~SPIx_C1_SPIE;
	pitEnable (&PIT1, 0);
	if (send_cmd(cmd, sector) == 0) {
		do {
			if (!rcvr_datablock(buff, 512)) break;
			buff += 512;
		} while (--count);
		if (cmd == CMD18) send_cmd(CMD12, 0);	/* STOP_TRANSMISSION */
	}
	deselect();
	pitDisable (&PIT1, 0);
	SPI1->C1 |= SPIx_C1_SPIE;
	spiReleaseBus(&SPID2);

	return count ? RES_ERROR : RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
DRESULT mmc_disk_write (
	const BYTE *buff,	/* Pointer to the data to be written */
	DWORD sector,		/* Start sector number (LBA) */
	UINT count			/* Sector count (1..128) */
)
{
	if (!count) return RES_PARERR;
	if (Stat & STA_NOINIT) return RES_NOTRDY;
	if (Stat & STA_PROTECT) return RES_WRPRT;

	if (!(CardType & CT_BLOCK)) sector *= 512;	/* Convert to byte address if needed */

	spiAcquireBus(&SPID2);
	SPI1->C1 &= ~SPIx_C1_SPIE;
	pitEnable (&PIT1, 0);
	if (count == 1) {	/* Single block write */
		if ((send_cmd(CMD24, sector) == 0)	/* WRITE_BLOCK */
			&& xmit_datablock(buff, 0xFE))
			count = 0;
	}
	else {				/* Multiple block write */
		if (CardType & CT_SDC) send_cmd(ACMD23, count);
		if (send_cmd(CMD25, sector) == 0) {	/* WRITE_MULTIPLE_BLOCK */
			do {
				if (!xmit_datablock(buff, 0xFC)) break;
				buff += 512;
			} while (--count);
			if (!xmit_datablock(0, 0xFD))	/* STOP_TRAN token */
				count = 1;
		}
	}
	deselect();
	pitDisable (&PIT1, 0);
	SPI1->C1 |= SPIx_C1_SPIE;
	spiReleaseBus(&SPID2);

	return count ? RES_ERROR : RES_OK;
}
#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

#if _USE_IOCTL
DRESULT mmc_disk_ioctl (
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	DRESULT res;
	BYTE n, csd[16], *ptr = buff;
	DWORD *dp, st, ed, csize;
#if _USE_ISDIO
	SDIO_CTRL *sdi;
	BYTE rc, *bp;
	UINT dc;
#endif

	if (Stat & STA_NOINIT) return RES_NOTRDY;

	spiAcquireBus(&SPID2);
	SPI1->C1 &= ~SPIx_C1_SPIE;
	pitEnable (&PIT1, 0);
	res = RES_ERROR;
	switch (cmd) {
	case CTRL_SYNC :		/* Make sure that no pending write process. Do not remove this or written sector might not left updated. */
		if (select()) res = RES_OK;
		deselect();
		break;

	case GET_SECTOR_COUNT :	/* Get number of sectors on the disk (DWORD) */
		if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {
			if ((csd[0] >> 6) == 1) {	/* SDC ver 2.00 */
				csize = csd[9] + ((WORD)csd[8] << 8) + ((DWORD)(csd[7] & 63) << 16) + 1;
				*(DWORD*)buff = csize << 10;
			} else {					/* SDC ver 1.XX or MMC*/
				n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
				csize = (csd[8] >> 6) + ((WORD)csd[7] << 2) + ((WORD)(csd[6] & 3) << 10) + 1;
				*(DWORD*)buff = csize << (n - 9);
			}
			res = RES_OK;
		}
		deselect();
		break;

	case GET_BLOCK_SIZE :	/* Get erase block size in unit of sector (DWORD) */
		if (CardType & CT_SD2) {	/* SDv2? */
			if (send_cmd(ACMD13, 0) == 0) {	/* Read SD status */
				xchg_spi(0xFF);
				if (rcvr_datablock(csd, 16)) {				/* Read partial block */
					for (n = 64 - 16; n; n--) xchg_spi(0xFF);	/* Purge trailing data */
					*(DWORD*)buff = 16UL << (csd[10] >> 4);
					res = RES_OK;
				}
			}
		} else {					/* SDv1 or MMCv3 */
			if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {	/* Read CSD */
				if (CardType & CT_SD1) {	/* SDv1 */
					*(DWORD*)buff = (((csd[10] & 63) << 1) + ((WORD)(csd[11] & 128) >> 7) + 1) << ((csd[13] >> 6) - 1);
				} else {					/* MMCv3 */
					*(DWORD*)buff = ((WORD)((csd[10] & 124) >> 2) + 1) * (((csd[11] & 3) << 3) + ((csd[11] & 224) >> 5) + 1);
				}
				res = RES_OK;
			}
		}
		deselect();
		break;

	case CTRL_TRIM:		/* Erase a block of sectors (used when _USE_TRIM in ffconf.h is 1) */
		if (!(CardType & CT_SDC)) break;				/* Check if the card is SDC */
		if (mmc_disk_ioctl(MMC_GET_CSD, csd)) break;	/* Get CSD */
		if (!(csd[0] >> 6) && !(csd[10] & 0x40)) break;	/* Check if sector erase can be applied to the card */
		dp = buff; st = dp[0]; ed = dp[1];				/* Load sector block */
		if (!(CardType & CT_BLOCK)) {
			st *= 512; ed *= 512;
		}
		if (send_cmd(CMD32, st) == 0 && send_cmd(CMD33, ed) == 0 && send_cmd(CMD38, 0) == 0 && wait_ready(30000))	/* Erase sector block */
			res = RES_OK;	/* FatFs does not check result of this command */
		break;

	/* Following commands are never used by FatFs module */

	case MMC_GET_TYPE :		/* Get card type flags (1 byte) */
		*ptr = CardType;
		res = RES_OK;
		break;

	case MMC_GET_CSD :		/* Receive CSD as a data block (16 bytes) */
		if (send_cmd(CMD9, 0) == 0 && rcvr_datablock(ptr, 16))		/* READ_CSD */
			res = RES_OK;
		deselect();
		break;

	case MMC_GET_CID :		/* Receive CID as a data block (16 bytes) */
		if (send_cmd(CMD10, 0) == 0 && rcvr_datablock(ptr, 16))		/* READ_CID */
			
			res = RES_OK;
		deselect();
		break;

	case MMC_GET_OCR :		/* Receive OCR as an R3 resp (4 bytes) */
		if (send_cmd(CMD58, 0) == 0) {	/* READ_OCR */
			for (n = 4; n; n--) *ptr++ = xchg_spi(0xFF);
			res = RES_OK;
		}
		deselect();
		break;

	case MMC_GET_SDSTAT :	/* Receive SD statsu as a data block (64 bytes) */
		if (send_cmd(ACMD13, 0) == 0) {	/* SD_STATUS */
			xchg_spi(0xFF);
			if (rcvr_datablock(ptr, 64)) res = RES_OK;
		}
		deselect();
		break;

	case CTRL_POWER_OFF :	/* Power off */
		power_off();
		Stat |= STA_NOINIT;
		res = RES_OK;
		break;
#if _USE_ISDIO
	case ISDIO_READ:
		sdi = buff;
		if (send_cmd(CMD48, 0x80000000 | (DWORD)sdi->func << 28 | (DWORD)sdi->addr << 9 | ((sdi->ndata - 1) & 0x1FF)) == 0) {
			for (Timer1 = 100; (rc = xchg_spi(0xFF)) == 0xFF && Timer1; ) ;
			if (rc == 0xFE) {
				for (bp = sdi->data, dc = sdi->ndata; dc; dc--) *bp++ = xchg_spi(0xFF);
				for (dc = 514 - sdi->ndata; dc; dc--) xchg_spi(0xFF);
				res = RES_OK;
			}
		}
		deselect();
		break;

	case ISDIO_WRITE:
		sdi = buff;
		if (send_cmd(CMD49, 0x80000000 | (DWORD)sdi->func << 28 | (DWORD)sdi->addr << 9 | ((sdi->ndata - 1) & 0x1FF)) == 0) {
			xchg_spi(0xFF); xchg_spi(0xFE);
			for (bp = sdi->data, dc = sdi->ndata; dc; dc--) xchg_spi(*bp++);
			for (dc = 514 - sdi->ndata; dc; dc--) xchg_spi(0xFF);
			if ((xchg_spi(0xFF) & 0x1F) == 0x05) res = RES_OK;
		}
		deselect();
		break;

	case ISDIO_MRITE:
		sdi = buff;
		if (send_cmd(CMD49, 0x84000000 | (DWORD)sdi->func << 28 | (DWORD)sdi->addr << 9 | sdi->ndata >> 8) == 0) {
			xchg_spi(0xFF); xchg_spi(0xFE);
			xchg_spi(sdi->ndata);
			for (dc = 513; dc; dc--) xchg_spi(0xFF);
			if ((xchg_spi(0xFF) & 0x1F) == 0x05) res = RES_OK;
		}
		deselect();
		break;
#endif
	default:
		res = RES_PARERR;
	}

	pitDisable (&PIT1, 0);
	SPI1->C1 |= SPIx_C1_SPIE;
	spiReleaseBus(&SPID2);
	return res;
}
#endif


/*-----------------------------------------------------------------------*/
/* Device Timer Interrupt Procedure                                      */
/*-----------------------------------------------------------------------*/
/* This function must be called in period of 10ms                        */

void mmc_disk_timerproc (void)
{
	BYTE n, s;


	n = Timer1;				/* 100Hz decrement timer */
	if (n) Timer1 = --n;
	n = Timer2;
	if (n) Timer2 = --n;

	s = Stat;

	if (MMC_WP)				/* Write protected */
		s |= STA_PROTECT;
	else					/* Write enabled */
		s &= ~STA_PROTECT;

	if (MMC_CD)				/* Card inserted */
		s &= ~STA_NODISK;
	else					/* Socket empty */
		s |= (STA_NODISK | STA_NOINIT);

	Stat = s;				/* Update MMC status */
}

DWORD get_fattime(void)
{
	return (0);
}
