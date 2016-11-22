#include "orchard-app.h"
#include "orchard-ui.h"
#include "configme.h"

#include "storage.h"

#include <string.h>

struct OrchardUiContext textUiContext;

static char myname[TEXTENTRY_MAXLEN + 1];
  
static void redraw_ui(void) {
  char tmp[] = "Enter your name";
  
  coord_t width;
  coord_t height;
  font_t font;
  struct myconfig newconfig;

  orchardGfxStart();

  // draw the title bar
  font = gdispOpenFont("DejaVuSans20");
  width = gdispGetWidth();
  height = gdispGetFontMetric(font, fontHeight);

  gdispClear(Black);
  gdispFillArea(0, 0, width, height, White);
  gdispDrawStringBox(0, 0, width, height,
                     tmp, font, Black, justifyCenter);

  //  memcpy(&newconfig, family, sizeof(struct myconfig));
  //  strncpy(newFamily.name, myname, GENE_NAMELENGTH);
  //  newFamily.name[NAME_MAXLEN] = '\0'; // enforce the null terminator
  
  //  storagePatchData(GENE_BLOCK, (uint32_t *) &newFamily, GENE_OFFSET, sizeof(struct genes));

  gdispFlush();
  orchardGfxEnd();
}

static void draw_confirmation(void) {
  coord_t width;
  coord_t height;
  font_t font;
  char tmp[] = "Your name is now:";
  char tmp2[] = "Press select to continue.";

  redraw_ui();  // clear the text entry area
  
  orchardGfxStart();
  font = gdispOpenFont("UI1");
  width = gdispGetWidth();
  height = gdispGetFontMetric(font, fontHeight);

  gdispDrawStringBox(0, height, width, height,
                     tmp, font, White, justifyCenter);

  gdispDrawStringBox(0, height * 2, width, height,
                     myname, font, White, justifyCenter);
  
  gdispDrawStringBox(0, height * 4, width, height,
                     tmp2, font, White, justifyCenter);
  
  gdispFlush();
  orchardGfxEnd();
  
}

static uint32_t name_init(OrchardAppContext *context) {

  (void)context;

  return 0;
}

static void name_start(OrchardAppContext *context) {
  const OrchardUi *textUi;
  
  redraw_ui();
  // all this app does is launch a text entry box and store the name
  textUi = getUiByName("textentry");
  textUiContext.itemlist = NULL;
  if( textUi != NULL ) {
    context->instance->uicontext = &textUiContext;
    context->instance->ui = textUi;
  }
  textUi->start(context);
  
}

void name_event(OrchardAppContext *context, const OrchardAppEvent *event) {

  (void)context;
  
  if (event->type == keyEvent) {
    if ( (event->key.flags == keyDown) && (event->key.code == keySelect) ) {
      orchardAppExit();
    }
  } else if( event->type == uiEvent ) {
    if(( event->ui.code == uiComplete ) && ( event->ui.flags == uiOK )) {
      strncpy(myname, (char *) context->instance->ui_result, TEXTENTRY_MAXLEN + 1);
    }
    chprintf( stream, "My name is: %s\n\r", myname );

    draw_confirmation();
  }
}

static void name_exit(OrchardAppContext *context) {

  (void)context;
}

orchard_app("Set your name", name_init, name_start, name_event, name_exit);


