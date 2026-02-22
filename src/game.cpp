#include "game.h"

#include <time.h>

internal void game_get_neighbors(u32 tile_x, u32 tile_y, u32 *neighbor_idx_list, u32 *neighbor_idx_list_count);
internal void game_get_neighbors_by_idx(u32 idx, u32 *neighbor_idx_list, u32 *neighbor_idx_list_count);
internal Tile *game_get_tile(u32 tile_x, u32 tile_y);
internal Tile *game_get_tile_by_idx(u32 idx);
internal u32  game_get_idx_by_screen_pos(u32 screen_x, u32 screen_y);
internal Tile *game_get_tile_by_screen_pos(u32 screen_x, u32 screen_y);
internal bool game_reveal_tile(u32 tile_x, u32 tile_y);
internal bool game_reveal_tile_by_idx(u32 idx);


////////////////////////////////
//~ nb: Internal functions
internal void 
game_get_neighbors_by_idx(u32 idx, u32 neighbor_idx_list[8], u32 *neighbor_idx_list_count)
{
	u32 tile_x = idx % g_game->columns;
	u32 tile_y = idx / g_game->columns;
	game_get_neighbors(tile_x, tile_y, neighbor_idx_list, neighbor_idx_list_count);
}

////////////////////////////////
// nb: This table shows the corresponding 1D array neighbor mappings
//       1D ARRAY                2D ARRAY
// [-W -1] [-W] [-W +1]  [-1, -1] [0, -1] [1, -1]
// [   -1] [ n] [   +1]  [-1,  0] [    n] [1,  0]
// [+W -1] [+W] [+W +1]  [-1,  1] [0,  1] [1,  1]
////////////////////////////////
internal void
game_get_neighbors(u32 tile_x, u32 tile_y, u32 neighbor_idx_list[8], u32 *neighbor_idx_list_count)
{
	u32 count = 0;
	
	for (int dy = -1; dy <= 1; dy++) 
	{
		for (int dx = -1; dx <= 1; dx++) 
		{
			// nb: skip current tile
			if (dx == 0 && dy == 0) 
				continue;
			
			int nx = tile_x + dx;
			int ny = tile_y + dy;
			
			// nb: bounds check
			if (nx >= 0 && nx < g_game->columns && ny >= 0 && ny < g_game->rows) 
			{
				u32 neighbor_idx = ny * g_game->columns + nx;
				neighbor_idx_list[count] = neighbor_idx;
				count++;
			}
		}
	}
	*neighbor_idx_list_count = count;
}

internal Tile *
game_get_tile(u32 tile_x, u32 tile_y)
{
	ASSERT(tile_x < g_game->columns && tile_y < g_game->rows);
	u32 idx = tile_y * g_game->columns + tile_x;
	return game_get_tile_by_idx(idx);
}

internal Tile *
game_get_tile_by_idx(u32 idx)
{
	ASSERT(idx < g_game->tiles_count);
	return &g_game->tiles[idx];
}

internal u32
game_get_idx_by_screen_pos(u32 screen_x, u32 screen_y)
{
	u32 tile_x = screen_x / g_game->camera.zoom / TILE_SIZE;
	u32 tile_y = screen_y / g_game->camera.zoom / TILE_SIZE;
	u32 idx = tile_y * g_game->columns + tile_x;
	return idx;
}

internal Tile *
game_get_tile_by_screen_pos(u32 screen_x, u32 screen_y)
{
	u32 tile_x = screen_x / g_game->camera.zoom / TILE_SIZE;
	u32 tile_y = screen_y / g_game->camera.zoom / TILE_SIZE;
	u32 idx = tile_y * g_game->columns + tile_x;
	if(tile_x >= g_game->columns || tile_y >= g_game->rows)
		return &game_tile_nil;
	return game_get_tile_by_idx(idx);
}


////////////////////////////////
//~ nb: Game functions
void 
game_init(Arena *arena)
{
	g_game = (Game*)arena_push(arena, sizeof(Game));
	g_game->arena = arena;
	
	//- nb: Arenas
	void *scratch_ptr = (void*)arena_push(g_game->arena, Megabytes(4));
	void *frame_ptr = (void*)arena_push(g_game->arena, Megabytes(4));
	void *level_ptr = (void*)arena_push(g_game->arena, Megabytes(4));
	arena_create(&g_game->scratch_arena, Megabytes(4), (char*)scratch_ptr);
	arena_create(&g_game->frame_arena, Megabytes(4), (char*)frame_ptr);
	arena_create(&g_game->level_arena, Megabytes(4), (char*)level_ptr);
	////////////////////////////////
	
	game_reset();
	r_init(g_game->arena);
	
	g_game->camera          = {0};
	g_game->camera.zoom     = 1.0f;
	g_game->camera.rotation = 1.0f;
	
	//- nb: Resources
	g_game->spritesheet_handle  = r_load_texture(L"sheet.png", &g_game->scratch_arena);
	g_game->floodfill_queue = (u32*)arena_push(&g_game->level_arena, g_game->tiles_count);
}

void 
game_destroy()
{
	r_destroy();
}

void 
game_set_window(void *window_handle, u32 width, u32 height)
{
	r_set_window(window_handle, width, height);
}

void 
game_on_mouse_down(MouseButton button, u32 x, u32 y)
{
	//- nb: Get tile index
	Tile *tile = game_get_tile_by_screen_pos(x, y);
	
	switch(button)
	{
		case LEFT_CLICK:
		break;
		
		case RIGHT_CLICK:
		// nb: Don't allow a flag to be placed on a swept mine
		if(tile->is_swept)
			break;
		
		// nb: Place flag
		if(!tile->has_flag)
		{
			tile->has_flag = true;
			tile->sprite = sprites[TILE_FLAG];
		}
		else
		{
			tile->has_flag = false;
			tile->sprite = sprites[TILE_DEFAULT];
		}
		break;
	}
	
}

void 
game_on_mouse_up(MouseButton button, u32 x, u32 y)
{
	if(!g_game->is_playable)
	{
		game_reset();
		return;
	}
	
	//- nb: Get tile index
	u32 idx = game_get_idx_by_screen_pos(x, y);
	
	switch(button)
	{
		case LEFT_CLICK:
		if(game_reveal_tile_by_idx(idx))
			game_gameover();
		break;
		////////////////////////////////
		case RIGHT_CLICK:
		break;
	}
}

void 
game_on_size_changed(u32 width, u32 height)
{
	r_window_size_changed(width, height);
}

void 
game_reset()
{
	arena_clear(&g_game->scratch_arena);
	arena_clear(&g_game->level_arena);
	g_game->floodfill_queue_count = 0;
	
	////////////////////////////////
	//- nb: Default values
	g_game->is_playable       = true;
	g_game->columns           = 30;
	g_game->rows              = 16;
	g_game->mine_count        = 90;
	g_game->flag_count        = 0;
	g_game->tiles = (Tile*)arena_push(&g_game->level_arena, sizeof(Tile) * g_game->rows * g_game->columns);
	g_game->tiles_count = g_game->columns * g_game->rows;
	
	// nb: Index array for shuffling, used for mine selection
	g_game->mine_indices = (u32*)arena_push(&g_game->level_arena, (sizeof(u32) * g_game->tiles_count));
	
	// nb: Populate board
	for(int i = 0; i < g_game->tiles_count; i++)
	{
		Tile tile;
		tile.sprite = sprites[TILE_DEFAULT];
		g_game->tiles[i] = tile;
		// nb: Populate index array
		g_game->mine_indices[i] = i;
	}
	
	////////////////////////////////
	//- nb: Pick mines at random
	srand(time(NULL));
	// shuffle index list
	for(int i = g_game->tiles_count - 1; i > 0; i--)
	{
		int j = rand() % (i + 1);
		
		int temp = g_game->mine_indices[i];
		g_game->mine_indices[i] = g_game->mine_indices[j];
		g_game->mine_indices[j] = temp;
	}
	// nb: Select n mines at random
	for(int i = 0; i < g_game->mine_count; i++)
	{
		int index = g_game->mine_indices[i];
		Tile &tile = g_game->tiles[index];
		tile.is_mine = true;
		//tile.sprite = sprites[TILE_MINE];
	}
	
	////////////////////////////////
	//- nb: Set the neighboring mine count for all tiles
	for(int i = 0; i < g_game->mine_count; i++)
	{
		u32 neighbor_idx_list[8] = {0};
		u32 neighbor_idx_list_count = 0;
		game_get_neighbors_by_idx(g_game->mine_indices[i], neighbor_idx_list, &neighbor_idx_list_count);
		for(int j = 0; j < neighbor_idx_list_count; j++)
		{
			if(!g_game->tiles[neighbor_idx_list[j]].is_mine)
				g_game->tiles[neighbor_idx_list[j]].neighbor_count++;
		}
	}
}


internal bool 
game_reveal_tile_by_idx(u32 idx)
{
	Tile &tile = g_game->tiles[idx];
	
	if(tile.is_swept)
	{
		// Nothing to chord
		if(tile.neighbor_count == 0)
			return false;
	}
	
	// nb: Disallow a flagged tile from being swept
	if(tile.has_flag)
		return false;
	
	if(tile.is_mine)
	{
		tile.sprite = sprites[TILE_MINERED];
		return true;
	};
	
	if(!tile.is_swept)
	{
		tile.is_swept = true;
		if(tile.neighbor_count == 0)
		{
			tile.sprite = sprites[TILE_EMPTY];
			g_game->floodfill_queue[g_game->floodfill_queue_count] = idx;
			g_game->floodfill_queue_count++;
		} 
		else
		{
			tile.sprite = sprites[tile.neighbor_count - 1];
			return false;
		}
	}
	else
	{
		//- nb: Chording logic
		u32 flag_count = 0;
		u32 neighbor_idx_list[8] = {0};
		u32 neighbor_idx_list_count = 0;
		game_get_neighbors_by_idx(idx, neighbor_idx_list, &neighbor_idx_list_count);
		
		// nb: Count the neighboring flags
		for(int i = 0; i < neighbor_idx_list_count; i++)
		{
			if(g_game->tiles[neighbor_idx_list[i]].has_flag)
				flag_count++;
		}
		if(flag_count == tile.neighbor_count)
		{
			for(int i = 0; i < neighbor_idx_list_count; i++)
			{
				Tile &nb = g_game->tiles[neighbor_idx_list[i]];
				
				if(!nb.is_swept && !nb.has_flag)
				{
					if (game_reveal_tile_by_idx(neighbor_idx_list[i]))
						return true;
				}
			}
		}
		
	}
	
	////////////////////////////////
	//- nb: Flood fill
	while(g_game->floodfill_queue_count > 0)
	{
		// nb: Pop a tile
		u32 tile_idx = g_game->floodfill_queue[g_game->floodfill_queue_count - 1];
		g_game->floodfill_queue_count--;
		
		// nb: Sweep the current tile
		g_game->tiles[tile_idx].is_swept = true;
		g_game->tiles[tile_idx].sprite = sprites[TILE_EMPTY];
		
		u32 neighbor_idx_list[8] = {0};
		u32 neighbor_idx_list_count = 0;
		game_get_neighbors_by_idx(tile_idx, neighbor_idx_list, &neighbor_idx_list_count);
		// nb: Sweep every neighboring tile
		for(int i = 0; i < neighbor_idx_list_count; i++)
		{
			Tile &neighbor = g_game->tiles[neighbor_idx_list[i]];
			if(neighbor.is_mine || neighbor.is_swept)
				continue;
			
			neighbor.is_swept = true;
			
			// nb: Keep filling until there are no more tiles with 0 neighbors
			if(neighbor.neighbor_count == 0)
			{
				neighbor.sprite = sprites[TILE_EMPTY];
				g_game->floodfill_queue[g_game->floodfill_queue_count] = neighbor_idx_list[i];
				g_game->floodfill_queue_count++;
			}else
			{
				// nb: We can use [neighbor_count - 1] to set the sprite,
				// as the sprite sheet is logically set up in such a way
				// that the first sprite is "1", second sprite is "2", etc.
				neighbor.sprite = sprites[neighbor.neighbor_count - 1];
			}
		}
	}
	return false;
}

internal bool 
game_reveal_tile(u32 tile_x, u32 tile_y)
{
	u32 idx = tile_y * g_game->columns + tile_x;
	return game_reveal_tile_by_idx(idx);
}

void 
game_gameover()
{
	// nb: Reveal all mines
	for(int i = 0; i < g_game->mine_count; i++)
	{
		g_game->tiles[g_game->mine_indices[i]].sprite = sprites[TILE_MINE];
	}
	g_game->is_playable = false;
};


void 
game_render()
{
	arena_clear(&g_game->frame_arena);
	const float color[4]{0.25f, 0.25f, 0.25f, 1.0f};
	r_clear(color);
	
	//- nb: submit board batch to GPU
	InstanceData *instance_data = (InstanceData*)arena_push(&g_game->frame_arena, sizeof(InstanceData) * g_game->tiles_count);
	for (int i = 0; i < g_game->tiles_count; i++)
	{
		int x = i % g_game->columns;
		int y = i / g_game->columns;
		
		Tile &tile = g_game->tiles[i];
		instance_data[i] = { {(float)x * TILE_SIZE,(float)y * TILE_SIZE}, tile.sprite, {TILE_SIZE, TILE_SIZE}};
	}
	
	r_submit_batch(instance_data, g_game->tiles_count, g_game->spritesheet_handle.U32[0]);
	r_present();
}