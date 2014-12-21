#include <pebble.h>

/* use wc -l to come up with a reasonable value here */
#define MAX_DEFS 392
#define MAX_DEF_LENGTH 525

#define DEFS_IN_ORDER 0

static Window *s_main_window;

static TextLayer *s_date_layer;
static TextLayer *s_time_layer;

static TextLayer *s_c4call_layer;
static TextLayer *s_c4def_layer;

static Layer *s_line_layer;

static uint16_t lfsr = 0xAACEu;
void seed_random(void) {
  time_t t = time(NULL);
  struct tm * tm = localtime(&t);
  // overflow is fine here. seed is ~ # secs since start of month
  // seconds are quantized to 10 second intervals
  lfsr=(tm->tm_sec/10 + 60*(tm->tm_min + 60*(tm->tm_hour + 24*tm->tm_mday)));
  if (lfsr==0) { lfsr++; }
}

/* generate a uniform random number in the range [0,1] */
static unsigned short get_random_bit(void) {
  /* 16-bit galois LFSR, period 65535. */
  /* see http://en.wikipedia.org/wiki/Linear_feedback_shift_register */
  /* taps: 16 14 13 11; feedback polynomial: x^16 + x^14 + x^13 + x^11 + 1 */
  unsigned short out = lfsr & 1u;
  lfsr = (lfsr >> 1) ^ (-(out) & 0xB400u);
  return out;
}
/* generate a uniform random number in the range [0, 2^n) */
static uint16_t get_random_bits(unsigned short n) {
  uint16_t out = 0;
  while (n--) { out = (out << 1) | get_random_bit(); }
  return out;
}
/* generate a uniform random number in the range [0, max) */
// max should be in range (1,1024]
uint16_t get_random_int(uint16_t max) {
  uint16_t val;
  uint16_t low;
  low = 1024 % max;
  do {
    // this loop is necessary to ensure the numbers are uniformly
    // distributed.
    val = get_random_bits(10);
  } while (val < low);
  return val % max;
}

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
    read = resource_load_byte_range(
      c4handle, start, data, CHUNK_SIZE
    );
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
  seed_random();
  current_def = get_random_int(total_defs);
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
  text_layer_set_text(s_c4call_layer, (char*)data);
  if (!show_def) { *def = 0; } // hide definition
  text_layer_set_text(s_c4def_layer, (char*)def);
}

static void line_layer_update_callback(Layer *me, GContext* ctx) {
  GRect bounds = layer_get_bounds(me);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, 0);
}

static void main_window_load(Window *window) {
  static TextLayer **layers[4] = {
    &s_date_layer, &s_time_layer, &s_c4call_layer, &s_c4def_layer
  };
  unsigned i;

  window_set_background_color(window, GColorBlack);
  
  // Create TextLayers
  s_date_layer = text_layer_create(GRect(0, 168-24, 144, 24));
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);

  s_time_layer = text_layer_create(GRect(0, 99, 144, 168-99));
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  s_c4call_layer = text_layer_create(GRect(0, 0, 144, 18));
  text_layer_set_font(s_c4call_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(s_c4call_layer, GTextAlignmentCenter);

  s_c4def_layer = text_layer_create(GRect(0, 16, 144, 106-16));
  text_layer_set_font(s_c4def_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));

  for (i=0; i<4; i++) {
    TextLayer *tl = *layers[i];
    text_layer_set_text_color(tl, GColorWhite);
    text_layer_set_background_color(tl, GColorClear);
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(tl));
  }
  
  s_line_layer = layer_create(GRect(8,97+9,144-8*2,2));
  layer_set_update_proc(s_line_layer, line_layer_update_callback);
  layer_add_child(window_get_root_layer(window), s_line_layer);

  // build index of definitions
  build_index();
}

static void main_window_unload(Window *window) {
  // Destroy TextLayers
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_c4call_layer);
  text_layer_destroy(s_c4def_layer);
}

static void handle_day_tick(struct tm *tick_time, TimeUnits units_changed) {
  static char date_text[] = "XxxxxxxxxZZZ 00";
  strftime(date_text, sizeof(date_text), "%B %e", tick_time);
  text_layer_set_text(s_date_layer, date_text);

}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  // Create a long-lived buffer
  static char buffer[] = "00:00";
  clock_copy_time_string(buffer, sizeof(buffer));
  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, buffer);
}

static void handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
  int sec = tick_time->tm_sec;
  unsigned short show_def = 1;
  if (units_changed&DAY_UNIT) { handle_day_tick(tick_time, units_changed); }
  if (units_changed&MINUTE_UNIT) { handle_minute_tick(tick_time, units_changed); }
  if ((sec % 5) != 0) { return; }
  if ((sec % 10) == 0) {
    if (DEFS_IN_ORDER) {
      current_def++;
      if (current_def >= total_defs) { current_def = 0; }
    } else {
      seed_random(); // make this a function of current time
      current_def = get_random_int(total_defs);
    }
    show_def = 0;
  }
  update_call(current_def, show_def);
}

static void init() {
  time_t t = time(NULL);
  struct tm * tm = localtime(&t);

  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true /* Animated */);

  // Register with TickTimerService
  tick_timer_service_subscribe(SECOND_UNIT, handle_second_tick);

  // Make sure the time is displayed from the start
  tm->tm_sec -= tm->tm_sec % 5;
  handle_second_tick(tm, ~0);
}

static void deinit() {
  // Destroy Window
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}