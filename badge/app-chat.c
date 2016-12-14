/*-
 * Copyright (c) 2016
 *      Bill Paul <wpaul@windriver.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "orchard-app.h"
#include "orchard-ui.h"
#include "radio.h"
#include "fontlist.h"
#include "sound.h"

#include <string.h>

typedef struct _ChatHandles {
	char *			listitems[MAX_ENEMIES + 1];
	OrchardUiContext	uiCtx;
	char			txbuf[KW01_PKT_PAYLOADLEN];
	char			rxbuf[CONFIG_NAME_MAXLEN +
				    KW01_PKT_PAYLOADLEN + 3];
	int			peer;
} ChatHandles;

extern orchard_app_instance instance;

static void chat_handler(KW01_PKT * pkt)
{
	/* Drop this message if the chat app isn't running. */

	if (instance.app != orchardAppByName ("Radio Chat"))
		return;

	orchardAppRadioCallback (pkt);

        return;
}

static uint32_t chat_init (OrchardAppContext *context)
{
	(void)context;

	/* Install the chat protocol handler */

	if (context == NULL)
		radioHandlerSet (&KRADIO1, RADIO_PROTOCOL_CHAT, chat_handler);

	return (0);
}

static void chat_start (OrchardAppContext *context)
{
	ChatHandles * p;
	font_t font;
	user ** enemies;
	int i;

	enemies = enemiesGet();

	p = chHeapAlloc(NULL, sizeof(ChatHandles));
	context->priv = p;
	gdispClear (Black);

	p->listitems[0] = "Select a badge/user";

	for (i = 0; i < MAX_ENEMIES; i++) {
		if (enemies[i] == NULL)
			break;
		p->listitems[i + 1] = enemies[i]->name;
	}

	if (i == 0) {
		font = gdispOpenFont(FONT_FIXED);
		gdispDrawStringBox (0, (gdispGetHeight() / 2) -
		    gdispGetFontMetric(font, fontHeight),
		    gdispGetWidth(),
		    gdispGetFontMetric (font, fontHeight),
		    "No badges found!", font, Red, justifyCenter);
		gdispCloseFont (font);
		chThdSleepMilliseconds (4000);
		orchardAppExit ();
		return;
	}

	p->uiCtx.itemlist = (const char **)p->listitems;
	p->uiCtx.total = i;

	context->instance->ui = getUiByName ("list");
	context->instance->uicontext = &p->uiCtx;
       	context->instance->ui->start (context);

	return;
}

static void chat_event (OrchardAppContext *context,
	const OrchardAppEvent *event)
{
	OrchardUiContext * uiContext;
	const OrchardUi * ui;
	ChatHandles * p;
	KW01_PKT * pkt;
	userconfig * config;
	user ** enemies;
	OrchardAppEvent e;

	enemies = enemiesGet ();
	p = context->priv;
	ui = context->instance->ui;
	uiContext = context->instance->uicontext;

	if (event->type == ugfxEvent)
		ui->event (context, event);

	if (event->type == radioEvent && event->radio.pPkt != NULL) {
		pkt = event->radio.pPkt;
		if (pkt->kw01_hdr.kw01_prot == RADIO_PROTOCOL_CHAT &&
		    pkt->kw01_hdr.kw01_src == enemies[p->peer]->netid) {
			chsnprintf (p->rxbuf, sizeof(p->rxbuf), "<%s> %s",
			    enemies[p->peer]->name, pkt->kw01_payload);
			p->listitems[0] = p->rxbuf;
			/* Tell the keyboard UI to redraw */
			e.type = uiEvent;
			ui->event (context, &e);
		}
	}

	if (event->type == appEvent && event->app.event == appTerminate)
		return;

	if (event->type == uiEvent &&
	    event->ui.code == uiComplete &&
	    event->ui.flags == uiOK) {
		/*
		 * If this is the list ui exiting, it means we chose a
		 * user to chat with. Now switch to the keyboard ui.
		 */
		if (context->instance->ui == getUiByName ("list")) {
			ui->exit (context);
			p->peer = uiContext->selected;
			config = getConfig();

			chsnprintf (p->txbuf, sizeof(p->txbuf),
			    "%s WANTS TO CHAT!",
			    config->name);

			radioSend (&KRADIO1, enemies[p->peer]->netid,
			    RADIO_PROTOCOL_SHOUT,
			    strlen (p->txbuf) + 1, p->txbuf);

			p->listitems[0] = "Type @ to exit";
			memset (p->txbuf, 0, sizeof(p->txbuf));
			p->listitems[1] = p->txbuf;

			p->uiCtx.itemlist = (const char **)p->listitems;
			p->uiCtx.total = KW01_PKT_PAYLOADLEN - 1;
			context->instance->ui = getUiByName ("keyboard");
			context->instance->uicontext = &p->uiCtx;
       			context->instance->ui->start (context);
		} else {
			/* 0xFF means exit chat */
			if (uiContext->total == 0xFF) {
				ui->exit (context);
				orchardAppExit ();
			} else {
				p->txbuf[uiContext->selected] = 0x0;
				radioSend (&KRADIO1, enemies[p->peer]->netid,
				    RADIO_PROTOCOL_CHAT,
				    uiContext->selected + 1, p->txbuf);
				memset (p->txbuf, 0, sizeof(p->txbuf));
				p->uiCtx.total =  KW01_PKT_PAYLOADLEN - 1;
				/* Tell the keyboard UI to redraw */
				e.type = uiEvent;
				e.ui.flags = uiCancel;
				ui->event (context, &e);
			}
		}
	}

	return;
}

static void chat_exit (OrchardAppContext *context)
{
	ChatHandles * p;

	p = context->priv;
	chHeapFree (p);

	return;
}

orchard_app("Radio Chat", APP_FLAG_AUTOINIT,
	chat_init, chat_start, chat_event, chat_exit);