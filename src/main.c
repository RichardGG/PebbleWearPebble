#include "pebble.h"

//image
#define ROW_SIZE 20 // 144 pixels / 8 bits per byte = 18 bytes. Row bytes must be multiple of 4 = 20 bytes
#define IMAGE_SIZE 2880 // 20 bytes * 144 = 2880 bytes.
//icon
#define ICON_ROW_SIZE 8 // 48 pixels / 8 bits per byte = 6 bytes. Row bytes must be multiple of 4 = 8 bytes
#define ICON_SIZE 384 // 8 bytes * 48 = 384 bytes.
//strings
#define TITLE_SIZE 30
#define TEXT_SIZE 80
//cache
#define CACHE_SIZE 4
//card
#define MIN_CARD_HEIGHT 54
#define MAX_CARD_HEIGHT 102
//message size
#define ICON_MESSAGE_ROWS 16
#define IMAGE_MESSAGE_ROWS 4
//expand size
#define EXPAND_SIZE 95
	
#define TEMP_SIZE 66
	
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
	
static Window* window;

//background
static Layer* back_layer_A;
static Layer* back_layer_B;
static PropertyAnimation* back_animation_old = NULL;
static PropertyAnimation* back_animation_new = NULL;
static GBitmap back_bitmaps[CACHE_SIZE];
static uint8_t back_image_data[CACHE_SIZE][IMAGE_SIZE];

//card
static Layer* card_layer_A;
static Layer* card_layer_B;
static PropertyAnimation* card_animation_old = NULL;
static PropertyAnimation* card_animation_new = NULL;
static GBitmap icon_bitmaps[CACHE_SIZE];
static uint8_t icon_image_data[CACHE_SIZE][ICON_SIZE];

//expanded
static Layer* expanded_layer;
static PropertyAnimation* expanded_animation = NULL;

//strings
static char title_strings[CACHE_SIZE][TITLE_SIZE];
static char text_strings[CACHE_SIZE][TEXT_SIZE];

//ids
static int notif_ids[CACHE_SIZE];

//watchface
static Layer* watchface_layer;

//selection
static int current = 0;
static int previous = 1;
static int total_cards = 22;
static char watchface_visible = 1;
static char expanded_visible = 0;
static char long_press_down = 0;

void reposition_new() //reposition current before animation
{
	GRect card_frame = layer_get_frame((current%2==0)?card_layer_A:card_layer_B);
	card_frame.origin.y = 168;
	layer_set_frame((current%2==0)?card_layer_A:card_layer_B, card_frame);
	layer_set_frame((current%2==0)?back_layer_A:back_layer_B, GRect(0,144,144,144));
}

void reposition_old() //reposition previous before animation
{
	GRect card_frame = layer_get_frame((current%2==1)?card_layer_A:card_layer_B);
	card_frame.origin.y = 168 - card_frame.size.h;
	layer_set_frame((current%2==1)?card_layer_A:card_layer_B, card_frame);
	layer_set_frame((current%2==1)?back_layer_A:back_layer_B, GRect(0,0,144,144));
}

void reposition_current() //reposition current on resize
{
	if(!watchface_visible)
	{
		
		//hide card B
		GRect card_frame = layer_get_frame(card_layer_B);
		card_frame.origin.y = 168;
		layer_set_frame(card_layer_B, card_frame);
		
		card_frame = layer_get_frame((current%2==0)?card_layer_A:card_layer_B);
		card_frame.origin.y = 168 - card_frame.size.h;
		layer_set_frame((current%2==0)?card_layer_A:card_layer_B, card_frame);
		layer_set_frame((current%2==0)?back_layer_A:back_layer_B, GRect(0,0,144,144));
		layer_set_frame((current%2==0)?back_layer_B:back_layer_A, GRect(0,144,144,144));
	}
}



void resize_layers()
{
	int current_height = MIN_CARD_HEIGHT + 4 + graphics_text_layout_get_content_size(text_strings[current%CACHE_SIZE],
																				 fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),GRect(0,0,142,168),GTextOverflowModeWordWrap, GTextAlignmentLeft).h;
	int previous_height = MIN_CARD_HEIGHT + 4 + graphics_text_layout_get_content_size(text_strings[previous%CACHE_SIZE],
																				  fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),GRect(0,0,142,168),GTextOverflowModeWordWrap, GTextAlignmentLeft).h;
	if(current_height > MAX_CARD_HEIGHT)
		current_height = MAX_CARD_HEIGHT;
	
	if(previous_height > MAX_CARD_HEIGHT)
		previous_height = MAX_CARD_HEIGHT;
	
	int back_A_pos = layer_get_frame(back_layer_A).origin.y;
	int back_B_pos = layer_get_frame(back_layer_B).origin.y;
	int card_A_pos = layer_get_frame(card_layer_A).origin.y;
	int card_B_pos = layer_get_frame(card_layer_B).origin.y;
	
	
	if(current % 2 == 0)//if even
	{
		layer_set_frame(back_layer_A, GRect(0,back_A_pos,144,144));				//0
		layer_set_frame(back_layer_B, GRect(0,back_B_pos,144,144));				//144
		layer_set_frame(card_layer_A, GRect(0,card_A_pos,144,current_height));	//168-current
		layer_set_frame(card_layer_B, GRect(0,card_B_pos,144,previous_height));	//168+168-previous
	}
	else
	{
		layer_set_frame(back_layer_A, GRect(0,back_A_pos,144,144));				//144
		layer_set_frame(back_layer_B, GRect(0,back_B_pos,144,144));				//0
		layer_set_frame(card_layer_A, GRect(0,card_A_pos,144,previous_height));	//168+168-previous
		layer_set_frame(card_layer_B, GRect(0,card_B_pos,144,current_height));	//168-current
	}
	
	if(watchface_visible)
	{
		layer_set_frame(watchface_layer, GRect(0,0,144,168));
		layer_set_frame(card_layer_A, GRect(0,168-MIN_CARD_HEIGHT,144,current_height));
	}
	else
		layer_set_frame(watchface_layer, GRect(0,-168,144,168));
}


void blank_image_data(int image_number){
	//for each row
	for(int row = 0; row < 144; row++){
		//for each byte
		for(int col = 0; col < ROW_SIZE; col++){
			//pointer to current byte, easier than typing out this string every time
			uint8_t* current_byte = &back_image_data[image_number][row * ROW_SIZE + col]; 
			*current_byte = 0;
			
			//for each bit
			for(int bit = 0; bit < 8; bit++){
				//add 1 or 0 (in checkerboard pattern)
				*current_byte += ((row % 2) + (col * 8 + bit)) % 2; 

				//if not last bit
				if(bit != 7)
					*current_byte <<= 1;//shift bits
			}
		}
	}
}

void blank_icon_data(int image_number){
	//for each row
	for(int row = 0; row < 48; row++){
		//for each byte
		for(int col = 0; col < ICON_ROW_SIZE; col++){
			//pointer to current byte, easier than typing out this string every time
			uint8_t* current_byte = &icon_image_data[image_number][row * ICON_ROW_SIZE + col]; 
			*current_byte = 0;
			
			//for each bit
			//for(int bit = 0; bit < 8; bit++){
				//add 1 or 0 (in checkerboard pattern)
			//	*current_byte += ((row % 2) + (col * 8 + bit)) % 2; 

			//	//if not last bit
			//	if(bit != 7)
			//		*current_byte <<= 1;//shift bits
			//}
		}
	}
}

void reset_card(int card_no)
{
	blank_image_data(card_no);
	blank_icon_data(card_no);
	

		strcpy(title_strings[card_no], "Loading");
		strcpy(text_strings[card_no], "");	
}

void in_received_handler(DictionaryIterator *iter, void *context) {

	//vibes_short_pulse();
	
	
	Tuple *tuple_pointer = dict_find(iter,ID);
	int id = tuple_pointer->value->int32;
	
	tuple_pointer = NULL;
	
	tuple_pointer = dict_find(iter, COMMAND);
	if(id < 4)
	{
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
				
				//char string[21];
				for(int i=0;i<TITLE_SIZE;i++){
					title_strings[id][i] = bytes[i];
				}
				for(int i=0; i<TEXT_SIZE;i++)
				{
					text_strings[id][i] = bytes[TITLE_SIZE+i];
				}
				title_strings[id][TITLE_SIZE-1] = '\0';
				text_strings[id][TEXT_SIZE-1] = '\0';
				
				resize_layers();
				if(current == id)
					reposition_current();
				layer_mark_dirty(card_layer_A);
				layer_mark_dirty(card_layer_B);
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
				
				int starting_row = tuple_row->value->int32;

				//icon_data[0] = 0;
				//icon_data[0] = byteArray[0];
				for(int additional_rows = 0; additional_rows < ICON_MESSAGE_ROWS; additional_rows++)
				{
					for(int i =0; i < 6; i++)
					{
						icon_image_data[id][i + (64/8)*(starting_row+additional_rows)] = byteArray[i+(48/8)*additional_rows];
					}
				}
				layer_mark_dirty(card_layer_A);
				layer_mark_dirty(card_layer_B);
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
				int starting_row = tuple_row->value->int32;

				for(int additional_rows = 0; additional_rows < IMAGE_MESSAGE_ROWS; additional_rows++)
				{
					for(int i =0; i < 18; i++)
					{
						//APP_LOG(APP_LOG_LEVEL_DEBUG, "row: %d byte: %d", starting_row + additional_rows, i + (160/8)*(starting_row+additional_rows));
						back_image_data[id][i + (160/8)*(starting_row+additional_rows)] = byteArray[i+(144/8)*additional_rows];
					}
				}
				layer_mark_dirty(back_layer_A);
				layer_mark_dirty(back_layer_B);
			}
		}
		/*else if(tuple_pointer->value->int8 == ACTIONS)
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
		}*/
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

static void update_back(GContext* ctx, int image_no)
{
	//if same odd/even, selection = current, else selection = previous
	int selection = (image_no % 2 == current % 2)? current : previous;
	int card_height = MIN_CARD_HEIGHT + 4 + graphics_text_layout_get_content_size(text_strings[selection%CACHE_SIZE],
																				 fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),GRect(0,0,142,168),GTextOverflowModeWordWrap, GTextAlignmentLeft).h;
	if(card_height > MAX_CARD_HEIGHT)
		card_height = MAX_CARD_HEIGHT;
	
	int image_pos = 0-(card_height / 4);
	
	graphics_draw_bitmap_in_rect(ctx, &back_bitmaps[image_no],GRect(0,image_pos,144,144));
}


static void update_expanded_layer(Layer *me, GContext* ctx)
{
	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_fill_rect(ctx, GRect(0, 0, 144, 168), 0, GCornerNone);
	
	graphics_context_set_text_color(ctx, GColorBlack);	
	graphics_draw_text(ctx, 
					   text_strings[current%CACHE_SIZE],  
					   fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
					   GRect( 2, -55, 142, 168), //magic number: 55
					   GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
}

static void update_back_layer_A(Layer *me, GContext* ctx)
{
	int image_no = (current % 2 == 0)?current:previous; //"current image" when even
	image_no = image_no%CACHE_SIZE;
	update_back(ctx, image_no);
}

static void update_back_layer_B(Layer *me, GContext* ctx)
{
	int image_no = (current % 2 == 1)?current:previous; //"current image" when odd
	image_no = image_no%CACHE_SIZE;
	update_back(ctx, image_no);
}

static void update_card(GContext* ctx, int card_no)
{
	int title_height = graphics_text_layout_get_content_size(title_strings[card_no],fonts_get_system_font(FONT_KEY_GOTHIC_18),GRect(0,0,142-54,168),GTextOverflowModeWordWrap, GTextAlignmentLeft).h;
	
	//card
	graphics_context_set_fill_color(ctx, GColorWhite);
	if(title_height > 18)
		graphics_fill_rect(ctx, GRect(0, 12, 144, 168), 0, GCornerNone);
	else
		graphics_fill_rect(ctx, GRect(0, 26, 144, 168), 0, GCornerNone);
	
	//icon box
	graphics_context_set_stroke_color(ctx, GColorWhite);
	graphics_draw_round_rect(ctx,GRect(144-54, 0, 54, 54),3);		
	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_rect(ctx, GRect(144-53, 1, 52, 52), 3, GCornersAll);
	
	//icon
	graphics_draw_bitmap_in_rect(ctx, &icon_bitmaps[card_no],GRect(144-51,3,48,48));
	
	//text
	graphics_context_set_text_color(ctx, GColorBlack);	
	if(title_height > 18)
		graphics_draw_text(ctx, 
						   title_strings[card_no],  
						   fonts_get_system_font(FONT_KEY_GOTHIC_18),
						   GRect( 2, 12, 142-54, 20),
						   GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
	else
		graphics_draw_text(ctx, 
					   title_strings[card_no],  
					   fonts_get_system_font(FONT_KEY_GOTHIC_18),
					   GRect( 2, 26, 142-54, 20),
					   GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
	
	graphics_draw_text(ctx, 
					   text_strings[card_no],  
					   fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
					   GRect( 2, MIN_CARD_HEIGHT - 7, 142, 60),
					   (expanded_visible)?GTextOverflowModeWordWrap:GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void update_card_layer_A(Layer *me, GContext* ctx)
{
	int card_no = (current % 2 == 0)?current:previous; //"current card" when even
	card_no = card_no%CACHE_SIZE;
	
	update_card(ctx, card_no);
}

static void update_card_layer_B(Layer *me, GContext* ctx)
{
	int card_no = (current % 2 == 1)?current:previous; //"current card" when odd
	card_no = card_no%CACHE_SIZE;
	
	update_card(ctx, card_no);
}

static void update_watchface(Layer *me, GContext* ctx)
{
	char current_time[10];
	
	graphics_context_set_fill_color(ctx, GColorBlack );
	graphics_fill_rect(ctx, GRect(0, 0, 144, 168), 0, GCornerNone);
	
	clock_copy_time_string(current_time, 10);
	
	graphics_context_set_text_color(ctx, GColorWhite);	
	graphics_draw_text(ctx, 
					   current_time,  
					   fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD),
					   GRect( 2, 2, 140, 164),
					   GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
}



void destroy_animations()
{
	//unschedule any previous animation
	animation_unschedule((Animation*)back_animation_old);
	animation_unschedule((Animation*)back_animation_new);
	animation_unschedule((Animation*)card_animation_old);
	animation_unschedule((Animation*)card_animation_new);
	animation_unschedule((Animation*)expanded_animation);
	
	//destroy memory used for previous animation
	if(back_animation_old != NULL)
		property_animation_destroy(back_animation_old);
	if(back_animation_new != NULL)
		property_animation_destroy(back_animation_new);
	if(card_animation_old != NULL)
		property_animation_destroy(card_animation_old);
	if(card_animation_new != NULL)
		property_animation_destroy(card_animation_new);
	if(expanded_animation != NULL)
		property_animation_destroy(expanded_animation);
	
	//make NULL to avoid redestroying memory
	back_animation_old = NULL;
	back_animation_new = NULL;
	card_animation_old = NULL;
	card_animation_new = NULL;
	expanded_animation = NULL;
}

void show_watchface()
{
	destroy_animations();
	resize_layers();
	reposition_current();
	
	int card_height = layer_get_frame(card_layer_A).size.h;
	APP_LOG(APP_LOG_LEVEL_DEBUG, "%d", card_height);
	GRect card_from = GRect(0,168-card_height,144,card_height);
	GRect card_to = GRect(0,168-MIN_CARD_HEIGHT,144,card_height);//SET THIS TO THE HEIGHT OF CARD TOP
	GRect watchface_from = GRect(0,-168, 144, 168);
	GRect watchface_to = GRect(0, 0, 144, 168);
	
	//animate card
	card_animation_old = property_animation_create_layer_frame(card_layer_A, &card_from, &card_to);
	animation_set_curve((Animation*) card_animation_old, AnimationCurveEaseOut);
	animation_set_duration((Animation*) card_animation_old, 300);
	animation_schedule((Animation*) card_animation_old);
	
	//animate watchface
	card_animation_new = property_animation_create_layer_frame(watchface_layer, &watchface_from, &watchface_to);
	animation_set_curve((Animation*) card_animation_new, AnimationCurveEaseOut);
	animation_set_duration((Animation*) card_animation_new, 300);
	animation_schedule((Animation*) card_animation_new);
	
	watchface_visible = 1;
}

void hide_watchface()
{
	destroy_animations();
	//watchface_visible = 0;
	resize_layers();
	
	int card_height = layer_get_frame(card_layer_A).size.h;
	APP_LOG(APP_LOG_LEVEL_DEBUG, "%d", card_height);
	GRect card_from = GRect(0,168-MIN_CARD_HEIGHT,144,card_height);//SET THIS TO THE HEIGHT OF CARD TOP
	GRect card_to = GRect(0,168-card_height,144,card_height);
	GRect watchface_from = GRect(0,0,144,168);
	GRect watchface_to = GRect(0,-168,144,168);
	
	//animate card
	card_animation_new = property_animation_create_layer_frame(card_layer_A, &card_from, &card_to);
	animation_set_curve((Animation*) card_animation_new, AnimationCurveEaseOut);
	animation_set_duration((Animation*) card_animation_new, 300);
	animation_schedule((Animation*) card_animation_new);
	
	//animate watchface
	card_animation_old = property_animation_create_layer_frame(watchface_layer, &watchface_from, &watchface_to);
	animation_set_curve((Animation*) card_animation_old, AnimationCurveLinear);
	animation_set_duration((Animation*) card_animation_old, 300);
	animation_schedule((Animation*) card_animation_old);
	
	watchface_visible = 0;
}

void hide_expanded_up_press()
{
	destroy_animations();
	
	Layer* current_card = (current%2 == 0)?card_layer_A:card_layer_B;
	
	int card_height = layer_get_frame(current_card).size.h;
	GRect card_from = layer_get_frame(current_card);
	GRect card_to = GRect(0,168-card_from.size.h,144,card_from.size.h);
	GRect expanded_from = GRect(0,EXPAND_SIZE, 144, 168-EXPAND_SIZE);
	GRect expanded_to = GRect(0, 168, 144, 168-EXPAND_SIZE);
	
	//animate card
	card_animation_old = property_animation_create_layer_frame(current_card, &card_from, &card_to);
	animation_set_curve((Animation*) card_animation_old, AnimationCurveEaseOut);
	animation_set_duration((Animation*) card_animation_old, 300);
	animation_schedule((Animation*) card_animation_old);
	
	//animate expanded layer
	expanded_animation = property_animation_create_layer_frame(expanded_layer, &expanded_from, &expanded_to);
	animation_set_curve((Animation*) expanded_animation, AnimationCurveEaseOut);
	animation_set_duration((Animation*) expanded_animation, 300);
	animation_schedule((Animation*) expanded_animation);
	
	expanded_visible = 0;
}

void hide_expanded_down_press()
{
	animation_unschedule((Animation*)expanded_animation);
	
	//destroy memory used for previous animation
	if(expanded_animation != NULL)
		property_animation_destroy(expanded_animation);
	
	//make NULL to avoid redestroying memory
	expanded_animation = NULL;
	
	GRect expanded_from = GRect(0,EXPAND_SIZE, 144, 168-EXPAND_SIZE);
	GRect expanded_to = GRect(0, 0-(168-EXPAND_SIZE), 144, 168-EXPAND_SIZE);
	
	//animate expanded layer
	expanded_animation = property_animation_create_layer_frame(expanded_layer, &expanded_from, &expanded_to);
	animation_set_curve((Animation*) expanded_animation, AnimationCurveEaseOut);
	animation_set_duration((Animation*) expanded_animation, 300);
	animation_schedule((Animation*) expanded_animation);
	
	expanded_visible = 0;
}

void show_expanded()
{	
	destroy_animations();
	
	Layer* current_card = (current%2 == 0)?card_layer_A:card_layer_B;
	
	int card_height = layer_get_frame(current_card).size.h;
	GRect card_from = layer_get_frame(current_card);
	GRect card_to = GRect(0,EXPAND_SIZE-card_from.size.h,144,card_from.size.h);
	GRect expanded_from = GRect(0,168, 144, 168-EXPAND_SIZE);
	GRect expanded_to = GRect(0, EXPAND_SIZE, 144, 168-EXPAND_SIZE);
	
	//animate card
	card_animation_old = property_animation_create_layer_frame(current_card, &card_from, &card_to);
	animation_set_curve((Animation*) card_animation_old, AnimationCurveEaseOut);
	animation_set_duration((Animation*) card_animation_old, 300);
	animation_schedule((Animation*) card_animation_old);
	
	//animate expanded layer
	expanded_animation = property_animation_create_layer_frame(expanded_layer, &expanded_from, &expanded_to);
	animation_set_curve((Animation*) expanded_animation, AnimationCurveEaseOut);
	animation_set_duration((Animation*) expanded_animation, 300);
	animation_schedule((Animation*) expanded_animation);
	
	expanded_visible = 1;
}

void animate()
{
	//start and end position of elements
	GRect old_back_from;
	GRect old_back_to;
	GRect new_back_from;
	GRect new_back_to;
	GRect old_card_from;
	GRect old_card_to;
	GRect new_card_from;
	GRect new_card_to;

	//pointers to choose odd or even
	Layer** old_back_layer;
	Layer** new_back_layer;
	Layer** old_card_layer;
	Layer** new_card_layer;

	//memory cleanup
	destroy_animations();

	if(current % 2 == 0) //if even, we must be moving to layer A
	{
		old_back_layer = &back_layer_B;
		old_card_layer = &card_layer_B;
		new_back_layer = &back_layer_A;
		new_card_layer = &card_layer_A;
	}
	else //if odd, we must be moving to layer B
	{
		old_back_layer = &back_layer_A;
		old_card_layer = &card_layer_A;
		new_back_layer = &back_layer_B;
		new_card_layer = &card_layer_B;
	}
	
	int old_card_height = layer_get_frame(*old_card_layer).size.h;
	int new_card_height = layer_get_frame(*new_card_layer).size.h;
	
	//set frames based on animation direction
	if(current - previous > 0)
	{
		//backgrounds
		old_back_from = GRect(0,   0,144,144);
		old_back_to = 	GRect(0,-144,144,144);
		new_back_from = GRect(0, 144,144,144);
		new_back_to = 	GRect(0,   0,144,144);
		//cards
		old_card_from = GRect(0,168-old_card_height,144,old_card_height);
		old_card_to = 	GRect(0,  0-old_card_height,144,old_card_height);
		new_card_from = GRect(0,336-new_card_height,144,new_card_height);
		new_card_to = 	GRect(0,168-new_card_height,144,new_card_height);
	}
	else
	{
		//backgrounds
		old_back_from = GRect(0,   0,144,144);
		old_back_to = 	GRect(0, 144,144,144);
		new_back_from = GRect(0,-144,144,144);
		new_back_to = 	GRect(0,   0,144,144);
		//cards
		old_card_from = GRect(0,168-old_card_height,144,old_card_height);
		old_card_to = 	GRect(0,336-old_card_height,144,old_card_height);
		new_card_from = GRect(0,  0-new_card_height,144,new_card_height);
		new_card_to = 	GRect(0,168-new_card_height,144,new_card_height);
	}
	
	//old background layer
	back_animation_old = property_animation_create_layer_frame(*old_back_layer, &old_back_from, &old_back_to);
	animation_set_curve((Animation*) back_animation_old, AnimationCurveLinear);
	animation_set_duration((Animation*) back_animation_old, 300);
	animation_schedule((Animation*) back_animation_old);
	
	//new background layer
	back_animation_new = property_animation_create_layer_frame(*new_back_layer, &new_back_from, &new_back_to);
	animation_set_curve((Animation*) back_animation_new, AnimationCurveEaseOut);
	animation_set_duration((Animation*) back_animation_new, 300);
	animation_schedule((Animation*) back_animation_new);
	
	//old card layer
	card_animation_old = property_animation_create_layer_frame(*old_card_layer, &old_card_from, &old_card_to);
	animation_set_curve((Animation*) card_animation_old, AnimationCurveLinear);
	animation_set_duration((Animation*) card_animation_old, 300);
	animation_schedule((Animation*) card_animation_old);
	
	//new card layer
	card_animation_new = property_animation_create_layer_frame(*new_card_layer, &new_card_from, &new_card_to);
	animation_set_curve((Animation*) card_animation_new, AnimationCurveEaseOut);
	animation_set_duration((Animation*) card_animation_new, 300);
	animation_schedule((Animation*) card_animation_new);
}

void press_down(ClickRecognizerRef recognizer, void *context) 
{
	
}

void release_down(ClickRecognizerRef recognizer, void *context) 
{
	if(long_press_down)
		long_press_down = 0;
	else
	{
		if(watchface_visible)
		{
			hide_watchface();
		}
		else if(current < total_cards - 1)
		{
			
			//else
			//{
				//update current valuees
				previous = current;
				current++;
				reposition_new();
				resize_layers();
				reposition_old();
				layer_mark_dirty(back_layer_A);
				layer_mark_dirty(back_layer_B);

				//animate
				animate();
			
			if(expanded_visible)
			{
				hide_expanded_down_press();
			}

			//}
		}
	}
}

void long_down(ClickRecognizerRef recognizer, void  *context)
{
	if(!expanded_visible)
	{
		char expandable = true; //?
		if(!watchface_visible && expandable)
		{
			long_press_down = 1;
			show_expanded();
		}
	}
}

void press_up(ClickRecognizerRef recognizer, void *context) 
{
	
}

void release_up(ClickRecognizerRef recognizer, void *context) 
{
	if(expanded_visible)
	{
		hide_expanded_up_press();
	}
	else if(current > 0)
	{
		previous = current;
		current--;
		resize_layers();
		layer_mark_dirty(back_layer_A);
		layer_mark_dirty(back_layer_B);
		
		animate();
	}
	else if(current == 0 && !watchface_visible)
	{
		show_watchface();
	}
}

void subscribe_buttons(Window *window) 
{
	window_raw_click_subscribe(BUTTON_ID_DOWN, press_down, release_down, NULL);
	window_long_click_subscribe(BUTTON_ID_DOWN, 400, long_down, NULL);
	window_raw_click_subscribe(BUTTON_ID_UP, press_up, release_up, NULL);
}

void tick(struct tm *tick_time, TimeUnits units_changed)
{
	layer_mark_dirty(watchface_layer);
}

void init()
{
	window = window_create();
	window_set_fullscreen(window, true);
	window_stack_push(window, true);
	
	Layer* window_layer = window_get_root_layer(window);
	
	//backgrounds
	back_layer_A = layer_create(GRect(0,0,144,144));
	layer_set_update_proc(back_layer_A, update_back_layer_A);
	back_layer_B = layer_create(GRect(0,168,144,144));
	layer_set_update_proc(back_layer_B, update_back_layer_B);
	//watchface
	watchface_layer = layer_create(GRect(0,0,144,168));
	layer_set_update_proc(watchface_layer, update_watchface);
	//cards
	card_layer_A = layer_create(GRect(0,168,144,168));
	layer_set_update_proc(card_layer_A, update_card_layer_A);
	card_layer_B = layer_create(GRect(0,168,144,168));
	layer_set_update_proc(card_layer_B, update_card_layer_B);
	//expanded
	expanded_layer = layer_create(GRect(0,168,144,168-50));
	layer_set_update_proc(expanded_layer, update_expanded_layer);
	
	for(int i=0; i<CACHE_SIZE; i++)
	{	
		back_bitmaps[i] = (GBitmap){.addr = back_image_data[i], .bounds = GRect(0,0,144,144), .row_size_bytes = ROW_SIZE};
		icon_bitmaps[i] = (GBitmap){.addr = icon_image_data[i], .bounds = GRect(0,0,48,48), .row_size_bytes = ICON_ROW_SIZE};
		reset_card(i);
	}
	
	resize_layers();
	
	layer_add_child(window_layer, back_layer_A);
	layer_add_child(window_layer, back_layer_B);
	layer_add_child(window_layer, watchface_layer);
	layer_add_child(window_layer, card_layer_A);
	layer_add_child(window_layer, card_layer_B);
	layer_add_child(window_layer, expanded_layer);
	
	window_set_click_config_provider(window, (ClickConfigProvider) subscribe_buttons);
	tick_timer_service_subscribe(MINUTE_UNIT, (TickHandler) tick);
	
	
	//message stuff
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

void deinit()
{
	destroy_animations();
	layer_destroy(back_layer_A);
	layer_destroy(back_layer_B);
	layer_destroy(card_layer_A);
	layer_destroy(card_layer_B);
	layer_destroy(watchface_layer);
	layer_destroy(expanded_layer);
	window_destroy(window);
}
	
int main(void)
{
	init();
	app_event_loop();
	deinit();
}
