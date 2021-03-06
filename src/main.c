#include "pebble.h"
#include "vars.h"

GColor background_color = GColorWhite;
GColor foreground_color = GColorBlack;
GCompOp compositing_mode = GCompOpAssign;

Window *window;
TextLayer *layer_date_text;
TextLayer *layer_wday_text;
TextLayer *layer_time_text;
#ifdef HANGOUT
TextLayer *layer_word_text;
TextLayer *layer_ulne_text;
#endif
#ifdef ACTIVITY
Layer *layer_graph;
#endif
Layer *layer_line;

BitmapLayer *layer_batt_img;
BitmapLayer *layer_conn_img;
GBitmap *img_battery_full;
GBitmap *img_battery_half;
GBitmap *img_battery_low;
GBitmap *img_battery_charge;
GBitmap *img_bt_connect;
GBitmap *img_bt_disconnect;
TextLayer *layer_batt_text;
int charge_percent = 0;
int cur_day = -1;
#ifdef HANGOUT
bool new_word = true;
#endif
#ifdef ACTIVITY
int totals[140/2] = { 0 };
#endif
	
	
#ifdef HANGOUT
#define INT_DIGITS 5		/* enough for 64 bit integer */
char *itoa(int i)
{
  /* Room for INT_DIGITS digits, - and '\0' */
  static char buf[INT_DIGITS + 2];
  char *p = buf + INT_DIGITS + 1;	/* points to terminating '\0' */
  if (i >= 0) {
    do {
      *--p = '0' + (i % 10);
      i /= 10;
    } while (i != 0);
    return p;
  }
  else {			/* i < 0 */
    do {
      *--p = '0' - (i % 10);
      i /= 10;
    } while (i != 0);
    *--p = '-';
  }
  return p;
}
#endif

	
#ifdef ACTIVITY
#define IDLE 128
#define SCALE 2
int min(int a, int b) { if (a<b) return(a); else return(b); }
int calc_height(int total){
int i, j, h = 0;
  for (i=0, j=1; i<=32; i=i+1) {
	  if ((total - IDLE) <= j) { h = i; break; }
    j = j * 2;
  }
  return(min(SCALE * h, 40));
}
void handle_graph_update(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

  int y;
  GPoint p0, p1;
  graphics_context_set_fill_color(ctx, GColorBlack);
  for (int x=0; x<140/2;x++) {
    if ((x % 10) == 0) {
	  p0 = GPoint(2*x+0, 0); p1 = GPoint(2*x+0, 6); graphics_draw_line(ctx, p0, p1); 
	}

	y = calc_height(totals[x]); 
    if (x > (140/2 - 8)) APP_LOG(APP_LOG_LEVEL_INFO, "UPDATE_GRAPH: %d - (%d, %d)", totals[x], x, y);
	p0 = GPoint(2*x+0, 40); p1 = GPoint(2*x+0, 40 - y); graphics_draw_line(ctx, p0, p1); 
	p0 = GPoint(2*x+1, 40); p1 = GPoint(2*x+1, 40 - y); graphics_draw_line(ctx, p0, p1); 
  }
} 

void handle_accel_data(AccelData *accel_data, uint32_t num_samples) {
static int prev_x = 0, prev_y = 0, prev_z = 0;
static int tick = 0;
static int x_axis = 140/2-1;
static int total = 0;
int next_x, next_y, next_z;
int delta_x, delta_y, delta_z;
int delta;	
	
  next_x = next_y = next_z = 0;
  for (uint32_t i=0;i<num_samples;i++){
	next_x = next_x + accel_data[i].x;
	next_y = next_y + accel_data[i].y;
	next_z = next_z + accel_data[i].z;
  }
  delta_x = abs(next_x - prev_x)/QUANTIZATION; prev_x = next_x;	
  delta_y = abs(next_y - prev_y)/QUANTIZATION; prev_y = next_y;	
  delta_z = abs(next_z - prev_z)/QUANTIZATION; prev_z = next_z;	
  delta   = delta_x + delta_y + delta_z;

  if ((tick % 60) == 0) { 
	  total = delta; tick = 1;
  } else { 
	  total = total + delta; tick++;
  }
  if ((tick % 60) == 0) {
	if (x_axis < 140/2-1) {
      totals[x_axis] = total;
	  x_axis++; 
	} else {
	  for (int i=0; i<140/2-1; i++) { totals[i] = totals[i+1]; }
      totals[x_axis] = total;
	}
  }
}
#endif
	
void handle_battery(BatteryChargeState charge_state) {
    static char battery_text[] = "100 ";

	APP_LOG(APP_LOG_LEVEL_INFO, "HANDLE BATTERY");
    if (charge_state.is_charging) {
        bitmap_layer_set_bitmap(layer_batt_img, img_battery_charge);

        snprintf(battery_text, sizeof(battery_text), "+%d", charge_state.charge_percent);
    } else {
        snprintf(battery_text, sizeof(battery_text), "%d", charge_state.charge_percent);
        if (charge_state.charge_percent <= 20) {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_low);
        } else if (charge_state.charge_percent <= 50) {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_half);
        } else {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_full);
        }

        /*if (charge_state.charge_percent < charge_percent) {
            if (charge_state.charge_percent==20){
                vibes_double_pulse();
            } else if(charge_state.charge_percent==10){
                vibes_long_pulse();
            }
        }*/ 
    }
    charge_percent = charge_state.charge_percent;
    text_layer_set_text(layer_batt_text, battery_text);
}

void handle_bluetooth(bool connected) {
    if (connected) {
        bitmap_layer_set_bitmap(layer_conn_img, img_bt_connect);
    } else {
        bitmap_layer_set_bitmap(layer_conn_img, img_bt_disconnect);
        vibes_long_pulse();
    }
}

void handle_appfocus(bool in_focus){
    if (in_focus) {
        handle_bluetooth(bluetooth_connection_service_peek());
        handle_battery(battery_state_service_peek());
    }
}

void line_layer_update_callback(Layer *layer, GContext* ctx) {
    graphics_context_set_fill_color(ctx, foreground_color);
    graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

void update_time(struct tm *tick_time) {
    // Need to be static because they're used by the system later.
    static char time_text[] = "00:00";
#ifdef HANGOUT
    static char wdat_text[] = "00";
    static char wday_text[] = "Xxx";
    static char mnth_text[] = "Xxx";
    static char date_text[] = "Xxx Xxx 00  ";
#else
    static char date_text[] = "Xxxxxxxxx 00";
    static char wday_text[] = "Xxxxxxxxx";
#endif
    
    char *time_format;

    // Only update the date when it's changed.
    int new_cur_day = tick_time->tm_year*1000 + tick_time->tm_yday;
    if (new_cur_day != cur_day) {
        cur_day = new_cur_day;

	
#ifdef HANGOUT
        strftime(wdat_text, sizeof(wdat_text), "%e", tick_time);
        strftime(wday_text, sizeof(wday_text), "%A", tick_time);
        strftime(mnth_text, sizeof(mnth_text), "%B", tick_time);
		strcpy(date_text, wday_text); strcat(date_text, "  ");
		strcat(date_text, mnth_text); strcat(date_text, " ");
		strcat(date_text, wdat_text);
#else
        strftime(date_text, sizeof(date_text), "%B %e", tick_time);
#endif
		text_layer_set_text(layer_date_text, date_text);
		
#ifndef HANGOUT
		strftime(wday_text, sizeof(wday_text), "%A", tick_time);
        text_layer_set_text(layer_wday_text, wday_text);
#endif
    }

    if (clock_is_24h_style()) {
        time_format = "%R";
    } else {
        time_format = "%I:%M";
    }

    strftime(time_text, sizeof(time_text), time_format, tick_time);

    // Kludge to handle lack of non-padded hour format string
    // for twelve hour clock.
    if (!clock_is_24h_style() && (time_text[0] == '0')) {
        memmove(time_text, &time_text[1], sizeof(time_text) - 1);
    }

    text_layer_set_text(layer_time_text, time_text);
	
#ifndef ACTIVITY
#ifdef HANGOUT
    static int word_idx;
	static int word_len;
	static int lttr_msk;
	static int rdm_lttr;
	static int pick_msk;
	static int cmpl_msk;
	static char word_text[16];
	static char owrd_text[32];
    static char blanks[]    = "                               ";
//	static char ulne_text[16];
//    static char underline[] = "= = = = = = = = = = = = = = = =";
	if (new_word) {
		lttr_msk = 0; pick_msk = 0;
		word_idx = rand() % WL_LEN;
		strcpy(word_text, wlst[word_idx]);
		word_len = strlen(word_text);
		cmpl_msk = (1 << word_len) - 1;
		strncpy(owrd_text, blanks, 2*word_len-1);
  		owrd_text[2*word_len] = '\0';
//		strncpy(ulne_text, underline, 2*word_len-1); 
//		ulne_text[2*word_len] = '\0';
		new_word = false;
	} else {
		if (lttr_msk == cmpl_msk) {
			new_word = true;
		} else {
			rdm_lttr = rand() % word_len;
			pick_msk = 1 << rdm_lttr;
			while (lttr_msk & pick_msk) {
				rdm_lttr = rand() % word_len;
				pick_msk = 1 << rdm_lttr;	
			}
			lttr_msk = lttr_msk | pick_msk;
		}
	}

//	static char debug0[32];
//	strcpy(debug0, itoa(lttr_msk*100 + pick_msk));
	
	for (int i=0; i<word_len; i++) { 
		if (lttr_msk & (1<<i)) {
			owrd_text[2*i] = word_text[i]; 
		} else {
			owrd_text[2*i] = '_'; 
		}
	}
	
	text_layer_set_text(layer_word_text, owrd_text);
//    text_layer_set_text(layer_ulne_text, ulne_text);
#endif
#endif
}

void set_style(void) {
    bool inverse = persist_read_bool(STYLE_KEY);

#ifdef HANGOUT
	inverse = false;
#endif
    
    background_color  = inverse ? GColorWhite : GColorBlack;
    foreground_color  = inverse ? GColorBlack : GColorWhite;
    compositing_mode  = inverse ? GCompOpAssign : GCompOpAssignInverted;
    
    window_set_background_color(window, background_color);
    text_layer_set_text_color(layer_time_text, foreground_color);
    text_layer_set_text_color(layer_wday_text, foreground_color);
    text_layer_set_text_color(layer_date_text, foreground_color);
    text_layer_set_text_color(layer_batt_text, foreground_color);
    bitmap_layer_set_compositing_mode(layer_batt_img, compositing_mode);
    bitmap_layer_set_compositing_mode(layer_conn_img, compositing_mode);
}

void force_update(void) {
	APP_LOG(APP_LOG_LEVEL_INFO, "FORCE UPDATE");
    handle_battery(battery_state_service_peek());
    handle_bluetooth(bluetooth_connection_service_peek());
    time_t now = time(NULL);
    update_time(localtime(&now));
}

void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
    update_time(tick_time);
}

void handle_deinit(void) {
    tick_timer_service_unsubscribe();
    battery_state_service_unsubscribe();
    bluetooth_connection_service_unsubscribe();
    app_focus_service_unsubscribe();
    accel_tap_service_unsubscribe();
}

void handle_tap(AccelAxisType axis, int32_t direction) {
    persist_write_bool(STYLE_KEY, !persist_read_bool(STYLE_KEY));
    set_style();
    force_update();
    vibes_long_pulse();
    accel_tap_service_unsubscribe();
}

void handle_tap_timeout(void* data) {
    accel_tap_service_unsubscribe();
}

void handle_init(void) {
    window = window_create();
    window_stack_push(window, true /* Animated */);

    // resources
    img_bt_connect     = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CONNECT);
    img_bt_disconnect  = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_DISCONNECT);
    img_battery_full   = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_FULL);
    img_battery_half   = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_HALF);
    img_battery_low    = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_LOW);
    img_battery_charge = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_CHARGE);

    // layers
#ifdef HANGOUT
    layer_date_text = text_layer_create(GRect(8, 43, 144-8, 30));
    layer_time_text = text_layer_create(GRect(7, 69, 144-7, 50));
    layer_line      = layer_create(GRect(8, 72, 128, 2));
#else
    layer_date_text = text_layer_create(GRect(8, 68, 144-8, 168-68));
    layer_time_text = text_layer_create(GRect(7, 92, 144-7, 168-92));
    layer_line      = layer_create(GRect(8, 97, 128, 2));
#endif

    layer_wday_text = text_layer_create(GRect(8, 47, 144-8, 168-68));
	layer_batt_text = text_layer_create(GRect(3,20,30,20));
    layer_batt_img  = bitmap_layer_create(GRect(10, 10, 16, 16));
    layer_conn_img  = bitmap_layer_create(GRect(118, 12, 20, 20));

#ifdef HANGOUT
	layer_word_text = text_layer_create(GRect(7, 130, 144-7, 30));
    text_layer_set_text_color(layer_word_text, GColorWhite);
	text_layer_set_background_color(layer_word_text, GColorClear);
    text_layer_set_font(layer_word_text, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_22)));
    text_layer_set_text_alignment(layer_word_text, GTextAlignmentCenter);

//	layer_ulne_text = text_layer_create(GRect(7, 150, 144-7, 30));
//    text_layer_set_text_color(layer_ulne_text, GColorWhite);
//    text_layer_set_background_color(layer_ulne_text, GColorClear);
//    text_layer_set_font(layer_ulne_text, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_22)));
//    text_layer_set_text_alignment(layer_ulne_text, GTextAlignmentCenter);
#endif
#ifdef ACTIVITY
	layer_graph = layer_create(GRect(0, 128, 140, 160));
#endif

	
    text_layer_set_background_color(layer_wday_text, GColorClear);
    text_layer_set_font(layer_wday_text, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_22)));

    text_layer_set_background_color(layer_date_text, GColorClear);
    text_layer_set_font(layer_date_text, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_22)));

    text_layer_set_background_color(layer_time_text, GColorClear);
    text_layer_set_font(layer_time_text, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_BOLD_SUBSET_44)));

    text_layer_set_background_color(layer_batt_text, GColorClear);
    text_layer_set_font(layer_batt_text, fonts_get_system_font(FONT_KEY_FONT_FALLBACK));
    text_layer_set_text_alignment(layer_batt_text, GTextAlignmentCenter);

    bitmap_layer_set_bitmap(layer_batt_img, img_battery_full);
    bitmap_layer_set_bitmap(layer_conn_img, img_bt_connect);

    layer_set_update_proc(layer_line, line_layer_update_callback);

    // composing layers
    Layer *window_layer = window_get_root_layer(window);

    layer_add_child(window_layer, layer_line);
    layer_add_child(window_layer, bitmap_layer_get_layer(layer_batt_img));
    layer_add_child(window_layer, bitmap_layer_get_layer(layer_conn_img));
    layer_add_child(window_layer, text_layer_get_layer(layer_wday_text));
    layer_add_child(window_layer, text_layer_get_layer(layer_date_text));
    layer_add_child(window_layer, text_layer_get_layer(layer_time_text));
    layer_add_child(window_layer, text_layer_get_layer(layer_batt_text));
	
#ifdef HANGOUT
    layer_add_child(window_layer, text_layer_get_layer(layer_word_text));
//    layer_add_child(window_layer, text_layer_get_layer(layer_ulne_text));
	srand(time(NULL));
#endif
#ifdef ACTIVITY
    layer_add_child(window_layer, layer_graph);
	layer_set_update_proc(layer_graph, &handle_graph_update);
#endif

    // style
    set_style();
	
    // handlers
    battery_state_service_subscribe(&handle_battery);
    bluetooth_connection_service_subscribe(&handle_bluetooth);
    app_focus_service_subscribe(&handle_appfocus);
    tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
    accel_tap_service_subscribe(handle_tap);
    app_timer_register(2000, handle_tap_timeout, NULL);

	// draw first frame
    force_update();

#ifdef ACTIVITY
	accel_data_service_subscribe(10, &handle_accel_data);
	accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
#endif

}


int main(void) {
    handle_init();

    app_event_loop();

    handle_deinit();
}
