#include "game.h"

#include <time.h>

Game::Game(Arena &arena) : m_arena_main(&arena) 
{
	//- nb: Arenas
	void *scratch_ptr = (void*)arena_push(m_arena_main, Megabytes(4));
	void *frame_ptr = (void*)arena_push(m_arena_main, Megabytes(4));
	void *level_ptr = (void*)arena_push(m_arena_main, Megabytes(4));
	arena_create(&m_arena_scratch, Megabytes(4), (char*)scratch_ptr);
	arena_create(&m_arena_frame, Megabytes(4), (char*)frame_ptr);
	arena_create(&m_arena_level, Megabytes(4), (char*)level_ptr);
	////////////////////////////////
	Reset();
}

void Game::Init()
{
	m_renderer = (R_D3D11_State*)arena_push(m_arena_main, sizeof(R_D3D11_State));
	m_renderer->Init();
	//- nb: Resources
	m_spritesheetID = m_renderer->LoadTexture(L"sheet.png", m_arena_scratch);
	
	
	m_floodfill_queue = (u32*)arena_push(&m_arena_level, m_tiles_size);
}

void Game::Destroy()
{
	m_renderer->Destroy();
}

void Game::SetWindow(void *window_handle, u32 width, u32 height)
{
	m_renderer->SetWindow(window_handle, width, height);
}

void Game::OnMouseDown(MouseButton button, u32 x, u32 y)
{
	//- nb: Get tile index
	int tile_x = x / 32;
	int tile_y = y / 32;
	int idx = tile_y * m_columns + tile_x;
	
	if(tile_x >= m_columns || tile_y >= m_rows)
		return;
	
	Tile &tile = GetTile(idx);
	
	switch(button)
	{
		case LEFT_CLICK:
		break;
		
		case RIGHT_CLICK:
		// nb: Don't allow a flag to be placed on a swept mine
		if(tile.is_swept)
			break;
		
		// nb: Place flag
		if(!tile.has_flag)
		{
			tile.has_flag = true;
			tile.sprite = sprites[TILE_FLAG];
		}
		else
		{
			tile.has_flag = false;
			tile.sprite = sprites[TILE_DEFAULT];
		}
		break;
	}
	
}

void Game::OnMouseUp(MouseButton button, u32 x, u32 y)
{
	if(!m_playable)
	{
		Reset();
		return;
	}
	
	//- nb: Get tile index
	int tile_x = x / 32;
	int tile_y = y / 32;
	int idx = tile_y * m_columns + tile_x;
	
	if(tile_x >= m_columns || tile_y >= m_rows)
		return;
	
	switch(button)
	{
		case LEFT_CLICK:
		if(RevealTile(idx))
		{
			Gameover();
			//Reset();
		}
		break;
		////////////////////////////////
		case RIGHT_CLICK:
		Tile &tile = GridToTile(tile_x, tile_y);
		char buff[64];
		sprintf_s(buff, 64, "neighbors: %d", tile.neighbor_count);
		OutputDebugString(buff);
		break;
	}
}

void Game::OnSizeChanged(u32 width, u32 height)
{
	m_renderer->WindowSizeChanged(width, height);
}

void Game::Reset()
{
	arena_clear(&m_arena_scratch);
	arena_clear(&m_arena_level);
	m_floodfill_queue_count = 0;
	
	////////////////////////////////
	//- nb: Default values
	m_playable          = true;
	m_columns           = 30;
	m_rows              = 16;
	m_mine_count        = 90;
	m_flag_count        = 0;
	m_tiles = (Tile*)arena_push(&m_arena_level, sizeof(Tile) * m_rows * m_columns);
	m_tiles_size = m_columns * m_rows;
	
	// nb: Index array for shuffling, used for mine selection
	m_mine_indices = (u32*)arena_push(&m_arena_level, (sizeof(u32) * m_tiles_size));
	
	// nb: Populate board
	for(int i = 0; i < m_tiles_size; i++)
	{
		Tile tile;
		tile.sprite = sprites[TILE_DEFAULT];
		m_tiles[i] = tile;
		// nb: Populate index array
		m_mine_indices[i] = i;
	}
	
	////////////////////////////////
	//- nb: Pick mines at random
	srand(time(NULL));
	// shuffle index list
	for(int i = m_tiles_size - 1; i > 0; i--)
	{
		int j = rand() % (i + 1);
		
		int temp = m_mine_indices[i];
		m_mine_indices[i] = m_mine_indices[j];
		m_mine_indices[j] = temp;
	}
	// nb: Select n mines at random
	for(int i = 0; i < m_mine_count; i++)
	{
		int index = m_mine_indices[i];
		Tile &tile = m_tiles[index];
		tile.is_mine = true;
		//tile.sprite = sprites[TILE_MINE];
	}
	
	////////////////////////////////
	//- nb: Set the neighboring mine count for all tiles
	for(int i = 0; i < m_mine_count; i++)
	{
		u32 neighbor_idx_list[8] = {0};
		u32 neighbor_idx_list_count = 0;
		GetNeighbors(m_mine_indices[i], neighbor_idx_list, &neighbor_idx_list_count);
		for(int j = 0; j < neighbor_idx_list_count; j++)
		{
			if(!m_tiles[neighbor_idx_list[j]].is_mine)
				m_tiles[neighbor_idx_list[j]].neighbor_count++;
		}
	}
}


bool Game::RevealTile(u32 idx)
{
	Tile &tile = m_tiles[idx];
	
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
			m_floodfill_queue[m_floodfill_queue_count] = idx;
			m_floodfill_queue_count++;
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
		GetNeighbors(idx, neighbor_idx_list, &neighbor_idx_list_count);
		
		// nb: Count the neighboring flags
		for(int i = 0; i < neighbor_idx_list_count; i++)
		{
			if(m_tiles[neighbor_idx_list[i]].has_flag)
				flag_count++;
		}
		if(flag_count == tile.neighbor_count)
		{
			for(int i = 0; i < neighbor_idx_list_count; i++)
			{
				Tile &nb = m_tiles[neighbor_idx_list[i]];
				
				if(!nb.is_swept && !nb.has_flag)
				{
					if (RevealTile(neighbor_idx_list[i]))
						return true;
				}
			}
		}
		
	}
	
	////////////////////////////////
	//- nb: Flood fill
	while(m_floodfill_queue_count > 0)
	{
		// nb: Pop a tile
		u32 tile_idx = m_floodfill_queue[m_floodfill_queue_count - 1];
		m_floodfill_queue_count--;
		
		// nb: Sweep the current tile
		m_tiles[tile_idx].is_swept = true;
		m_tiles[tile_idx].sprite = sprites[TILE_EMPTY];
		
		u32 neighbor_idx_list[8] = {0};
		u32 neighbor_idx_list_count = 0;
		GetNeighbors(tile_idx, neighbor_idx_list, &neighbor_idx_list_count);
		// nb: Sweep every neighboring tile
		for(int i = 0; i < neighbor_idx_list_count; i++)
		{
			Tile &neighbor = m_tiles[neighbor_idx_list[i]];
			if(neighbor.is_mine || neighbor.is_swept)
				continue;
			
			neighbor.is_swept = true;
			
			// nb: Keep filling until there are no more tiles with 0 neighbors
			if(neighbor.neighbor_count == 0)
			{
				neighbor.sprite = sprites[TILE_EMPTY];
				m_floodfill_queue[m_floodfill_queue_count] = neighbor_idx_list[i];
				m_floodfill_queue_count++;
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

bool Game::RevealTile(u32 tile_x, u32 tile_y)
{
	u32 idx = tile_y * m_columns + tile_x;
	return RevealTile(idx);
}

void Game::Gameover()
{
	// nb: Reveal all mines
	for(int i = 0; i < m_mine_count; i++)
	{
		m_tiles[m_mine_indices[i]].sprite = sprites[TILE_MINE];
	}
	
	m_playable = false;
}


void Game::GetNeighbors(u32 idx, u32 neighbor_idx_list[8], u32 *neighbor_idx_list_count)
{
	u32 tile_x = idx % m_columns;
	u32 tile_y = idx / m_columns;
	GetNeighbors(tile_x, tile_y, neighbor_idx_list, neighbor_idx_list_count);
}
////////////////////////////////
// nb: This table shows the corresponding 1D array neighbor mappings
//       1D ARRAY                2D ARRAY
// [-W -1] [-W] [-W +1]  [-1, -1] [0, -1] [1, -1]
// [   -1] [ n] [   +1]  [-1,  0] [    n] [1,  0]
// [+W -1] [+W] [+W +1]  [-1,  1] [0,  1] [1,  1]
////////////////////////////////
void Game::GetNeighbors(u32 tile_x, u32 tile_y, u32 neighbor_idx_list[8], u32 *neighbor_idx_list_count)
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
			if (nx >= 0 && nx < m_columns && ny >= 0 && ny < m_rows) 
			{
				u32 neighbor_idx = ny * m_columns + nx;
				neighbor_idx_list[count] = neighbor_idx;
				count++;
			}
		}
	}
	*neighbor_idx_list_count = count;
}


Tile &Game::GridToTile(u32 tile_x, u32 tile_y)
{
	ASSERT(tile_x < m_columns && tile_y < m_rows);
	u32 idx = tile_y * m_columns + tile_x;
	return GetTile(idx);
}

Tile &Game::GetTile(u32 idx)
{
	ASSERT(idx < m_tiles_size);
	return m_tiles[idx];
}


void Game::Render()
{
	arena_clear(&m_arena_frame);
	
	const float color[4]{0.25f, 0.25f, 0.25f, 1.0f};
	m_renderer->Clear(color);
	
	
	//- nb: submit board batch to GPU
	InstanceData *instance_data = (InstanceData*)arena_push(&m_arena_frame, sizeof(InstanceData) * m_tiles_size);
	for (int i = 0; i < m_tiles_size; i++)
	{
		int x = i % m_columns;
		int y = i / m_columns;
		
		Tile &tile = m_tiles[i];
		instance_data[i] = { {(float)x,(float)y}, tile.sprite};
	}
	
	m_renderer->SubmitBatch(instance_data, m_tiles_size, m_spritesheetID);
	
	m_renderer->Present();
}