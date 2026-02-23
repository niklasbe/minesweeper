#ifndef GAME_H
#define GAME_H

////////////////////////////////
//~ nb: Mouse Input 
// Left click to sweep, right click to plant a flag.
// NOTE(nb): Left click should operate on BUTTONUP 
// to avoid accidental sweeps.
typedef enum MouseButton MouseButton;
enum MouseButton
{
	LEFT_CLICK,
	RIGHT_CLICK
};

typedef struct Tile Tile;
struct Tile
{
	u32               neighbor_count = 0;
	bool              has_flag       = false;
	bool              is_mine        = false;
	bool              is_swept       = false;
	
	DirectX::XMFLOAT2 sprite; // uv offset
};

typedef struct Camera Camera;
struct Camera
{
	f32 x, y;
	f32 zoom;
};

// TODO(nb): make a system for this
#define TILE_SIZE 32

typedef struct Game Game;
struct Game
{
	
	////////////////////////////////
	// nb: Arenas
	Arena         *arena;
	Arena         scratch_arena;
	Arena         frame_arena;
	Arena         level_arena;
	
	
	////////////////////////////////
	R_Handle      spritesheet_handle;
	u32           *floodfill_queue;
	u32           floodfill_queue_count = 0;
	u32           *mine_indices;
	// nb: if first sweep protection happened, store the idx of the mine 
	u32           first_sweep_protection_idx;
	
	////////////////////////////////
	// nb: Variables
	Camera        camera;
	bool          is_playable;
	f64           elapsed_time;
	u32           mine_count;
	u32           swept_count;
	u32           flag_count;
	u32           columns;
	u32           rows;
	Tile          *tiles;
	u32           tiles_count;
};


void game_init(Arena *arena);
void game_destroy();

void game_set_window(void *window_handle, u32 width, u32 height);
void game_on_mouse_up(u32 x, u32 y);
void game_on_mouse_down(u32 x, u32 y);
void game_size_changed(u32 w, u32 h);

void game_gameover();

void game_reset();
void game_render();

////////////////////////////////
//~ nb: Helper functions
internal void game_get_neighbors(u32 tile_x, u32 tile_y, u32 *neighbor_idx_list, u32 *neighbor_idx_list_count);
internal void game_get_neighbors_by_idx(u32 idx, u32 *neighbor_idx_list, u32 *neighbor_idx_list_count);
internal Tile *game_get_tile(u32 tile_x, u32 tile_y);
internal Tile *game_get_tile_by_idx(u32 idx);
internal u32  game_get_idx_by_screen_pos(u32 screen_x, u32 screen_y);
internal Tile *game_get_tile_by_screen_pos(u32 screen_x, u32 screen_y);
internal bool game_reveal_tile(u32 tile_x, u32 tile_y);
internal bool game_reveal_tile_by_idx(u32 idx);

global Tile game_tile_nil = {0};
global Game *g_game = {0};;

#endif //GAME_H
