#include <pebble.h>
#include "timeout.h"
#include "storage.h"
#include "localize.h"
#include "debug.h"

#define MENU_ROW_COUNT 5

#define MENU_ROW_KONAMI     0
#define MENU_ROW_VIBRATION     4
#define MENU_ROW_DURATION      3
#define MENU_ROW_AUTO_SNOOZE   2
#define MENU_ROW_DISMISS       1
//#define MENU_ROW_SMART         5

static void window_load(Window* window);
static void window_unload(Window* window);
static void window_appear(Window* window);
static uint16_t menu_num_sections(struct MenuLayer* menu, void* callback_context);
static uint16_t menu_num_rows(struct MenuLayer* menu, uint16_t section_index, void* callback_context);
static int16_t menu_cell_height(struct MenuLayer *menu, MenuIndex *cell_index, void *callback_context);
static int16_t menu_header_height(struct MenuLayer *menu, uint16_t section_index, void *callback_context);
static void menu_draw_row(GContext* ctx, const Layer* cell_layer, MenuIndex* cell_index, void* callback_context);
static void menu_select(struct MenuLayer* menu, MenuIndex* cell_index, void* callback_context);
static void menu_selection_changed(struct MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *callback_context);
static void menu_draw_header(GContext* ctx, const Layer* cell_layer, uint16_t section_index, void* callback_context);
static void scroll_timer_callback(void* data);

static Window*    s_window;
static MenuLayer* s_menu;
static int s_konami_dismiss;
static int s_vibration_pattern;

//static bool s_flip_to_snooze;
static int s_vibration_duration;
static bool s_auto_snooze;
//static bool s_background_tracking;
static bool s_dismiss_top;

static int16_t s_scroll_index;
static int16_t s_scroll_row_index;
static AppTimer *s_scroll_timer;

void win_advanced_init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
    .appear = window_appear
  });
  s_konami_dismiss = load_persistent_storage_int(KONAMI_DISMISS_KEY,0);
  APP_LOG(APP_LOG_LEVEL_DEBUG,"key: %d",s_konami_dismiss);
  s_vibration_pattern = load_persistent_storage_int(VIBRATION_PATTERN_KEY,0);
  //s_flip_to_snooze = load_persistent_storage_bool(FLIP_TO_SNOOZE_KEY, false);
  s_vibration_duration = load_persistent_storage_int(VIBRATION_DURATION_KEY,2);
  s_auto_snooze = load_persistent_storage_bool(AUTO_SNOOZE_KEY,true);
  s_dismiss_top = load_persistent_storage_bool(TOP_BUTTON_DISMISS_KEY,true);
//#ifdef PBL_HEALTH
//  s_smart_alarm = load_persistent_storage_int(SMART_ALARM_KEY,0);
//#endif
  //s_background_tracking = load_persistent_storage_bool(BACKGROUND_TRACKING_KEY,false);
  refresh_timeout();
  s_scroll_timer = app_timer_register(500,scroll_timer_callback,NULL);
}

void win_advanced_show(void) {
  window_stack_push(s_window, true);
}

static void window_load(Window* window) {
  Layer *window_layer = window_get_root_layer(s_window);
  GRect bounds = layer_get_frame(window_layer);

  // Create the menu layer
  s_menu = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks) {
    .get_num_sections = menu_num_sections,
    .get_num_rows = menu_num_rows,
    .get_cell_height = menu_cell_height,
    .draw_row = menu_draw_row,
    .select_click = menu_select,
    .get_header_height = menu_header_height,
    .draw_header = menu_draw_header,
    .selection_changed = menu_selection_changed,
  });
  // Bind the menu layer's click config provider to the window for interactivity
  menu_layer_set_click_config_onto_window(s_menu, s_window);

#ifdef PBL_COLOR
  menu_layer_pad_bottom_enable(s_menu,false);
  menu_layer_set_highlight_colors(s_menu,GColorBlue,GColorWhite);
#endif
#ifdef PBL_ROUND
  menu_layer_set_center_focused(s_menu,true);
#endif
  // Add it to the window for display
  layer_add_child(window_layer, menu_layer_get_layer(s_menu));

}

static void window_unload(Window* window) {
  menu_layer_destroy(s_menu);
}

static void window_appear(Window* window) {
  layer_mark_dirty(menu_layer_get_layer(s_menu));
}

static uint16_t menu_num_sections(struct MenuLayer* menu, void* callback_context) {
  return 1;
}

static uint16_t menu_num_rows(struct MenuLayer* menu, uint16_t section_index, void* callback_context) {
  return MENU_ROW_COUNT;
}

static int16_t menu_cell_height(struct MenuLayer *menu, MenuIndex *cell_index, void *callback_context) {
#ifdef PBL_ROUND
  if(menu_layer_is_index_selected(menu,cell_index))
    return MENU_CELL_ROUND_FOCUSED_TALL_CELL_HEIGHT;
  else
    return MENU_CELL_ROUND_UNFOCUSED_SHORT_CELL_HEIGHT;
#else
#ifdef PBL_PLATFORM_EMERY
  return 54;
#else
  return 38;
#endif
#endif
}

static int16_t menu_header_height(struct MenuLayer *menu, uint16_t section_index, void *callback_context) {
#ifdef PBL_ROUND
  return 0;
#else
  return 16;
#endif
}

static void menu_draw_header(GContext* ctx, const Layer* cell_layer, uint16_t section_index, void* callback_context) {
#ifndef PBL_ROUND
    GRect bounds = layer_get_bounds(cell_layer);
    graphics_context_set_text_color(ctx, GColorWhite);
    #ifdef PBL_COLOR
      graphics_context_set_fill_color(ctx, GColorBlue);
    #else
      graphics_context_set_fill_color(ctx, GColorBlack);
    #endif
    graphics_fill_rect(ctx,GRect(0,1,bounds.size.w,14),0,GCornerNone);

    graphics_draw_text(ctx, _("Options"),
                       fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(0, -2, bounds.size.w, 14), GTextOverflowModeWordWrap,
                       GTextAlignmentCenter, NULL);
#endif
}


static void menu_cell_animated_draw(GContext* ctx, const Layer* cell_layer, char* text, char* subtext, bool animate)
{
  if(animate && s_scroll_index>0)
  {
    if(((int16_t)strlen(text)-15-s_scroll_index)>0)
    text+=s_scroll_index;
  }
  menu_cell_basic_draw(ctx,cell_layer,text,subtext,NULL);
}

static void menu_draw_row(GContext* ctx, const Layer* cell_layer, MenuIndex* cell_index, void* callback_context) {
  bool animate = cell_index->row==s_scroll_row_index;
  char* text = NULL;
  char* subtext = NULL;
  switch (cell_index->row) {
    case MENU_ROW_KONAMI:
      text = _("Dismiss Alarm");
      switch (s_konami_dismiss) {
        case 0:
        subtext =  "Easy";
          break;
        case 1:
        subtext = "Hard";
          break;
        case 2:
        subtext = "Medium";
          break;
      }
      break;
    case MENU_ROW_DISMISS:
      text = _("Button to Dismiss Alarm");
      subtext = s_dismiss_top?_("Top"):_("Bottom");
      break;
    case MENU_ROW_VIBRATION:
    text = _("Vibration Strength");
    switch (s_vibration_pattern) {
      case 0:
      subtext =  _("Constant");
        break;
      case 1:
      subtext = _("Increasing 10s");
        break;
      case 2:
      subtext = _("Increasing 20s");
        break;
      case 3:
      subtext = _("Increasing 30s");
        break;
      default:
        break;
    }
    break;
    case MENU_ROW_AUTO_SNOOZE:
    text = _("Snooze after Vibration End");
    subtext = s_auto_snooze?_("ON"):_("OFF");
      break;
    case MENU_ROW_DURATION:
      text = _("Vibration Duration");
    switch (s_vibration_duration) {
      case 0:
      subtext = _("30 seconds");
      break;
      case 1:
      subtext = _("1 minute");
      break;
      case 2:
      subtext = _("2 minutes");
      break;
      case 3:
      subtext = _("5 minutes");
      break;
      case 4:
      subtext = _("2 seconds");
      break;
      case 5:
      subtext = _("10 seconds");
      break;
    }
      break;
//#ifdef PBL_HEALTH
//    case MENU_ROW_SMART:
//        text = "Smart Alarm";
//        switch (s_smart_alarm) {
//          case 0:
//            subtext = "Off";
//            break;
//          case 1:
//            subtext = "10 min";
//            break;
//          case 2:
//            subtext = "20 min";
//            break;
//          case 3:
//            subtext = "30 min";
//            break;
//          default:
//            break;
//        }
//      default:
//      break;
//#endif
  }
  menu_cell_animated_draw(ctx, cell_layer, text, subtext, animate);

}

static void scroll_timer_callback(void *data)
{
  s_scroll_index+=3;
  if(s_scroll_row_index>0)
  layer_mark_dirty(menu_layer_get_layer(s_menu));
  s_scroll_timer = app_timer_register(1000,scroll_timer_callback,NULL);
}

static void menu_selection_changed(struct MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *callback_context)
{
  s_scroll_index=0;
  s_scroll_row_index = new_index.row;
  app_timer_reschedule(s_scroll_timer,1000);
  refresh_timeout();
}

static void menu_select(struct MenuLayer* menu, MenuIndex* cell_index, void* callback_context) {
  switch (cell_index->row) {
    case MENU_ROW_KONAMI:
      s_konami_dismiss++;
      if(s_konami_dismiss>2)
        s_konami_dismiss=0;
      persist_write_int(KONAMI_DISMISS_KEY,s_konami_dismiss);
      break;
    case MENU_ROW_DISMISS:
      s_dismiss_top=!s_dismiss_top;
      persist_write_bool(TOP_BUTTON_DISMISS_KEY,s_dismiss_top);
      break;
    case MENU_ROW_AUTO_SNOOZE:
      s_auto_snooze=!s_auto_snooze;
      persist_write_bool(AUTO_SNOOZE_KEY,s_auto_snooze);
      break;
    case MENU_ROW_VIBRATION:
      s_vibration_pattern++;
      if(s_vibration_pattern>3)
        s_vibration_pattern=0;
      persist_write_int(VIBRATION_PATTERN_KEY,s_vibration_pattern);
      break;
    case MENU_ROW_DURATION:
      s_vibration_duration++;
      if(s_vibration_duration>5)
        s_vibration_duration=0;
      persist_write_int(VIBRATION_DURATION_KEY,s_vibration_duration);
    break;
//#ifdef PBL_HEALTH
//    case MENU_ROW_SMART:
//      s_smart_alarm++;
//      if(s_smart_alarm>3)
//        s_smart_alarm=0;
//      persist_write_int(SMART_ALARM_KEY,s_smart_alarm);
//      break;
//#endif
  }
  refresh_timeout();
  layer_mark_dirty(menu_layer_get_layer(menu));
}
