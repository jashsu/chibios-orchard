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
#include "radio_lld.h"
#include "fontlist.h"
#include "sound.h"
#include "ides_gfx.h"

#include <string.h>

static uint32_t shout_init (OrchardAppContext *context)
{
	(void)context;

	/*
	 * We don't want any extra stack space allocated for us.
	 * We'll use the heap.
	 */

	return (0);
}

static void shout_start (OrchardAppContext *context)
{
	OrchardUiContext * keyboardUiContext;
        context->instance->uicontext = NULL;
	char * p;
        if (airplane_mode_check() == false) { 
          p = chHeapAlloc (NULL, KW01_PKT_PAYLOADLEN);
          memset (p, 0, KW01_PKT_PAYLOADLEN);
          keyboardUiContext = chHeapAlloc(NULL, sizeof(OrchardUiContext));
          keyboardUiContext->itemlist = (const char **)chHeapAlloc(NULL,
                                                                   sizeof(char *) * 2);
          keyboardUiContext->itemlist[0] =
            "Shout something,\npress ENTER when done";
          keyboardUiContext->itemlist[1] = p;
          keyboardUiContext->total = KW01_PKT_PAYLOADLEN - 1;
          
          context->instance->ui = getUiByName ("keyboard");
          context->instance->uicontext = keyboardUiContext;
          
          context->instance->ui->start (context);
        }
        
	return;
}

static void shout_event (OrchardAppContext *context,
	const OrchardAppEvent *event)
{
	OrchardUiContext * keyboardUiContext;
	const OrchardUi * keyboardUi;
	font_t font;

	keyboardUi = context->instance->ui;
	keyboardUiContext = context->instance->uicontext;

	if (event->type == ugfxEvent)
		keyboardUi->event (context, event);

	if (event->type == appEvent && event->app.event == appTerminate)
		return;

	if (event->type == uiEvent) {
		if ((event->ui.code == uiComplete) &&
		    (event->ui.flags == uiOK)) {

			/* Terminate UI */

			keyboardUi->exit (context);

			/* Send the message */

			radioSend (&KRADIO1, RADIO_BROADCAST_ADDRESS,
			    RADIO_PROTOCOL_SHOUT,
			    keyboardUiContext->selected + 1,
			    keyboardUiContext->itemlist[1]);

			chHeapFree ((char *)keyboardUiContext->itemlist[1]);

			/* Display a confirmation message */

			font = gdispOpenFont(FONT_FIXED);
			gdispDrawStringBox (0, (gdispGetHeight() / 2) -
				gdispGetFontMetric(font, fontHeight),
				gdispGetWidth(),
				gdispGetFontMetric(font, fontHeight),
				"Shout sent!", font, Red, justifyCenter);
			gdispCloseFont (font);
			playVictory ();

			/* Wait for a second, then exit */

			chThdSleepMilliseconds (1000);

			orchardAppExit ();
		}
	}
	return;
}

static void shout_exit (OrchardAppContext *context)
{
  if (context->instance->uicontext) { 
    chHeapFree (context->instance->uicontext->itemlist);
    chHeapFree (context->instance->uicontext);
  }

  return;
}

orchard_app("Radio Shout", "shout.rgb", 0, shout_init, shout_start, shout_event, shout_exit);
