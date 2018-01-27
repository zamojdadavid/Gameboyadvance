/*
 * collide.c
 * program which demonstrates sprites colliding with tiles
 */

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 160

/* include the background image we are using */
#include "background.h"


/* include the sprite image we are using */
/*#include "falcoShooting.h"
#include "shyguy.h" 
*/
#include "spritesheet.h"

/* include the tile map we are using */
#include "map.h"
#include "map2.h"
#include "map3.h"
#include "map4.h"

/* the tile mode flags needed for display control register */
#define MODE0 0x00
#define BG0_ENABLE 0x100
#define BG1_ENABLE 0x200
#define BG2_ENABLE 0X400

/* flags to set sprite handling in display control register */
#define SPRITE_MAP_2D 0x0
#define SPRITE_MAP_1D 0x40
#define SPRITE_ENABLE 0x1000


/* the control registers for the four tile layers */
volatile unsigned short* bg0_control = (volatile unsigned short*) 0x4000008;
volatile unsigned short* bg1_control = (volatile unsigned short*) 0x400000a;
volatile unsigned short* bg2_control = (volatile unsigned short*) 0x400000c;

/* palette is always 256 colors */
#define PALETTE_SIZE 256

/* there are 128 sprites on the GBA */
#define NUM_SPRITES 128

/* the display control pointer points to the gba graphics register */
volatile unsigned long* display_control = (volatile unsigned long*) 0x4000000;

/* the memory location which controls sprite attributes */
volatile unsigned short* sprite_attribute_memory = (volatile unsigned short*) 0x7000000;

/* the memory location which stores sprite image data */
volatile unsigned short* sprite_image_memory = (volatile unsigned short*) 0x6010000;

/* the address of the color palettes used for backgrounds and sprites */
volatile unsigned short* bg_palette = (volatile unsigned short*) 0x5000000;
volatile unsigned short* sprite_palette = (volatile unsigned short*) 0x5000200;

/* the button register holds the bits which indicate whether each button has
 * been pressed - this has got to be volatile as well
 */
volatile unsigned short* buttons = (volatile unsigned short*) 0x04000130;

/* scrolling registers for backgrounds */
volatile short* bg0_x_scroll = (unsigned short*) 0x4000010;
volatile short* bg0_y_scroll = (unsigned short*) 0x4000012;
volatile short* bg1_x_scroll = (unsigned short*) 0x4000014;
volatile short* bg1_y_scroll = (unsigned short*) 0x4000016;

/* the bit positions indicate each button - the first bit is for A, second for
 * B, and so on, each constant below can be ANDED into the register to get the
 * status of any one button */
#define BUTTON_A (1 << 0)
#define BUTTON_B (1 << 1)
#define BUTTON_SELECT (1 << 2)
#define BUTTON_START (1 << 3)
#define BUTTON_RIGHT (1 << 4)
#define BUTTON_LEFT (1 << 5)
#define BUTTON_UP (1 << 6)
#define BUTTON_DOWN (1 << 7)
#define BUTTON_R (1 << 8)
#define BUTTON_L (1 << 9)

/* the scanline counter is a memory cell which is updated to indicate how
 * much of the screen has been drawn */
volatile unsigned short* scanline_counter = (volatile unsigned short*) 0x4000006;

/* wait for the screen to be fully drawn so we can do something during vblank */
void wait_vblank() {
	/* wait until all 160 lines have been updated */
	while (*scanline_counter < 160) { }
}

/* this function checks whether a particular button has been pressed */
unsigned char button_pressed(unsigned short button) {
	/* and the button register with the button constant we want */
	unsigned short pressed = *buttons & button;

	/* if this value is zero, then it's not pressed */
	if (pressed == 0) {
		return 1;
	} else {
		return 0;
	}
}

/* return a pointer to one of the 4 character blocks (0-3) */
volatile unsigned short* char_block(unsigned long block) {
	/* they are each 16K big */
	return (volatile unsigned short*) (0x6000000 + (block * 0x4000));
}

/* return a pointer to one of the 32 screen blocks (0-31) */
volatile unsigned short* screen_block(unsigned long block) {
	/* they are each 2K big */
	return (volatile unsigned short*) (0x6000000 + (block * 0x800));
}

/* flag for turning on DMA */
#define DMA_ENABLE 0x80000000

/* flags for the sizes to transfer, 16 or 32 bits */
#define DMA_16 0x00000000
#define DMA_32 0x04000000

/* pointer to the DMA source location */
volatile unsigned int* dma_source = (volatile unsigned int*) 0x40000D4;

/* pointer to the DMA destination location */
volatile unsigned int* dma_destination = (volatile unsigned int*) 0x40000D8;

/* pointer to the DMA count/control */
volatile unsigned int* dma_count = (volatile unsigned int*) 0x40000DC;

/* copy data using DMA */
void memcpy16_dma(unsigned short* dest, unsigned short* source, int amount) {
	*dma_source = (unsigned int) source;
	*dma_destination = (unsigned int) dest;
	*dma_count = amount | DMA_16 | DMA_ENABLE;
}

/* function to setup background 0 for this program */
void setup_background() {

	/* load the palette from the image into palette memory*/
	memcpy16_dma((unsigned short*) bg_palette, (unsigned short*) background_palette, PALETTE_SIZE);

	/* load the image into char block 0 */
	memcpy16_dma((unsigned short*) char_block(0), (unsigned short*) background_data,
			(background_width * background_height) / 2);

	/* set all control the bits in this register */
	*bg0_control = 2 |    /* priority, 0 is highest, 3 is lowest */
		(0 << 2)  |       /* the char block the image data is stored in */
		(0 << 6)  |       /* the mosaic flag */
		(1 << 7)  |       /* color mode, 0 is 16 colors, 1 is 256 colors */
		(16 << 8) |       /* the screen block the tile data is stored in */
		(1 << 13) |       /* wrapping flag */
		(0 << 14);        /* bg size, 0 is 256x256 */
	*bg1_control = 1 |    /* priority, 0 is highest, 3 is lowest */
		(0 << 2)  |       /* the char block the image data is stored in */
		(0 << 6)  |       /* the mosaic flag */
		(1 << 7)  |       /* color mode, 0 is 16 colors, 1 is 256 colors */
		(24 << 8) |       /* the screen block the tile data is stored in*/ 
		(1 << 13) |       /* wrapping flag */
		(0 << 14);        /* bg size, 0 is 256x256 */
	*bg2_control = 3 |    /* priority, 0 is highest, 3 is lowest */
		(0 << 2)  |       /* the char block the image data is stored in */
		(0 << 6)  |       /* the mosaic flag */
		(1 << 7)  |       /* color mode, 0 is 16 colors, 1 is 256 colors */
		(8 << 8) |       /* the screen block the tile data is stored in*/ 
		(1 << 13) |       /* wrapping flag */
		(0 << 14);        /* bg size, 0 is 256x256 */
	
   /*  load the tile data into screen block 16 
	*/
	/* load the tile data into screen block 16 */
	memcpy16_dma((unsigned short*) screen_block(16), (unsigned short*) map, map_width * map_height);
	memcpy16_dma((unsigned short*) screen_block(24), (unsigned short*) map2, map2_width * map2_height);
	memcpy16_dma((unsigned short*) screen_block(8), (unsigned short*) map3, map3_width * map3_height);

}

/* just kill time */
void delay(unsigned int amount) {
	for (int i = 0; i < amount * 10; i++);
}

/* a sprite is a moveable image on the screen */
struct Sprite {
	unsigned short attribute0;
	unsigned short attribute1;
	unsigned short attribute2;
	unsigned short attribute3;
};

/* array of all the sprites available on the GBA */
struct Sprite sprites[NUM_SPRITES];
int next_sprite_index = 0;

/* the different sizes of sprites which are possible */
enum SpriteSize {
	SIZE_8_8,
	SIZE_16_16,
	SIZE_32_32,
	SIZE_64_64,
	SIZE_16_8,
	SIZE_32_8,
	SIZE_32_16,
	SIZE_64_32,
	SIZE_8_16,
	SIZE_8_32,
	SIZE_16_32,
	SIZE_32_64
};

/* function to initialize a sprite with its properties, and return a pointer */
struct Sprite* sprite_init(int x, int y, enum SpriteSize size, int horizontal_flip, int vertical_flip, int tile_index, int priority) {

	/* grab the next index */
	int index = next_sprite_index++;

	/* setup the bits used for each shape/size possible */
	int size_bits, shape_bits;
	switch (size) {
		case SIZE_8_8:   size_bits = 0; shape_bits = 0; break;
		case SIZE_16_16: size_bits = 1; shape_bits = 0; break;
		case SIZE_32_32: size_bits = 2; shape_bits = 0; break;
		case SIZE_64_64: size_bits = 3; shape_bits = 0; break;
		case SIZE_16_8:  size_bits = 0; shape_bits = 1; break;
		case SIZE_32_8:  size_bits = 1; shape_bits = 1; break;
		case SIZE_32_16: size_bits = 2; shape_bits = 1; break;
		case SIZE_64_32: size_bits = 3; shape_bits = 1; break;
		case SIZE_8_16:  size_bits = 0; shape_bits = 2; break;
		case SIZE_8_32:  size_bits = 1; shape_bits = 2; break;
		case SIZE_16_32: size_bits = 2; shape_bits = 2; break;
		case SIZE_32_64: size_bits = 3; shape_bits = 2; break;
	}

	int h = horizontal_flip ? 1 : 0;
	int v = vertical_flip ? 1 : 0;

	/* set up the first attribute */
	sprites[index].attribute0 = y |             /* y coordinate */
							(0 << 8) |          /* rendering mode */
							(0 << 10) |         /* gfx mode */
							(0 << 12) |         /* mosaic */
							(1 << 13) |         /* color mode, 0:16, 1:256 */
							(shape_bits << 14); /* shape */

	/* set up the second attribute */
	sprites[index].attribute1 = x |             /* x coordinate */
							(0 << 9) |          /* affine flag */
							(h << 12) |         /* horizontal flip flag */
							(v << 13) |         /* vertical flip flag */
							(size_bits << 14);  /* size */

	/* setup the second attribute */
	sprites[index].attribute2 = tile_index |   // tile index */
							(priority << 10) | // priority */
							(0 << 12);         // palette bank (only 16 color)*/

	/* return pointer to this sprite */
	return &sprites[index];
}

/* update all of the spries on the screen */
void sprite_update_all() {
	/* copy them all over */
	memcpy16_dma((unsigned short*) sprite_attribute_memory, (unsigned short*) sprites, NUM_SPRITES * 4);
}

/* setup all sprites */
void sprite_clear() {
	/* clear the index counter */
	next_sprite_index = 0;

	/* move all sprites offscreen to hide them */
	for(int i = 0; i < NUM_SPRITES; i++) {
		sprites[i].attribute0 = SCREEN_HEIGHT;
		sprites[i].attribute1 = SCREEN_WIDTH;
	}
}

/* set a sprite postion */
void sprite_position(struct Sprite* sprite, int x, int y) {
	/* clear out the y coordinate */
	sprite->attribute0 &= 0xff00;

	/* set the new y coordinate */
	sprite->attribute0 |= (y & 0xff);

	/* clear out the x coordinate */
	sprite->attribute1 &= 0xfe00;

	/* set the new x coordinate */
	sprite->attribute1 |= (x & 0x1ff);
}

/* move a sprite in a direction */
void sprite_move(struct Sprite* sprite, int dx, int dy) {
	/* get the current y coordinate */
	int y = sprite->attribute0 & 0xff;

	/* get the current x coordinate */
	int x = sprite->attribute1 & 0x1ff;

	/* move to the new location */
	sprite_position(sprite, x + dx, y + dy);
}

/* change the vertical flip flag */
void sprite_set_vertical_flip(struct Sprite* sprite, int vertical_flip) {
	if (vertical_flip) {
		/* set the bit */
		sprite->attribute1 |= 0x2000;
	} else {
		/* clear the bit */
		sprite->attribute1 &= 0xdfff;
	}
}

/* change the vertical flip flag */
void sprite_set_horizontal_flip(struct Sprite* sprite, int horizontal_flip) {
	if (horizontal_flip) {
		/* set the bit */
		sprite->attribute1 |= 0x1000;
	} else {
		/* clear the bit */
		sprite->attribute1 &= 0xefff;
	}
}

/* change the tile offset of a sprite */
void sprite_set_offset(struct Sprite* sprite, int offset) {
	/* clear the old offset */
	sprite->attribute2 &= 0xfc00;

	/* apply the new one */
	sprite->attribute2 |= (offset & 0x03ff);
}

/* setup the sprite image and palette */
void setup_sprite_image() {
	/* load the palette from the image into palette memory*/
	memcpy16_dma((unsigned short*) sprite_palette, (unsigned short*) spritesheet_palette, PALETTE_SIZE);

	/* load the image into sprite image memory */
	memcpy16_dma((unsigned short*) sprite_image_memory, (unsigned short*) spritesheet_data, (spritesheet_width * spritesheet_height) / 2);
}

/* a struct for Falco's logic and behavior */
struct Falco {
	/* the actual sprite attribute info */
	struct Sprite* sprite;

	/* the x and y postion, in 1/256 pixels */
	int x, y;

	/* the falco's y velocity in 1/256 pixels/second */
	int yvel;

	/* the falco's y acceleration in 1/256 pixels/second^2 */
	int gravity; 

	/* which frame of the animation he is on */
	int frame;

	/* the number of frames to wait before flipping */
	int animation_delay;

	/* the animation counter counts how many frames until we flip */
	int counter;

	/* whether the falco is moving right now or not */
	int move;

	/* the number of pixels away from the edge of the screen the falco stays */
	int border;

	/* if the falco is currently falling */
	int falling;

	//if falco is facing left = 0, right = 1
	int facing;

	//Score of how many times he shot a shyguy
	int score;
};

struct Shyguy {
	struct Sprite* sprite;
	int x, y, origx;
	int xvel;
	int gravity;
	int frame;
	int animation_delay;
	int counter;
	int move;
	int border;
	int falling;
	int facing;
};

struct Laser {
	struct Sprite* sprite;
	int x, y;
	int frame;
	int move;
	int border;
	int facing;
};

struct Score {
	struct Sprite* sprite;
	int x, y;
	int frame;
	int border;
};

void score_init(struct Score* score){
	score->x = 210 << 8;
	score->y = 5 << 8;
	
	score->frame = 464;
	score->border = 20;
	score->sprite = sprite_init(score->x >> 8, score->y >> 8, SIZE_32_32, 0, 0, 464, 0);	
}

/* initialize the falco */
void falco_init(struct Falco* falco) {
	falco->x = 100 << 8;
	falco->y = 113 << 8;
	falco->yvel = 0;
	falco->gravity = 50;
	falco->border = 40;
	falco->frame = 112;
	falco->move = 0;
	falco->counter = 0;
	falco->falling = 0;
	falco->animation_delay = 8;
	falco->facing = 1;
	falco->score = 0;
	falco->sprite = sprite_init(falco->x >> 8, falco->y >> 8, SIZE_32_32, 0, 0, 112, 0);
}

/* initialize the Shy guy */
void shyguy_init(struct Shyguy* shyguy, int xcoordinate){
	shyguy->origx = xcoordinate << 8;
	shyguy->x = xcoordinate << 8;
	shyguy->y = 113 << 8;
	shyguy->xvel = 64;
	shyguy->gravity = 50;
	shyguy->border = 40;
	shyguy->frame = 0;
	shyguy->move = 1;
	shyguy->counter = 0;
	shyguy->falling = 0;
	shyguy->animation_delay = 8;
	shyguy->facing = 0;
	shyguy->sprite = sprite_init(shyguy->x >> 8, shyguy->y >> 8, SIZE_32_32, 0, 0, 0, 0);
}

void laser_init(struct Laser* laser){
	laser->x = 240;
	laser->y = 160;
	laser->border = 40;
	laser->frame = 0;
	laser->move = 0;
	laser->sprite = sprite_init(laser->x, laser->y, SIZE_32_16, 0, 0, 96, 0);
	laser->facing = 1;
}

/* move the falco left or right returns if it is at edge of the screen */
int falco_left(struct Falco* falco) {
	/* face left */
	sprite_set_horizontal_flip(falco->sprite, 1);
	falco->move = 1;
	falco->facing = 0;
	/* if we are at the left end, just scroll the screen */
	if ((falco->x >> 8) < falco->border) {
		return 1;
	} else {
		/* else move left */
		falco->x -= 256;
		return 0;
	}
}

int falco_right(struct Falco* falco) {
	/* face right */
	sprite_set_horizontal_flip(falco->sprite, 0);
	falco->move = 1;
	falco->facing = 1;
	/* if we are at the right end, just scroll the screen */
	if ((falco->x >> 8) > (SCREEN_WIDTH - 16 - falco->border)) {
		return 1;
	} else {
		/* else move right */
		falco->x += 256;
		return 0;
	}
}

/* stop the falco from walking left/right */
void falco_stop(struct Falco* falco) {
	falco->move = 0;
   // falco->frame = 64;
	falco->counter = 7;
	sprite_set_offset(falco->sprite, 64);
}

/* start the falco jumping, unless already fgalling */
void falco_jump(struct Falco* falco) {
	if (!falco->falling) {
		falco->yvel = -1350;
		falco->falling = 1;
	}
}

void shyguy_move(struct Shyguy* shyguy, struct Falco* falco){
	if(falco->x > shyguy->x){
		sprite_set_horizontal_flip(shyguy->sprite, 0);
		shyguy->x += 128;
		shyguy->move = 1;
	}
	else if(falco->x < shyguy->x){
		shyguy->facing = 1;
		sprite_set_horizontal_flip(shyguy->sprite, 1);
		shyguy->x -= 128;
		shyguy->move = 1;
	}
	else{
		shyguy->move = 0;
	}
}

/* finds which tile a screen coordinate maps to, taking scroll into account */
unsigned short tile_lookup(int x, int y, int xscroll, int yscroll, const unsigned short* tilemap, int tilemap_w, int tilemap_h) {

	/* adjust for the scroll */
	x += xscroll;
	y += yscroll;

	/* convert from screen coordinates to tile coordinates */
	x >>= 3;
	y >>= 3;

	/* account for wraparound */
	while (x >= tilemap_w) {
		x -= tilemap_w;
	}
	while (y >= tilemap_h) {
		y -= tilemap_h;
	}
	while (x < 0) {
		x += tilemap_w;
	}
	while (y < 0) {
		y += tilemap_h;
	}

	/* lookup this tile from the map */
	int index = y * tilemap_w + x;

	/* return the tile */
	return tilemap[index];
}

/* collision detection, offscreen detection, and moving the laser forward */
void laser_update(struct Laser* laser, struct Shyguy* shyguy, struct Falco* falco){
	if((laser->x >> 8) >= 230 || (laser->x >> 8) < 5){
		laser->move = 0;
		laser->x = 240;
		laser->y = 160;
		sprite_position(laser->sprite, laser->x, laser->y);
	}
	else if(laser->move == 1){
		if(shyguy->x > laser->x && laser->facing == 0 && ((laser->y >> 8) + 12 > shyguy->y >> 8) && (falco->x > shyguy->x)){
			laser->x = 240;
			laser->y = 160;
			laser->move = 0;
			sprite_position(laser->sprite, laser->x, laser->y);
			falco->score += 1;
			
			shyguy->x = shyguy->origx;
			shyguy->y = 113 << 8;
			sprite_position(shyguy->sprite,shyguy->origx >> 8,shyguy->y >> 8);
		}
		else if(laser->x > shyguy->x && (laser->facing == 1) && ((laser->y >> 8) + 12 > shyguy->y >> 8) && (falco->x < shyguy->x)){
			laser->x = 240;
			laser->y = 160;
			laser->move = 0;
			sprite_position(laser->sprite, laser->x, laser->y);
			falco->score += 1;
		
			shyguy->x = shyguy->origx;
			shyguy->y = 113 << 8;
			sprite_position(shyguy->sprite,shyguy->origx >> 8 ,shyguy->y >> 8);
		}
		else {
			if(laser->facing == 1){
				laser->x += 512;
			}
			else{
				laser->x -= 512;
			}
			sprite_position(laser->sprite, laser->x >> 8, (laser->y >> 8) + 12);
		}
		
	}
}

void laser_shoot(struct Laser* laser, struct Falco* falco){
	laser->facing = falco->facing;
	laser->x = falco->x;
	laser->y = falco->y;
	laser->move = 1;
	sprite_position(laser->sprite, (laser->x >> 8) + 5, (laser->y >> 8) + 12);
	/*update animation when shooting */
}

/* update the falco */
void falco_update(struct Falco* falco, int xscroll) {
	/* update y position and speed if falling */
	if (falco->falling) {
		falco->y += falco->yvel;
		falco->yvel += falco->gravity;
	}

	/* check which tile the falco's feet are over */
	unsigned short tile = tile_lookup((falco->x >> 8) + 8, (falco->y >> 8) + 32, xscroll,
			0, map, map_width, map_height);

	/* if it's block tile
	 * these numbers refer to the tile indices of the blocks the falco can walk on */
	if ((tile >= 1 && tile <= 6) || 
		(tile >= 12 && tile <= 17)) {
		/* stop the fall! */
		falco->falling = 0;
		falco->yvel = 0;

		/* make him line up with the top of a block
		 * works by clearing out the lower bits to 0 */
		falco->y &= ~0x7ff;

		/* move him down one because there is a one pixel gap in the image */
		falco->y++;

	} else {
		/* he is falling now */
		falco->falling = 1;
	}
	/* update animation if moving */
	if (falco->move) {
		falco->counter++;
		if (falco->counter >= falco->animation_delay) {
			falco->frame = falco->frame + 32;
			if (falco->frame > 144) {
				falco->frame = 112;
			}
			sprite_set_offset(falco->sprite, falco->frame);
			falco->counter = 0;
		}
	}
	/* set on screen position */
	sprite_position(falco->sprite, falco->x >> 8, falco->y >> 8);
}

void shyguy_update(struct Shyguy* shyguy, int xscroll){
	/*update animation if moving */
	if(shyguy->move == 1){
		shyguy->counter++;
		if(shyguy->counter >= shyguy->animation_delay) {
			shyguy->frame = shyguy->frame + 32;
			if (shyguy->frame > 32) {
				shyguy->frame = 0;
			} 
			sprite_set_offset(shyguy->sprite, shyguy->frame);
			shyguy->counter = 0;
		}     
	   sprite_position(shyguy->sprite, shyguy->x>>8, shyguy->y >>8);
	}
}

// Declaring the assembly function
int checkscore(int score);

void score_update(struct Score* score, struct Falco* falco){

	//Calling the assembly function, sets the correct sprite frame based on score
	int frame = checkscore(falco->score);
	sprite_set_offset(score->sprite, frame);
}

/* Assembly function is dead */
int iskill(int a, int b, int c);

int isdead(struct Shyguy* shyguy, struct Falco* falco) {
	int falcoy = falco->y >> 8;
	int falcox = falco->x;
	int shyguyy = shyguy->y >> 8;
	int shyguyx = shyguy->x;
	
	int ded = iskill(falcox, shyguyx, falcoy);
	return ded; 
}

/* the main function */
int main() {
	/* we set the mode to mode 0 with bg0 on */
	*display_control = MODE0 | BG0_ENABLE | BG1_ENABLE | SPRITE_ENABLE | SPRITE_MAP_1D;

	/* setup the background 0 */
	setup_background();

	/* setup the sprite image data */
	setup_sprite_image();

	/* clear all the sprites on screen now */
	sprite_clear();

	/* create the falco */
	struct Falco falco;
	falco_init(&falco);
	/* create the shyguy */
	struct Shyguy shyguy;
	struct Shyguy shyguy2;
	shyguy_init(&shyguy, 20);
	shyguy_init(&shyguy2, 200);
	/* create a laser */
	struct Laser laser;
	laser_init(&laser);
	struct Score score;
	score_init(&score);
	
	/* set initial scroll to 0 */
	int xscroll = 0;
	int dead = 0;
	int kills = 0;
	
	/* loop forever */
	while (dead == 0 && kills < 10) {
		/* update the falco */
		kills = falco.score;
		
		falco_update(&falco, xscroll);
		/*update the shyguy */
		shyguy_update(&shyguy, xscroll);
		shyguy_update(&shyguy2, xscroll);
		/*update the laser */
		
		laser_update(&laser, &shyguy, &falco);
		laser_update(&laser, &shyguy2, &falco);
		score_update(&score, &falco);

		if(isdead(&shyguy, &falco) || isdead(&shyguy2, &falco)){
			dead = 1;
		}

		/* now the arrow keys move the falco */
		if (button_pressed(BUTTON_RIGHT)) {
			if (falco_right(&falco)) {
				xscroll++;
			}
		} else if (button_pressed(BUTTON_LEFT)) {
			if (falco_left(&falco)) {
				xscroll--;
			}
		} else {
			falco_stop(&falco);
		}

		/* check for jumping */
		
		if (button_pressed(BUTTON_A)) {
			falco_jump(&falco);
		}
	
		if (button_pressed(BUTTON_B)){
			laser_shoot(&laser, &falco);
		}

		shyguy_move(&shyguy, &falco);
		shyguy_move(&shyguy2, &falco);


		/* wait for vblank before scrolling and moving sprites */
		wait_vblank();
		*bg0_x_scroll = xscroll;
		sprite_update_all();

		/* delay some */
		delay(300);
	}
	/* when you lose */
	while(1){
		if(kills == 10){
			memcpy16_dma((unsigned short*) screen_block(24), (unsigned short*) map4, map4_width * map4_height);
		} else {
			memcpy16_dma((unsigned short*) screen_block(24), (unsigned short*) map3, map3_width * map3_height); 
		}
	}
}

/* the game boy advance uses "interrupts" to handle certain situations
 * for now we will ignore these */
void interrupt_ignore() {
	/* do nothing */
}

/* this table specifies which interrupts we handle which way
 * for now, we ignore all of them */
typedef void (*intrp)();
const intrp IntrTable[13] = {
	interrupt_ignore,   /* V Blank interrupt */
	interrupt_ignore,   /* H Blank interrupt */
	interrupt_ignore,   /* V Counter interrupt */
	interrupt_ignore,   /* Timer 0 interrupt */
	interrupt_ignore,   /* Timer 1 interrupt */
	interrupt_ignore,   /* Timer 2 interrupt */
	interrupt_ignore,   /* Timer 3 interrupt */
	interrupt_ignore,   /* Serial communication interrupt */
	interrupt_ignore,   /* DMA 0 interrupt */
	interrupt_ignore,   /* DMA 1 interrupt */
	interrupt_ignore,   /* DMA 2 interrupt */
	interrupt_ignore,   /* DMA 3 interrupt */
	interrupt_ignore,   /* Key interrupt */
};

