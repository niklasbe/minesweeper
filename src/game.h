#ifndef GAME_H
#define GAME_H

////////////////////////////////
//~ nb: Mouse Input 
// Left click to sweep, right click to plant a flag.
// NOTE(nb): Left click should operate on BUTTONUP 
// to avoid accidental sweeps.
typedef enum
{
	LEFT_CLICK,
	RIGHT_CLICK
} MouseButton;

struct Tile
{
	u32               neighbor_count = 0;
	bool              has_flag       = false;
	bool              is_mine        = false;
	bool              is_swept       = false;
	
	DirectX::XMFLOAT2 sprite; // uv offset
};

struct Game
{
	////////////////////////////////
	// nb: Arenas
	Arena         *arena_main;
	Arena         arena_scratch;
	Arena         arena_frame;
	Arena         arena_level;
	
	
	////////////////////////////////
	u32           spritesheet_id;
	u32           *floodfill_queue;
	u32           floodfill_queue_count = 0;
	u32           *mine_indices;
	
	////////////////////////////////
	// nb: Variables
	bool          is_playable;
	f64           elapsed_time;
	u32           mine_count;
	u32           flag_count;
	u32           columns;
	u32           rows;
	Tile          *tiles;
	u32           tiles_count;
};


void game_init(Arena *arena);
void game_destroy();

void game_set_window(void *window_handle, u32 width, u32 height);
void game_mouse_up(u32 x, u32 y);
void game_mouse_down(u32 x, u32 y);
void game_size_changed(u32 w, u32 h);

void game_gameover();

void game_reset();
void game_render();

global Game *g_game = {0};;

#endif //GAME_H
