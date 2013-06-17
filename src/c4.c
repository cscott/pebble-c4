#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"


#define MY_UUID { 0x22, 0xD3, 0x09, 0x29, 0x93, 0xBA, 0x42, 0xAE, 0xBB, 0xB1, 0x04, 0xD1, 0xB3, 0xAB, 0x1E, 0xEC }
PBL_APP_INFO(MY_UUID,
             "C4", "cscott.net",
             1, 0, /* App version */
             RESOURCE_ID_IMAGE_MENU_ICON,
             APP_INFO_WATCH_FACE);

/* use wc -l to come up with a reasonable value here */
#define MAX_DEFS 147
#define MAX_DEF_LENGTH 350

Window window;

TextLayer text_date_layer;
TextLayer text_time_layer;

TextLayer c4call_layer;
TextLayer c4def_layer;

Layer line_layer;

ResHandle c4handle;
uint32_t def_index[MAX_DEFS+1];
uint16_t current_def, total_defs;


void build_index(void) {
#define CHUNK_SIZE 1024
    uint8_t data[CHUNK_SIZE];
    // read the file in chunks
    uint16_t i = 0;
    uint32_t start = 0;
    size_t read, j;
    c4handle = resource_get_handle(RESOURCE_ID_C4DEFS);
    def_index[i++] = start;
    while (1) {
        read = resource_load_byte_range(c4handle, start, data,
                                        CHUNK_SIZE);
        if (read <= 0) { break; }
        for (j=0; j<read; ) {
            if (data[j++]=='\n') {
                def_index[i++] = start + j;
                if (i == (MAX_DEFS+1)) { goto no_more_chunks; }
            }
        }
        start += read;
    }
    if (i>0 && def_index[i-1] != start) {
        def_index[i++] = start;
    }
 no_more_chunks:
    total_defs = (i-1);
    current_def = 0;
}

void update_call(uint16_t which, unsigned short show_def) {
    static uint8_t data[MAX_DEF_LENGTH+1];
    uint32_t start = def_index[which];
    uint32_t end = def_index[which+1] - 1;
    uint32_t size = end - start;
    uint8_t *def, c;
    if (size > MAX_DEF_LENGTH) { size = MAX_DEF_LENGTH; }
    resource_load_byte_range(c4handle, start, data, size);
    data[size] = 0;
    /* look for end of call name */
    for (def = data; ; def++) {
        c = *def;
        if (c==0 || c=='\t') { break; }
    }
    if (*def == '\t') { *def++ = 0; }
    text_layer_set_text(&c4call_layer, (char*)data);
    if (!show_def) { *def = 0; } // hide definition
    text_layer_set_text(&c4def_layer, (char*)def);
}

void line_layer_update_callback(Layer *me, GContext* ctx) {
  (void)me;

  graphics_context_set_stroke_color(ctx, GColorWhite);

  graphics_draw_line(ctx, GPoint(8, 97+9), GPoint(131, 97+9));
  graphics_draw_line(ctx, GPoint(8, 98+9), GPoint(131, 98+9));

}


void handle_init(AppContextRef ctx) {
  TextLayer *layers[4] = { &text_date_layer, &text_time_layer,
                           &c4call_layer, &c4def_layer };
  unsigned int i;
  (void)ctx;

  window_init(&window, "C4");
  window_stack_push(&window, true /* Animated */);
  window_set_background_color(&window, GColorBlack);

  resource_init_current_app(&APP_RESOURCES);

  for (i=0; i<4; i++) {
      text_layer_init(layers[i], window.layer.frame);
      text_layer_set_text_color(layers[i], GColorWhite);
      text_layer_set_background_color(layers[i], GColorClear);
      layer_add_child(&window.layer, &(layers[i]->layer));
  }
  layer_set_frame(&text_date_layer.layer, GRect(0, 168-24, 144, 24));
  text_layer_set_font(&text_date_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_21)));
  text_layer_set_text_alignment(&text_date_layer, GTextAlignmentCenter);

  layer_set_frame(&text_time_layer.layer, GRect(0, 99, 144, 168-99));
  text_layer_set_font(&text_time_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_BOLD_SUBSET_49)));
  text_layer_set_text_alignment(&text_time_layer, GTextAlignmentCenter);

  layer_set_frame(&c4call_layer.layer, GRect(0, 0, 144, 18));
  text_layer_set_font(&c4def_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_BOLD_14)));
  text_layer_set_text_alignment(&c4call_layer, GTextAlignmentCenter);

  layer_set_frame(&c4def_layer.layer, GRect(0, 16, 144, 106-16));
  text_layer_set_font(&c4def_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_14)));


  layer_init(&line_layer, window.layer.frame);
  line_layer.update_proc = &line_layer_update_callback;
  layer_add_child(&window.layer, &line_layer);

  // build index of definitions
  build_index();
  text_layer_set_text(&c4call_layer, "C4 Calls");
}


void handle_minute_tick(AppContextRef ctx, PebbleTickEvent *t) {

  (void)ctx;

  // Need to be static because they're used by the system later.
  static char time_text[] = "00:00";
  static char date_text[] = "Xxxxxxxxx 00";

  char *time_format;


  // TODO: Only update the date when it's changed.
  string_format_time(date_text, sizeof(date_text), "%B %e", t->tick_time);
  text_layer_set_text(&text_date_layer, date_text);


  if (clock_is_24h_style()) {
    time_format = "%R";
  } else {
    time_format = "%I:%M";
  }

  string_format_time(time_text, sizeof(time_text), time_format, t->tick_time);

  // Kludge to handle lack of non-padded hour format string
  // for twelve hour clock.
  if (!clock_is_24h_style() && (time_text[0] == '0')) {
    memmove(time_text, &time_text[1], sizeof(time_text) - 1);
  }

  text_layer_set_text(&text_time_layer, time_text);
}

static void handle_second_tick(AppContextRef ctx, PebbleTickEvent *t) {
    static unsigned short first = 1;
    int sec = t->tick_time->tm_sec;
    unsigned short show_def = 1;
    if ((sec % 5) != 0 && !first) { return; }
    if ((sec % 10) == 0) {
        current_def++;
        if (current_def >= total_defs) { current_def = 0; }
        show_def = 0;
    }
    update_call(current_def, show_def);

    if (sec==0 || first) {
        handle_minute_tick(ctx, t);
    }
    first = 0;
}

void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,

    .tick_info = {
      .tick_handler = &handle_second_tick,
      .tick_units = SECOND_UNIT
    }

  };
  app_event_loop(params, &handlers);
}
