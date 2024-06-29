
// wonderswan colour parallax effect
// joe kennedj 2024

#include <wonderful.h>
#include <ws.h>
#include "graphics/bg.h"


#ifdef IRAM_IMPLEMENTATION
#define IRAM_EXTERN
#else
#define IRAM_EXTERN extern
#endif

#define screen_1 ((ws_scr_entry_t __wf_iram*) 0x1000)
#define screen_2 ((ws_scr_entry_t __wf_iram*) 0x1800)
#define sprites ((ws_sprite_t __wf_iram*) 0x2e00)

typedef struct {
	uint8_t x;			// current scroll x
	uint8_t line;		// line to split on
	uint8_t timer;		// update timer
	uint8_t timer_max;	// value to reset timer to
	uint8_t speed;		// value to add to scroll x
} split_t;

#define BG_SPLIT_COUNT 8

// definitions for split for the bg
const split_t bg_splits[BG_SPLIT_COUNT] = { 
	{.x = 0, .line = 0, .timer = 0, .timer_max = 255, .speed = 0 },	// sky
	{.x = 0, .line = 35, .timer = 0, .timer_max = 32, .speed = 1 },	// pale mountains
	{.x = 0, .line = 47, .timer = 0, .timer_max = 12, .speed = 1 }, // hills
	{.x = 0, .line = 63, .timer = 0, .timer_max = 4, .speed = 1 }, // forest
	{.x = 0, .line = 83, .timer = 0, .timer_max = 3, .speed = 1 }, // track
	{.x = 0, .line = 92, .timer = 0, .timer_max = 2, .speed = 1 }, // coast
	{.x = 0, .line = 103, .timer = 0, .timer_max = 1, .speed = 2 }, // trees
	{.x = 0, .line = 255, .timer = 0, .timer_max = 0, .speed = 0 }, // terminator
};

// current split which will be used in hblank
volatile split_t *scroll_x_split;

// all splits for this screen
volatile split_t scroll_x_splits[BG_SPLIT_COUNT];

// tracks whether the vblank has just fired
// used to disambiguate between vblank and hblank when using cpu_halt()
volatile uint8_t vblank_fired;

uint16_t tic;

__attribute__((interrupt)) void hblank(void) __far
{
	// update split's X scroll
	outportb(IO_SCR1_SCRL_X, scroll_x_split->x);
	
	// move pointer to next split
	scroll_x_split++;
	
	// if line != 0 then set the line interrupt line to the next split's line
	if (scroll_x_split->line)
	{
		outportb(IO_LCD_INTERRUPT, scroll_x_split->line);
	}

	ws_hwint_ack(HWINT_LINE);
}

__attribute__((interrupt)) void vblank(void) __far
{
	static uint8_t i;
	
	vblank_fired = 1;

	// point scroll_x_split at the first split
	scroll_x_split = scroll_x_splits;

	// go through all splits
	for (i = 0; i < BG_SPLIT_COUNT; i++)
	{
		// decrement timer
		scroll_x_split->timer--;

		// if the timer has hit 0, reset the timer and
		// add the split's speed to its x scroll
		if (scroll_x_split->timer == 0)
		{
			scroll_x_split->timer = scroll_x_split->timer_max;
			scroll_x_split->x += scroll_x_split->speed;
		}

		// move to next split
		scroll_x_split++;
	}
	
	// point scroll_x_split at the list of splits
	scroll_x_split = scroll_x_splits;
	
	// if first split's x scroll is 0, set the scroll x now
	// and move pointer to next split
	if (scroll_x_split->line == 0)
	{
		outportb(IO_SCR1_SCRL_X, scroll_x_split->x);
		scroll_x_split++;
	}
	
	// set up the interrupt for the next split
	outportb(IO_LCD_INTERRUPT, scroll_x_split->line);

	ws_hwint_ack(HWINT_VBLANK);
}

void disable_interrupts()
{
	// disable cpu interrupts
	cpu_irq_disable();

	// disable wonderswan hardware interrupts
	ws_hwint_disable_all();
}

void enable_interrupts()
{
	// acknowledge interrupt
	outportb(IO_HWINT_ACK, 0xFF);

	// set interrupt handlers for vblank and line interrupt
	ws_hwint_set_handler(HWINT_IDX_VBLANK, vblank);
	ws_hwint_set_handler(HWINT_IDX_LINE, hblank);
	
	// enable wonderswan vblank and line interrupt
	ws_hwint_enable(HWINT_VBLANK | HWINT_LINE);

	// set interrupt to trigger on a line value which will not be reached
	outportb(IO_LCD_INTERRUPT, 255);

	// enable cpu interrupts
	cpu_irq_enable();
}

void main(void)
{
	uint8_t i;
	
	disable_interrupts();

	ws_mode_set(WS_MODE_COLOR_4BPP);

	// disable all video output for now
	outportw(IO_DISPLAY_CTRL, 0);

	// set base addresses for screens 1 and 2
	outportb(IO_SCR_BASE, SCR1_BASE(screen_1) | SCR2_BASE(screen_2));

	// set sprite base address
	outportb(IO_SPR_BASE, SPR_BASE(sprites));

	// reset scroll registers to 0
	outportb(IO_SCR1_SCRL_X, 16);
	outportb(IO_SCR1_SCRL_Y, 0);
	outportb(IO_SCR2_SCRL_X, 0);
	outportb(IO_SCR2_SCRL_Y, 0);
	
	// don't render any sprites for now
	outportb(IO_SPR_COUNT, 0);

	// load palette
	ws_dma_copy_words(MEM_COLOR_PALETTE(0), gfx_bg_palette, gfx_bg_palette_size);

	// load gfx
	ws_dma_copy_words(MEM_TILE_4BPP(0), gfx_bg_tiles, gfx_bg_tiles_size);

	// load tilemap
	ws_dma_copy_words(screen_1, gfx_bg_map, gfx_bg_map_size);

	// enable just screen_1
	outportw(IO_DISPLAY_CTRL, DISPLAY_SCR1_ENABLE);

	// initialise screen splits from the array in rom
	for (i = 0; i < BG_SPLIT_COUNT; i++)
	{
		scroll_x_splits[i] = bg_splits[i];
		scroll_x_splits[i].timer = scroll_x_splits[i].timer_max;
	}

	tic = 0;
	vblank_fired = 0;

	enable_interrupts();

	while(1)
	{
		// keep halting until vblank is done
		while(!vblank_fired)
		{
			cpu_halt();
		}

		// update tic count
		tic++;

		// reset vblank flag
		vblank_fired = 0;
	}
}
