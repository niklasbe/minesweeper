#include "game.h"

#include <time.h>



internal void game_get_neighbors(Game *, u32, u32, u32 *, u32 *);
internal void game_get_neighbors_by_idx(Game *, u32, u32 *, u32 *);
internal Tile *game_get_tile(Game *, u32, u32);
internal Tile *game_get_tile_by_idx(Game *, u32);
internal bool game_reveal_tile(Game *, u32, u32);
internal bool game_reveal_tile_by_idx(Game *, u32);


////////////////////////////////
//~ nb: Internal functions
internal void 
game_get_neighbors_by_idx(Game *game, u32 idx, u32 neighbor_idx_list[8], u32 *neighbor_idx_list_count)
{
	u32 tile_x = idx % game->columns;
	u32 tile_y = idx / game->columns;
	game_get_neighbors(game, tile_x, tile_y, neighbor_idx_list, neighbor_idx_list_count);
}

////////////////////////////////
// nb: This table shows the corresponding 1D array neighbor mappings
//       1D ARRAY                2D ARRAY
// [-W -1] [-W] [-W +1]  [-1, -1] [0, -1] [1, -1]
// [   -1] [ n] [   +1]  [-1,  0] [    n] [1,  0]
// [+W -1] [+W] [+W +1]  [-1,  1] [0,  1] [1,  1]
////////////////////////////////
internal void
game_get_neighbors(Game *game, u32 tile_x, u32 tile_y, u32 neighbor_idx_list[8], u32 *neighbor_idx_list_count)
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
			if (nx >= 0 && nx < game->columns && ny >= 0 && ny < game->rows) 
			{
				u32 neighbor_idx = ny * game->columns + nx;
				neighbor_idx_list[count] = neighbor_idx;
				count++;
			}
		}
	}
	*neighbor_idx_list_count = count;
}

internal Tile *
game_grid_to_tile(Game *game, u32 tile_x, u32 tile_y)
{
	ASSERT(tile_x < game->columns && tile_y < game->rows);
	u32 idx = tile_y * game->columns + tile_x;
	return game_get_tile_by_idx(game, idx);
}

internal Tile *
game_get_tile_by_idx(Game *game, u32 idx)
{
	ASSERT(idx < game->tiles_count);
	return &game->tiles[idx];
}


////////////////////////////////
//~ nb: Game functions
void 
game_init(Game *game)
{
	//- nb: Arenas
	void *scratch_ptr = (void*)arena_push(game->arena_main, Megabytes(4));
	void *frame_ptr = (void*)arena_push(game->arena_main, Megabytes(4));
	void *level_ptr = (void*)arena_push(game->arena_main, Megabytes(4));
	arena_create(&game->arena_scratch, Megabytes(4), (char*)scratch_ptr);
	arena_create(&game->arena_frame, Megabytes(4), (char*)frame_ptr);
	arena_create(&game->arena_level, Megabytes(4), (char*)level_ptr);
	////////////////////////////////
	game_reset(game);
	
	game->renderer = (R_D3D11_State*)arena_push(game->arena_main, sizeof(R_D3D11_State));
	//r_init(game->renderer);
	game->renderer->Init();
	
	//- nb: Resources
	game->spritesheetID = game->renderer->LoadTexture(L"sheet.png", game->arena_scratch);
	game->floodfill_queue = (u32*)arena_push(&game->arena_level, game->tiles_count);
}

void 
game_destroy(Game *game)
{
	game->renderer->Destroy();
}

void 
game_set_window(Game *game, void *window_handle, u32 width, u32 height)
{
	game->renderer->SetWindow(window_handle, width, height);
}

void 
game_on_mouse_down(Game *game, MouseButton button, u32 x, u32 y)
{
	//- nb: Get tile index
	int tile_x = x / 32;
	int tile_y = y / 32;
	int idx = tile_y * game->columns + tile_x;
	
	if(tile_x >= game->columns || tile_y >= game->rows)
		return;
	
	Tile *tile = game_get_tile_by_idx(game, idx);
	
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
game_on_mouse_up(Game *game, MouseButton button, u32 x, u32 y)
{
	if(!game->is_playable)
	{
		game_reset(game);
		return;
	}
	
	//- nb: Get tile index
	int tile_x = x / 32;
	int tile_y = y / 32;
	int idx = tile_y * game->columns + tile_x;
	
	if(tile_x >= game->columns || tile_y >= game->rows)
		return;
	
	switch(button)
	{
		case LEFT_CLICK:
		if(game_reveal_tile_by_idx(game, idx))
			game_gameover(game);
		break;
		////////////////////////////////
		case RIGHT_CLICK:
		Tile *tile = game_grid_to_tile(game, tile_x, tile_y);
		char buff[64];
		sprintf_s(buff, 64, "neighbors: %d", tile->neighbor_count);
		OutputDebugString(buff);
		break;
	}
}

void 
game_on_size_changed(Game *game, u32 width, u32 height)
{
	game->renderer->WindowSizeChanged(width, height);
}

void 
game_reset(Game *game)
{
	arena_clear(&game->arena_scratch);
	arena_clear(&game->arena_level);
	game->floodfill_queue_count = 0;
	
	////////////////////////////////
	//- nb: Default values
	game->is_playable          = true;
	game->columns           = 30;
	game->rows              = 16;
	game->mine_count        = 90;
	game->flag_count        = 0;
	game->tiles = (Tile*)arena_push(&game->arena_level, sizeof(Tile) * game->rows * game->columns);
	game->tiles_count = game->columns * game->rows;
	
	// nb: Index array for shuffling, used for mine selection
	game->mine_indices = (u32*)arena_push(&game->arena_level, (sizeof(u32) * game->tiles_count));
	
	// nb: Populate board
	for(int i = 0; i < game->tiles_count; i++)
	{
		Tile tile;
		tile.sprite = sprites[TILE_DEFAULT];
		game->tiles[i] = tile;
		// nb: Populate index array
		game->mine_indices[i] = i;
	}
	
	////////////////////////////////
	//- nb: Pick mines at random
	srand(time(NULL));
	// shuffle index list
	for(int i = game->tiles_count - 1; i > 0; i--)
	{
		int j = rand() % (i + 1);
		
		int temp = game->mine_indices[i];
		game->mine_indices[i] = game->mine_indices[j];
		game->mine_indices[j] = temp;
	}
	// nb: Select n mines at random
	for(int i = 0; i < game->mine_count; i++)
	{
		int index = game->mine_indices[i];
		Tile &tile = game->tiles[index];
		tile.is_mine = true;
		//tile.sprite = sprites[TILE_MINE];
	}
	
	////////////////////////////////
	//- nb: Set the neighboring mine count for all tiles
	for(int i = 0; i < game->mine_count; i++)
	{
		u32 neighbor_idx_list[8] = {0};
		u32 neighbor_idx_list_count = 0;
		game_get_neighbors_by_idx(game, game->mine_indices[i], neighbor_idx_list, &neighbor_idx_list_count);
		for(int j = 0; j < neighbor_idx_list_count; j++)
		{
			if(!game->tiles[neighbor_idx_list[j]].is_mine)
				game->tiles[neighbor_idx_list[j]].neighbor_count++;
		}
	}
}


internal bool 
game_reveal_tile_by_idx(Game *game, u32 idx)
{
	Tile &tile = game->tiles[idx];
	
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
			game->floodfill_queue[game->floodfill_queue_count] = idx;
			game->floodfill_queue_count++;
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
		game_get_neighbors_by_idx(game, idx, neighbor_idx_list, &neighbor_idx_list_count);
		
		// nb: Count the neighboring flags
		for(int i = 0; i < neighbor_idx_list_count; i++)
		{
			if(game->tiles[neighbor_idx_list[i]].has_flag)
				flag_count++;
		}
		if(flag_count == tile.neighbor_count)
		{
			for(int i = 0; i < neighbor_idx_list_count; i++)
			{
				Tile &nb = game->tiles[neighbor_idx_list[i]];
				
				if(!nb.is_swept && !nb.has_flag)
				{
					if (game_reveal_tile_by_idx(game, neighbor_idx_list[i]))
						return true;
				}
			}
		}
		
	}
	
	////////////////////////////////
	//- nb: Flood fill
	while(game->floodfill_queue_count > 0)
	{
		// nb: Pop a tile
		u32 tile_idx = game->floodfill_queue[game->floodfill_queue_count - 1];
		game->floodfill_queue_count--;
		
		// nb: Sweep the current tile
		game->tiles[tile_idx].is_swept = true;
		game->tiles[tile_idx].sprite = sprites[TILE_EMPTY];
		
		u32 neighbor_idx_list[8] = {0};
		u32 neighbor_idx_list_count = 0;
		game_get_neighbors_by_idx(game, tile_idx, neighbor_idx_list, &neighbor_idx_list_count);
		// nb: Sweep every neighboring tile
		for(int i = 0; i < neighbor_idx_list_count; i++)
		{
			Tile &neighbor = game->tiles[neighbor_idx_list[i]];
			if(neighbor.is_mine || neighbor.is_swept)
				continue;
			
			neighbor.is_swept = true;
			
			// nb: Keep filling until there are no more tiles with 0 neighbors
			if(neighbor.neighbor_count == 0)
			{
				neighbor.sprite = sprites[TILE_EMPTY];
				game->floodfill_queue[game->floodfill_queue_count] = neighbor_idx_list[i];
				game->floodfill_queue_count++;
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
game_reveal_tile(Game *game, u32 tile_x, u32 tile_y)
{
	u32 idx = tile_y * game->columns + tile_x;
	return game_reveal_tile_by_idx(game, idx);
}

void 
game_gameover(Game *game)
{
	// nb: Reveal all mines
	for(int i = 0; i < game->mine_count; i++)
	{
		game->tiles[game->mine_indices[i]].sprite = sprites[TILE_MINE];
	}
	game->is_playable = false;
};


void 
game_render(Game *game)
{
	arena_clear(&game->arena_frame);
	
	const float color[4]{0.25f, 0.25f, 0.25f, 1.0f};
	game->renderer->Clear(color);
	
	
	//- nb: submit board batch to GPU
	InstanceData *instance_data = (InstanceData*)arena_push(&game->arena_frame, sizeof(InstanceData) * game->tiles_count);
	for (int i = 0; i < game->tiles_count; i++)
	{
		int x = i % game->columns;
		int y = i / game->columns;
		
		Tile &tile = game->tiles[i];
		instance_data[i] = { {(float)x,(float)y}, tile.sprite};
	}
	
	game->renderer->SubmitBatch(instance_data, game->tiles_count, game->spritesheetID);
	
	game->renderer->Present();
}