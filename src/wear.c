	/*
 * Pebble Wear
 * Richard Gilbert
 */

#include "pebble.h"
	
#define CACHESIZE 5 //assumed odd, card_update array must be updated accordingly
#define ROWSIZE 20 // 144 pixels / 8 bits per byte = 18 bytes per row, must be multiple of 4
#define IMAGESIZE 3360 // ROWSIZE * 168
#define ICONROWSIZE 8 // 48 pixels / 8 = 6
#define ICONSIZE 384
#define TITLESIZE 100
#define SUMMARYSIZE 100
#define TEXTSIZE 200

static Window* window;

//layers
static Layer* card[CACHESIZE];
static Layer* back[CACHESIZE];
static Layer* watchface;
static TextLayer* actions_layer;

//animation
static PropertyAnimation* card_anim[CACHESIZE];
static PropertyAnimation* back_anim[CACHESIZE];
//static PropertyAnimation* watchface_anim;

//bitmap structures
static GBitmap card_icon[CACHESIZE];
static GBitmap back_image[CACHESIZE];

//image data
static uint8_t icon_data[ICONSIZE * CACHESIZE];
static uint8_t image_data[IMAGESIZE * CACHESIZE];

//strings
static char content_title[TITLESIZE * CACHESIZE];
static char content_text[TEXTSIZE * CACHESIZE];
static char currenttime[10];
static char actions[20 * 3];

//counters
static int current = -1;//WATCHFACE
static int count = 0;

enum { //DICTIONARY KEYS
	 COMMAND,
	 BYTES,
	 LINE,
	 ID
     };

enum { //command types
	CLEAR,
	UPDATETEXT,
	UPDATEICON,
	UPDATEIMAGE,
	MOVE,
	VIEW,
	REPORT,
	ACTIONS
	};


//BACKGROUND DRAWING
static void draw_back(Layer *back, GContext* ctx, int backNo) {
	GRect bounds = ((GBitmap*)&back_image[backNo])->bounds;
	graphics_draw_bitmap_in_rect(ctx, &back_image[backNo], (GRect) { .origin = { 0, 0 }, .size = bounds.size });
}

//CARD DRAWING
static void draw_card(Layer *card, GContext* ctx, int cardNo){
	
	//get title content size
	GSize title_size = graphics_text_layout_get_content_size(&content_title[cardNo * TITLESIZE], 
															 fonts_get_system_font(FONT_KEY_GOTHIC_18), 
															 GRect(0,0,142-54,168), 
															 GTextOverflowModeWordWrap, GTextAlignmentLeft);
	//get text content size
	GSize text_size = graphics_text_layout_get_content_size(&content_text[cardNo * TEXTSIZE], 
															fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), 
															GRect(0,0,142,168), 
															GTextOverflowModeWordWrap, GTextAlignmentLeft);
	//total card height
	int card_height = title_size.h + text_size.h;	
	
	//card
	graphics_context_set_fill_color(ctx, GColorWhite );
	graphics_fill_rect(ctx, GRect(0, 168-card_height, 144, card_height), 0, GCornerNone);
	
	//box
	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_rect(ctx, GRect(144-54, 168-card_height-26, 52, 52), 3, GCornersAll);
	
	//icon
	//GRect bounds = card_icon[cardNo]->bounds;
	//graphics_draw_bitmap_in_rect(ctx, card_icon[cardNo], (GRect) { .origin = { 144-52, height-card_height-24 }, .size = bounds.size });
	
	graphics_context_set_text_color(ctx, GColorBlack);	
	
	//draw title content
	graphics_draw_text(ctx, 
					   &content_title[cardNo * TITLESIZE],  
					   fonts_get_system_font(FONT_KEY_GOTHIC_18),
					   GRect( 2, 168-card_height, 142-54, title_size.h ),
					   GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
	//draw text content
	graphics_draw_text(ctx, 
					   &content_text[cardNo * TEXTSIZE],  
					   fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), 
					   GRect(2, 168-text_size.h-4, 142, text_size.h), 
					   GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
}

static void draw_watchface(Layer *me, GContext* ctx){
	graphics_context_set_fill_color(ctx, GColorBlack );
	graphics_fill_rect(ctx, GRect(0, 0, 144, 168), 0, GCornerNone);
	
	clock_copy_time_string(currenttime, 10);
	
	graphics_context_set_text_color(ctx, GColorWhite);	
	graphics_draw_text(ctx, 
					   currenttime,  
					   fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD),
					   GRect( 2, 2, 140, 164),
					   GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
}



//INDIVIDUAL CARD FUNCTIONS
static void card_update_0(Layer *me, GContext* ctx) {	draw_card(me, ctx, 0);	}
static void card_update_1(Layer *me, GContext* ctx) {	draw_card(me, ctx, 1);	}
static void card_update_2(Layer *me, GContext* ctx) {	draw_card(me, ctx, 2);	}
static void card_update_3(Layer *me, GContext* ctx) {	draw_card(me, ctx, 3);	}
static void card_update_4(Layer *me, GContext* ctx) {	draw_card(me, ctx, 4);	}

static void back_update_0(Layer *me, GContext* ctx) {	draw_back(me, ctx, 0);	}
static void back_update_1(Layer *me, GContext* ctx) {	draw_back(me, ctx, 1);	}
static void back_update_2(Layer *me, GContext* ctx) {	draw_back(me, ctx, 2);	}
static void back_update_3(Layer *me, GContext* ctx) {	draw_back(me, ctx, 3);	}
static void back_update_4(Layer *me, GContext* ctx) {	draw_back(me, ctx, 4);	}

static void (*card_update[CACHESIZE]) (Layer *me, GContext* ctx) = {card_update_0,card_update_1,card_update_2,card_update_3,card_update_4};
static void (*back_update[CACHESIZE]) (Layer *me, GContext* ctx) = {back_update_0,back_update_1,back_update_2,back_update_3,back_update_4};


//IMAGE DATA RESET
void blank_image_data(int image_number){
	//for each row
	for(int row = 0; row < 168; row++){
		//for each byte
		for(int col = 0; col < ROWSIZE; col++){
			//pointer to current byte, easier than typing out this string every time
			uint8_t* current_byte = &image_data[(image_number * IMAGESIZE) + (row * ROWSIZE) + col]; 
			*current_byte = 0;
			
			//for each bit
			for(int bit = 0; bit < 8; bit++){					
				*current_byte += ((row % 2) + (col * 8 + bit)) % 2; //add 1 or 0 (in checkerboard pattern)

				//if not last bit
				if(bit != 7)
					*current_byte <<= 1;//shift bits
			}
		}
	}
}

//CARD DATA RESET
void reset_card(int card_number)
{
	blank_image_data(card_number);
	
	strcpy(&content_title[card_number * TITLESIZE], "Loading");	
	strcpy(&content_text[card_number * TEXTSIZE], "");	
}

//load a card as [card_no % CACHESIZE] (scroll cycle)
void load_new_card(int card_no, bool above){
	if(above){
		//reinsert background (for layering)
		//layer_remove_from_parent(back[card_no % CACHESIZE]);
		//layer_insert_below_sibling(back[card_no % CACHESIZE], back[((card_no+1)%CACHESIZE)]);
		
		layer_set_frame(card[card_no % CACHESIZE], GRect(0,-168,144,168));
		layer_set_frame(back[card_no % CACHESIZE], GRect(0, -84, 144, 168));
		
	}else{
		//reinsert background (for layering)
		//layer_remove_from_parent(back[card_no % CACHESIZE]);
		//if(card_no == 0)
		//	layer_insert_above_sibling(back[card_no % CACHESIZE], watchface);
		//else
		//	layer_insert_above_sibling(back[card_no % CACHESIZE], back[((card_no-1)%CACHESIZE)]);
		
		layer_set_frame(card[card_no % CACHESIZE], GRect(0,168,144,168));
		layer_set_frame(back[card_no % CACHESIZE], GRect(0, 168, 144, 168));
	}
}

void load_fake_cards(){
	count = 3;
	for(int i = 0; i < CACHESIZE; i++){
		load_new_card(i, false);
	}
}

void animate(bool up)
{
	GRect from_frame;
	GRect to_frame;
	
	int new = current+1;
	if(up)
		new = current-1;
	
	//cleanup/memory management (things are not destroyed automatically)
	animation_unschedule((Animation*)back_anim[current%CACHESIZE]);
	animation_unschedule((Animation*)card_anim[current%CACHESIZE]);
	if(new!=-1){
	animation_unschedule((Animation*)back_anim[new%CACHESIZE]);
	animation_unschedule((Animation*)card_anim[new%CACHESIZE]);
	}
	if(current != -1){
		if(back_anim[current%CACHESIZE] != NULL)
			property_animation_destroy(back_anim[current%CACHESIZE]);
		if(card_anim[current%CACHESIZE] != NULL)
			property_animation_destroy(card_anim[current%CACHESIZE]);
	}
	if(new!=-1){
	if(back_anim[new%CACHESIZE] != NULL)
		property_animation_destroy(back_anim[new%CACHESIZE]);
	if(card_anim[new%CACHESIZE] != NULL)
		property_animation_destroy(card_anim[new%CACHESIZE]);
	}
	
	
	//layer_remove_from_parent(back[card_no % CACHESIZE]);
	//layer_insert_below_sibling(back[card_no % CACHESIZE], back[((card_no+1)%CACHESIZE)]);
	
	
	if(current!=-1){
		//move current background
		from_frame = layer_get_frame(back[current%CACHESIZE]); //grab the current position
		if(!up)
			to_frame = GRect(0, -124, 144, 0);
		else
			to_frame = GRect(0,144,144,168);

		back_anim[current%CACHESIZE] = property_animation_create_layer_frame(back[current%CACHESIZE], &from_frame, &to_frame);
		animation_set_curve((Animation*) back_anim[current%CACHESIZE], AnimationCurveLinear);
		animation_set_duration((Animation*) back_anim[current%CACHESIZE],300);
		animation_schedule((Animation*) back_anim[current%CACHESIZE]);


		//move current card
		from_frame = layer_get_frame(card[current%CACHESIZE]); //grab the current position
		if(!up)
			to_frame = GRect(0, -168, 144, 168);
		else
			to_frame = GRect(0,168,144,168);

		card_anim[current%CACHESIZE] = property_animation_create_layer_frame(card[current%CACHESIZE], &from_frame, &to_frame);
		animation_set_curve((Animation*) card_anim[current%CACHESIZE], AnimationCurveLinear);
		animation_set_duration((Animation*) card_anim[current%CACHESIZE],300);
		animation_schedule((Animation*) card_anim[current%CACHESIZE]);
		
	}else{
		//from_frame = layer_get_frame(watchface);
		//to_frame = GRect(0,-168,144,168);
		//watchface_anim = property_animation_create_layer_frame(watchface, &from_frame, &to_frame);
		//animation_schedule((Animation*) watchface_anim);
	}
	
	if(!(current==0 && up))
	{
		//move new background
		from_frame = layer_get_frame(back[new%CACHESIZE]); //grab the current position
		to_frame = GRect(0 ,0 ,144, 168);

		back_anim[new%CACHESIZE] = property_animation_create_layer_frame(back[new%CACHESIZE], &from_frame, &to_frame);
		animation_set_curve((Animation*) back_anim[new%CACHESIZE], AnimationCurveEaseOut);
		animation_set_duration((Animation*) back_anim[new%CACHESIZE],300);
		animation_schedule((Animation*) back_anim[new%CACHESIZE]);


		//move new card
		from_frame = layer_get_frame(card[new%CACHESIZE]); //grab the current position

		card_anim[new%CACHESIZE] = property_animation_create_layer_frame(card[new%CACHESIZE], &from_frame, &to_frame);
		animation_set_curve((Animation*) card_anim[new%CACHESIZE], AnimationCurveEaseOut);
		animation_set_duration((Animation*) card_anim[new%CACHESIZE],300);
		animation_schedule((Animation*) card_anim[new%CACHESIZE]);
	}else{
		//from_frame = layer_get_frame(watchface);
		//to_frame = GRect(0,-168,144,168);
		//watchface_anim = property_animation_create_layer_frame(watchface, &from_frame, &to_frame);
		//animation_schedule((Animation*) watchface_anim);
	}
}

void up_single_click_handler(ClickRecognizerRef recognizer, void *context) {
	if(current > -1){
		animate(true);
		current--;
		//if(current > CACHESIZE / 2 + 1)
			//load_new_card(current - CACHESIZE / 2, true);
	}
}

void down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
	if(current < count){
		animate(false);
		current++;
		//if( current > CACHESIZE / 2)
			//load_new_card(current + CACHESIZE / 2, false);
	}
}

void back_single_click_handler(ClickRecognizerRef recognizer, void *context) {
	DictionaryIterator *iter;
 	app_message_outbox_begin(&iter);
	Tuplet value = TupletInteger(1, 1);
	dict_write_tuplet(iter, &value);
	Tuplet value2 = TupletInteger(2, current);
	dict_write_tuplet(iter, &value2);
	app_message_outbox_send();
}

void select_single_click_handler(ClickRecognizerRef recognizer, void *context) {
	Window* actionWindow = window_create();
	window_stack_push(actionWindow, true);
	actions_layer = text_layer_create(GRect(0,0,144,168));
	text_layer_set_text(actions_layer, actions);
	layer_add_child(window_get_root_layer(actionWindow), text_layer_get_layer(actions_layer));
						
	DictionaryIterator *iter;
 	app_message_outbox_begin(&iter);
	Tuplet value = TupletInteger(1, 2);
	dict_write_tuplet(iter, &value);
	Tuplet value2 = TupletInteger(2, current);
	dict_write_tuplet(iter, &value2);
	app_message_outbox_send();
}

//CLICK ASSIGNMENT
void config_provider(Window *window) {
	window_single_click_subscribe(BUTTON_ID_DOWN, down_single_click_handler);
	window_single_click_subscribe(BUTTON_ID_UP, up_single_click_handler);
	//window_single_click_subscribe(BUTTON_ID_BACK, back_single_click_handler);
	//window_single_click_subscribe(BUTTON_ID_SELECT, select_single_click_handler);
}


void in_received_handler(DictionaryIterator *iter, void *context) {

	//vibes_short_pulse();
	
	
	Tuple *tuple_pointer = dict_find(iter,ID);
	int id = tuple_pointer->value->int32;
	
	tuple_pointer = NULL;
	
	tuple_pointer = dict_find(iter, COMMAND);
	if(tuple_pointer)
	{
		if(tuple_pointer->value->int8 == CLEAR)
		{
				reset_card(id);
		}
		else if(tuple_pointer->value->int8 == UPDATETEXT)
		{
			tuple_pointer = NULL;
			tuple_pointer = dict_find(iter, BYTES);
			if(tuple_pointer)
			{				
				uint8_t* bytes = tuple_pointer->value->data;
				
				char string[21];
				for(int i=0;i<20;i++){
					content_title[i + id*TITLESIZE] = bytes[i];
				}
				for(int i=0; i<40;i++)
				{
					content_text[i+ id*TEXTSIZE] = bytes[20+i];
				}
				content_title[20+ id*TITLESIZE] = '\0';
				content_text[40+ id*TEXTSIZE] = '\0';
				
				layer_mark_dirty(card[id]);
			}
			
		
		}
		else if(tuple_pointer->value->int8 == UPDATEICON)
		{
			tuple_pointer = NULL;
			tuple_pointer = dict_find(iter, BYTES);
			if (tuple_pointer) 
			{
				uint8_t* byteArray = tuple_pointer->value->data;
				
				
				Tuple *tuple_row = dict_find(iter, LINE);
				int row = tuple_row->value->int32;

				//icon_data[0] = 0;
				//icon_data[0] = byteArray[0];
				for(int i =0; i < 6; i++)
				{
					//icon_data[i + (64/8)*row] = byteArray[i];
				}
				layer_mark_dirty(card[id]);
			}
		}
		else if(tuple_pointer->value->int8 == UPDATEIMAGE)
		{
			tuple_pointer = NULL;
			tuple_pointer = dict_find(iter, BYTES);
			if (tuple_pointer) 
			{
				uint8_t* byteArray = tuple_pointer->value->data;
				
				Tuple *tuple_row = dict_find(iter, LINE);
				int row = tuple_row->value->int32;

				for(int i =0; i < 18; i++)
				{
					image_data[i + (160/8)*row + id*IMAGESIZE] = byteArray[i];
				}
				layer_mark_dirty(back[id]);
			}
		}
		else if(tuple_pointer->value->int8 == ACTIONS)
		{
			tuple_pointer = NULL;
			tuple_pointer = dict_find(iter, BYTES);
			if(tuple_pointer)
			{				
				uint8_t* bytes = tuple_pointer->value->data;
				
				for(int i=0;i<30;i++){
					actions[i] = bytes[i];
				}
				text_layer_set_text(actions_layer, actions);
			}
		}
	}
}


 void out_sent_handler(DictionaryIterator *sent, void *context) {
   // outgoing message was delivered
 }
 void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
   // outgoing message failed
 }
 void in_dropped_handler(AppMessageResult reason, void *context) {
   // incoming message dropped
 }

void tick(struct tm *tick_time, TimeUnits units_changed)
{
	layer_mark_dirty(watchface);
}


//INIT
void init(){
	
	//window
	window = window_create();
	window_set_fullscreen(window, true);
	window_stack_push(window, true);
	
	Layer* window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_frame(window_layer);
	
	//buttons
	window_set_click_config_provider(window, (ClickConfigProvider) config_provider);
	tick_timer_service_subscribe(MINUTE_UNIT, (TickHandler) tick);
	
	watchface = layer_create(bounds);
	layer_set_update_proc(watchface, draw_watchface);
	
	
	//layers and bitmaps
	for(int i=0; i<CACHESIZE; i++){
		
		card_anim[i] = NULL;
		back_anim[i] = NULL;
		
		//create layers
		card[i] = layer_create(bounds);
		layer_set_update_proc(card[i], card_update[i]);
		//layer_add_child(window_layer, card[i]);
		
		back[i] = layer_create(bounds);
		layer_set_update_proc(back[i], back_update[i]);
		//layer_add_child(window_layer, back[i]);
		
		//point to image data
		back_image[i] = (GBitmap){.addr = &image_data[i * IMAGESIZE], .bounds = GRect(0,0,144,168), .row_size_bytes = ROWSIZE};
		
		//set cards blank (loading)
		reset_card(i);
		
	}
	
	layer_add_child(window_layer, watchface);
	
	for(int i=0; i<CACHESIZE; i++)
	{
		layer_add_child(window_layer, back[i]);
	}
	
	
	
	for(int i = 0; i < CACHESIZE; i++)
		{
		layer_add_child(window_layer, card[i]);
	}
	
	load_fake_cards();
	
	
	//register handlers
	app_message_register_inbox_received(in_received_handler);
	app_message_register_inbox_dropped(in_dropped_handler);
	app_message_register_outbox_sent(out_sent_handler);
	app_message_register_outbox_failed(out_failed_handler);

	//set size
	const uint32_t inbound_size = 512;
	const uint32_t outbound_size = 64;
	app_message_open(inbound_size, outbound_size);
	
	app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
	
	DictionaryIterator *iter;
 	app_message_outbox_begin(&iter);
	Tuplet value = TupletInteger(1, 0);
	dict_write_tuplet(iter, &value);
	app_message_outbox_send();
	
}

//DEINIT
void deinit(){
	
	animation_unschedule_all();
	
	//deinit layers
	for(int i=0; i<CACHESIZE; i++){
		if(card_anim[i] != NULL)
		property_animation_destroy(card_anim[i]);
		if(back_anim[i] != NULL)
		property_animation_destroy(back_anim[i]);
		
		layer_destroy(card[i]);
		layer_destroy(back[i]);
		
		
	}
	
	layer_destroy(watchface);
	window_destroy(window);
	
}

//MAIN
int main(void) {
	init();
	app_event_loop();
	deinit();
}