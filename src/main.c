#include "pebble.h"
#include "QTPlus.h"

#define TIME_FRAME      (GRect(0, 0, 144, 66))
#define DATE_FRAME      (GRect(0, 55, 144, 90-55))
#define HUB_FRAME      (GRect(0, 90, 144, 168-90))

/* Possible messages received from the config page
*/
#define CONFIG_KEY_BATTERY   (0)

/* Global variables to keep track of the UI elements */
static Window *window;
static TextLayer *date_layer;
static TextLayer *time_layer;
InverterLayer *inv_layer;

static char date_text[] = "XXX 00";
static char time_text[] = "00:00";

/* Preload the fonts */
GFont font_date;
GFont font_time;

static TextLayer *template1_value_layer;
static char template1_value[16];
static BitmapLayer *template1_icon_layer;
static GBitmap *template1_icon_bitmap = NULL;

static TextLayer *template2_value_layer;
static char template2_value[16];
static BitmapLayer *template2_icon_layer;
static GBitmap *template2_icon_bitmap = NULL;

static TextLayer *template3_value_layer;
static char template3_value[16];
static BitmapLayer *template3_icon_layer;
static GBitmap *template3_icon_bitmap = NULL;

static TextLayer *template4_value_layer;
static char template4_value[16];
static BitmapLayer *template4_icon_layer;
static GBitmap *template4_icon_bitmap = NULL;

static TextLayer *template5_value_layer;
static char template5_value[16];
static BitmapLayer *template5_icon_layer;
static GBitmap *template5_icon_bitmap = NULL;

static TextLayer *template6_value_layer;
static char template6_value[16];
static BitmapLayer *template6_icon_layer;
static GBitmap *template6_icon_bitmap = NULL;

static AppSync sync;
static uint8_t sync_buffer[256];

enum HubKey {
  HUB1_ACCOUNT_KEY = 0x0,         // TUPLE_INT
  HUB1_VALUE_KEY = 0x1,  		// TUPLE_CSTRING
  HUB2_ACCOUNT_KEY = 0x2,         // TUPLE_INT
  HUB2_VALUE_KEY = 0x3,  		// TUPLE_CSTRING
  HUB3_ACCOUNT_KEY = 0x4,         // TUPLE_INT
  HUB3_VALUE_KEY = 0x5,  		// TUPLE_CSTRING
  HUB4_ACCOUNT_KEY = 0x6,         // TUPLE_INT
  HUB4_VALUE_KEY = 0x7, 		// TUPLE_CSTRING
  HUB5_ACCOUNT_KEY = 0x8,         // TUPLE_INT
  HUB5_VALUE_KEY = 0x9,  		// TUPLE_CSTRING
  HUB6_ACCOUNT_KEY = 0xA,         // TUPLE_INT
  HUB6_VALUE_KEY = 0xB,  		// TUPLE_CSTRING
  VIBRATE_KEY = 0xC,  			  // TUPLE_INT
  PHONE_PERCENT = 0xD,  		  // TUPLE_INT
  CALENDAR_EVENT_NAME = 0xE		// TUPLE_CSTRING
};

static uint32_t ALL_ICONS[] = {
    RESOURCE_ID_CLEAR_DAY,				// 0
	RESOURCE_ID_CLEAR_NIGHT,			// 1
	RESOURCE_ID_CLOUD_ERROR,			// 2
	RESOURCE_ID_CLOUDY,					// 3
	RESOURCE_ID_COLD,					// 4
	RESOURCE_ID_DRIZZLE,				// 5
	RESOURCE_ID_FOG,					// 6
	RESOURCE_ID_HOT,					// 7
	RESOURCE_ID_NOT_AVAILABLE,			// 8
	RESOURCE_ID_PARTLY_CLOUDY_DAY,		// 9
	RESOURCE_ID_PARTLY_CLOUDY_NIGHT,  	// 10
	RESOURCE_ID_PHONE_ERROR,			// 11
	RESOURCE_ID_RAIN,					// 12
	RESOURCE_ID_RAIN_SLEET,				// 13
	RESOURCE_ID_RAIN_SNOW,				// 14
	RESOURCE_ID_SLEET,					// 15
	RESOURCE_ID_SNOW,					// 16
	RESOURCE_ID_SNOW_SLEET,				// 17
	RESOURCE_ID_THUNDER,				// 18
	RESOURCE_ID_WIND,					// 19
	RESOURCE_ID_ICON_BBM,  				// 20
  	RESOURCE_ID_ICON_CALL,				// 21
  	RESOURCE_ID_ICON_EMAIL,				// 22
  	RESOURCE_ID_ICON_FACEBOOK,			// 23
  	RESOURCE_ID_ICON_LINKEDIN,			// 24
  	RESOURCE_ID_ICON_NOTIFICATION,		// 25
  	RESOURCE_ID_ICON_PIN,				// 26
  	RESOURCE_ID_ICON_TEXT,				// 27
  	RESOURCE_ID_ICON_TWITTER,			// 28
  	RESOURCE_ID_ICON_VOICEMAIL,			// 29
  	RESOURCE_ID_ICON_WHATSAPP,			// 30
  	RESOURCE_ID_ICON_EMPTY,				// 31
  	RESOURCE_ID_ICON_BLAQ,				// 32
  	RESOURCE_ID_ICON_CANDID,			// 33
  	RESOURCE_ID_ICON_INSTAGRAM,			// 34
  	RESOURCE_ID_ICON_QUICKPOST,			// 35
  	RESOURCE_ID_ICON_TWITTLY,			// 36
  	RESOURCE_ID_ICON_WECHAT,			// 37
  	RESOURCE_ID_ICON_HG10,				// 38
  	RESOURCE_ID_ICON_GOOGLE_TALK,		// 39
  	RESOURCE_ID_ICON_CALENDAR,			// 40
  	RESOURCE_ID_ICON_VIP				// 41
};

bool was_BTconnected_last_time;

static const VibePattern custom_pattern_1 = {
  .durations = (uint32_t []) {100},
  .num_segments = 1
};  // 1 short vibes

static const VibePattern custom_pattern_2 = {
  .durations = (uint32_t []) {100, 100, 100},
  .num_segments = 3
};  // 2 short vibes

static const VibePattern custom_pattern_3 = {
  .durations = (uint32_t []) {100, 100, 100, 100, 100},
  .num_segments = 5
};  // 3 short vibes

void battery_handler(BatteryChargeState charge) {
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);
	int percent = charge.charge_percent;
	dict_write_int(iter, 0, &percent, sizeof(int), true);
	app_message_outbox_send();
}

static void handle_tick(struct tm *tick_time, TimeUnits units_changed)
{
  if (units_changed & MINUTE_UNIT) {
    // Update the time - Fix to deal with 12 / 24 centering bug
    time_t currentTime = time(0);
    struct tm *currentLocalTime = localtime(&currentTime);

    // Manually format the time as 12 / 24 hour, as specified
    strftime(   time_text, 
                sizeof(time_text), 
                clock_is_24h_style() ? "%R" : "%I:%M", 
                currentLocalTime);

    // Drop the first char of time_text if needed
    if (!clock_is_24h_style() && (time_text[0] == '0')) {
      memmove(time_text, &time_text[1], sizeof(time_text) - 1);
    }

    text_layer_set_text(time_layer, time_text);
  }
  if (units_changed & DAY_UNIT) {
    // Update the date - Without a leading 0 on the day of the month
    char day_text[4];
    strftime(day_text, sizeof(day_text), "%a", tick_time);
    snprintf(date_text, sizeof(date_text), "%s %i", day_text, tick_time->tm_mday);
    text_layer_set_text(date_layer, date_text);
  }
}

void make_vibes_1()  {
	vibes_enqueue_custom_pattern(custom_pattern_1);
}

void make_vibes_2()  {
	vibes_enqueue_custom_pattern(custom_pattern_2);
}

void make_vibes_3()  {
	vibes_enqueue_custom_pattern(custom_pattern_3);
}

void update_configuration(void)
{
    bool show_battery = 1;    /* default to true */

    if (persist_exists(CONFIG_KEY_BATTERY))
    {
        show_battery = persist_read_bool(CONFIG_KEY_BATTERY);
    }
	
	if (show_battery) {
		qtp_setup();
	} else {
		qtp_app_deinit();
	}
}

static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %d", app_message_error);
}

static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
  switch (key) {
    case HUB1_ACCOUNT_KEY: {
        if (strcmp(new_tuple->value->cstring, "off") == 0) {
            persist_write_bool(CONFIG_KEY_BATTERY, false);
			update_configuration();
		}
		else if (strcmp(new_tuple->value->cstring, "on") == 0) {
            persist_write_bool(CONFIG_KEY_BATTERY, true);
			update_configuration();
		}
		else {
			if (template1_icon_bitmap) {
        		gbitmap_destroy(template1_icon_bitmap);
            }
		
            template1_icon_bitmap = gbitmap_create_with_resource(ALL_ICONS[new_tuple->value->uint8]);
            bitmap_layer_set_bitmap(template1_icon_layer, template1_icon_bitmap);
		}
		break;
	}

    case HUB1_VALUE_KEY: {
      // App Sync keeps new_tuple in sync_buffer, so we may use it directly
      text_layer_set_text(template1_value_layer, new_tuple->value->cstring);
      break;
    }
	
    case HUB2_ACCOUNT_KEY: {
		if (template2_icon_bitmap) {
        	gbitmap_destroy(template2_icon_bitmap);
        }

        template2_icon_bitmap = gbitmap_create_with_resource(ALL_ICONS[new_tuple->value->uint8]);
        bitmap_layer_set_bitmap(template2_icon_layer, template2_icon_bitmap);
		break;
	}

    case HUB2_VALUE_KEY: {
      // App Sync keeps new_tuple in sync_buffer, so we may use it directly
      text_layer_set_text(template2_value_layer, new_tuple->value->cstring);
      break;
    }
	  
	case HUB3_ACCOUNT_KEY: {
		if (template3_icon_bitmap) {
        	gbitmap_destroy(template3_icon_bitmap);
        }

        template3_icon_bitmap = gbitmap_create_with_resource(ALL_ICONS[new_tuple->value->uint8]);
        bitmap_layer_set_bitmap(template3_icon_layer, template3_icon_bitmap);
		break;
	}

    case HUB3_VALUE_KEY: {
      // App Sync keeps new_tuple in sync_buffer, so we may use it directly
      text_layer_set_text(template3_value_layer, new_tuple->value->cstring);
      break;
    }
	  
	 case HUB4_ACCOUNT_KEY: {
		if (template4_icon_bitmap) {
        	gbitmap_destroy(template4_icon_bitmap);
        }

        template4_icon_bitmap = gbitmap_create_with_resource(ALL_ICONS[new_tuple->value->uint8]);
        bitmap_layer_set_bitmap(template4_icon_layer, template4_icon_bitmap);
		break;
	}

    case HUB4_VALUE_KEY: {
      // App Sync keeps new_tuple in sync_buffer, so we may use it directly
      text_layer_set_text(template4_value_layer, new_tuple->value->cstring);
      break;
    }

	  	case HUB5_ACCOUNT_KEY: {
		if (template5_icon_bitmap) {
        	gbitmap_destroy(template5_icon_bitmap);
        }

        template5_icon_bitmap = gbitmap_create_with_resource(ALL_ICONS[new_tuple->value->uint8]);
        bitmap_layer_set_bitmap(template5_icon_layer, template5_icon_bitmap);
		break;
	}

    case HUB5_VALUE_KEY: {
      // App Sync keeps new_tuple in sync_buffer, so we may use it directly
      text_layer_set_text(template5_value_layer, new_tuple->value->cstring);
      break;
    }

	  case HUB6_ACCOUNT_KEY: {
		if (template6_icon_bitmap) {
        	gbitmap_destroy(template6_icon_bitmap);
        }

        template6_icon_bitmap = gbitmap_create_with_resource(ALL_ICONS[new_tuple->value->uint8]);
        bitmap_layer_set_bitmap(template6_icon_layer, template6_icon_bitmap);
		break;
	}

    case HUB6_VALUE_KEY: {
      // App Sync keeps new_tuple in sync_buffer, so we may use it directly
      text_layer_set_text(template6_value_layer, new_tuple->value->cstring);
      break;
    }
	  
	case VIBRATE_KEY: {
      // App Sync keeps new_tuple in sync_buffer, so we may use it directly
	  if (new_tuple->value->uint8 == 1) {
		make_vibes_1();
	  }
	  else if (new_tuple->value->uint8 == 2) {
		make_vibes_2();
	  }
	  else if (new_tuple->value->uint8 == 3) {
		make_vibes_3();
	  }
	  else if (new_tuple->value->uint8 > 3) {
		vibes_long_pulse();
	  }
      break;
    }
	  
	case PHONE_PERCENT: {
        qtp_phone_percent = new_tuple->value->uint8;
		break;
	}
	  
	case CALENDAR_EVENT_NAME: {
		strncpy(qtp_calendar_events, new_tuple->value->cstring, 100);
      	break;
    }
	  
  } // End switch (key)
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);

  time_layer = text_layer_create(TIME_FRAME);
  text_layer_set_text_color(time_layer, GColorWhite);
  text_layer_set_background_color(time_layer, GColorBlack);
  text_layer_set_font(time_layer, font_time);
  text_layer_set_text_alignment(time_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(time_layer));

  date_layer = text_layer_create(DATE_FRAME);
  text_layer_set_text_color(date_layer, GColorWhite);
  text_layer_set_background_color(date_layer, GColorBlack);
  text_layer_set_font(date_layer, font_date);
  text_layer_set_text_alignment(date_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(date_layer));

  template1_icon_layer = bitmap_layer_create(GRect(10, 90, 26, 26));
  layer_add_child(window_layer, bitmap_layer_get_layer(template1_icon_layer));

  template1_value_layer = text_layer_create(GRect(36, 90, 41, 26));
  text_layer_set_text_color(template1_value_layer, GColorBlack);
  text_layer_set_background_color(template1_value_layer, GColorWhite);
  text_layer_set_font(template1_value_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(template1_value_layer, GTextAlignmentCenter);
  text_layer_set_text(template1_value_layer, template1_value);
  layer_add_child(window_layer, text_layer_get_layer(template1_value_layer));

  template2_icon_layer = bitmap_layer_create(GRect(77, 90, 26, 26));
  layer_add_child(window_layer, bitmap_layer_get_layer(template2_icon_layer));

  template2_value_layer = text_layer_create(GRect(103, 90, 41, 26));
  text_layer_set_text_color(template2_value_layer, GColorBlack);
  text_layer_set_background_color(template2_value_layer, GColorWhite);
  text_layer_set_font(template2_value_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(template2_value_layer, GTextAlignmentCenter);
  text_layer_set_text(template2_value_layer, template2_value);
  layer_add_child(window_layer, text_layer_get_layer(template2_value_layer));

  template3_icon_layer = bitmap_layer_create(GRect(10, 116, 26, 26));
  layer_add_child(window_layer, bitmap_layer_get_layer(template3_icon_layer));

  template3_value_layer = text_layer_create(GRect(36, 116, 41, 26));
  text_layer_set_text_color(template3_value_layer, GColorBlack);
  text_layer_set_background_color(template3_value_layer, GColorWhite);
  text_layer_set_font(template3_value_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(template3_value_layer, GTextAlignmentCenter);
  text_layer_set_text(template3_value_layer, template3_value);
  layer_add_child(window_layer, text_layer_get_layer(template3_value_layer));

  template4_icon_layer = bitmap_layer_create(GRect(77, 116, 26, 26));
  layer_add_child(window_layer, bitmap_layer_get_layer(template4_icon_layer));

  template4_value_layer = text_layer_create(GRect(103, 116, 41, 26));
  text_layer_set_text_color(template4_value_layer, GColorBlack);
  text_layer_set_background_color(template4_value_layer, GColorWhite);
  text_layer_set_font(template4_value_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(template4_value_layer, GTextAlignmentCenter);
  text_layer_set_text(template4_value_layer, template4_value);
  layer_add_child(window_layer, text_layer_get_layer(template4_value_layer));

  template5_icon_layer = bitmap_layer_create(GRect(10, 142, 26, 26));
  layer_add_child(window_layer, bitmap_layer_get_layer(template5_icon_layer));

  template5_value_layer = text_layer_create(GRect(36, 142, 41, 26));
  text_layer_set_text_color(template5_value_layer, GColorBlack);
  text_layer_set_background_color(template5_value_layer, GColorWhite);
  text_layer_set_font(template5_value_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(template5_value_layer, GTextAlignmentCenter);
  text_layer_set_text(template5_value_layer, template5_value);
  layer_add_child(window_layer, text_layer_get_layer(template5_value_layer));

  template6_icon_layer = bitmap_layer_create(GRect(77, 142, 26, 26));
  layer_add_child(window_layer, bitmap_layer_get_layer(template6_icon_layer));

  template6_value_layer = text_layer_create(GRect(103, 142, 41, 26));
  text_layer_set_text_color(template6_value_layer, GColorBlack);
  text_layer_set_background_color(template6_value_layer, GColorWhite);
  text_layer_set_font(template6_value_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(template6_value_layer, GTextAlignmentCenter);
  text_layer_set_text(template6_value_layer, template6_value);
  layer_add_child(window_layer, text_layer_get_layer(template6_value_layer));

	Tuplet initial_values[] = {
    TupletInteger(HUB1_ACCOUNT_KEY, (uint8_t) 31),
    TupletCString(HUB1_VALUE_KEY, "1"),
    TupletInteger(HUB2_ACCOUNT_KEY, (uint8_t) 31),
    TupletCString(HUB2_VALUE_KEY, "2"),
    TupletInteger(HUB3_ACCOUNT_KEY, (uint8_t) 31),
    TupletCString(HUB3_VALUE_KEY, "3"),
    TupletInteger(HUB4_ACCOUNT_KEY, (uint8_t) 31),
    TupletCString(HUB4_VALUE_KEY, "4"),
    TupletInteger(HUB5_ACCOUNT_KEY, (uint8_t) 31),
    TupletCString(HUB5_VALUE_KEY, "5"),
    TupletInteger(HUB6_ACCOUNT_KEY, (uint8_t) 31),
    TupletCString(HUB6_VALUE_KEY, "6"),
	TupletInteger(VIBRATE_KEY, (uint8_t) 0),
	TupletInteger(PHONE_PERCENT, (uint8_t) 100),
	TupletCString(CALENDAR_EVENT_NAME, "")
  };
  app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values, ARRAY_LENGTH(initial_values),
      sync_tuple_changed_callback, sync_error_callback, NULL);
}

static void window_unload(Window *window) {
  app_sync_deinit(&sync);

  text_layer_destroy(time_layer);
  text_layer_destroy(date_layer);

  if (template1_icon_bitmap) {
    gbitmap_destroy(template1_icon_bitmap);
  }
  text_layer_destroy(template1_value_layer);
  bitmap_layer_destroy(template1_icon_layer);

  if (template2_icon_bitmap) {
    gbitmap_destroy(template2_icon_bitmap);
  }
  text_layer_destroy(template2_value_layer);
  bitmap_layer_destroy(template2_icon_layer);

  if (template3_icon_bitmap) {
    gbitmap_destroy(template3_icon_bitmap);
  }
  text_layer_destroy(template3_value_layer);
  bitmap_layer_destroy(template3_icon_layer);

  if (template4_icon_bitmap) {
    gbitmap_destroy(template4_icon_bitmap);
  }
  text_layer_destroy(template4_value_layer);
  bitmap_layer_destroy(template4_icon_layer);

  if (template5_icon_bitmap) {
    gbitmap_destroy(template5_icon_bitmap);
  }
  text_layer_destroy(template5_value_layer);
  bitmap_layer_destroy(template5_icon_layer);

  if (template6_icon_bitmap) {
    gbitmap_destroy(template6_icon_bitmap);
  }
  text_layer_destroy(template6_value_layer);
  bitmap_layer_destroy(template6_icon_layer);
}

void bluetooth_handler(bool connected) {
	//	This handler is called when BT connection state changes

	// Destroy inverter layer if BT changed from disconnected to connected
	if ((connected) && (!(was_BTconnected_last_time)))  {
		inverter_layer_destroy(inv_layer);
	}
	window_set_background_color(window, GColorWhite);
	//Inverter layer in case of disconnect
	if (!(connected)) {
		inv_layer = inverter_layer_create(GRect(0, 0, 144, 168));
		layer_add_child(window_get_root_layer(window), (Layer*) inv_layer);	
		vibes_double_pulse();
	}
	was_BTconnected_last_time = connected;
}

static void init() {
  window = window_create();
  window_set_background_color(window, GColorWhite);
//  font_date = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FUTURA_18));
  font_date = fonts_get_system_font(FONT_KEY_GOTHIC_28);
  font_time = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FUTURA_CONDENSED_53));
  window_set_fullscreen(window, true);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });

  const int inbound_size = 256;
  const int outbound_size = 16;
  app_message_open(inbound_size, outbound_size);

  const bool animated = true;
  window_stack_push(window, animated);

  // Update the screen right away
  time_t now = time(NULL);
  handle_tick(localtime(&now), MINUTE_UNIT | DAY_UNIT );
  // And then every minute
  tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);
	
  was_BTconnected_last_time = bluetooth_connection_service_peek();
  bluetooth_handler(was_BTconnected_last_time);
  bluetooth_connection_service_subscribe( bluetooth_handler );
  update_configuration();
	
  battery_state_service_subscribe(&battery_handler);
  battery_handler(battery_state_service_peek());
}

static void deinit() {
  battery_state_service_unsubscribe();
  window_destroy(window);
  tick_timer_service_unsubscribe();
  fonts_unload_custom_font(font_time);
	
  // Destroy inverter if last screen was inverted
  if (!(was_BTconnected_last_time))  {
	inverter_layer_destroy(inv_layer);
  }
  bluetooth_connection_service_unsubscribe();
}

int main(void) {
  setlocale(LC_ALL, "");
  qtp_conf = QTP_K_AUTOHIDE | QTP_K_INVERT;
  init();
  app_event_loop();
  deinit();
}
