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

////////////////////////////////
//~ nb: Game Structure
class Game
{
	//-
	public:
	Game(Arena &arena);
	void Init();
	void Destroy();
	void SetWindow(void *window_handle, u32, u32);
	////////////////////////////////
	// nb: Events
	void OnMouseDown(MouseButton, u32, u32);
	void OnMouseUp(MouseButton, u32, u32);
	void OnSizeChanged(u32, u32);
	////////////////////////////////
	void Reset();
	void Render();
	
	//-
	private:
	// nb: Returns true if a mine is found, signaling the end of the game
	bool          RevealTile(u32, u32);
	bool          RevealTile(u32);
	void          Gameover();
	
	void          GetNeighbors(u32 tile_x, u32 tile_y, u32 neighbor_idx_list[8], u32 *neighor_idx_list_count);
	void          GetNeighbors(u32 idx, u32 neighbor_idx_list[8], u32 *neighor_idx_list_count);
	Tile          &GridToTile(u32 tile_x, u32 tile_y); 
	Tile          &GetTile(u32 idx);
	
	////////////////////////////////
	// nb: Arenas
	Arena         *m_arena_main;
	Arena         m_arena_scratch;
	Arena         m_arena_frame;
	Arena         m_arena_level;
	
	
	////////////////////////////////
	R_D3D11_State *m_renderer;
	u32           m_spritesheetID;
	u32           *m_floodfill_queue;
	u32           m_floodfill_queue_count = 0;
	u32           *m_mine_indices;
	
	////////////////////////////////
	// nb: Variables
	bool          m_playable;
	f64           m_elapsed_time;
	u32           m_mine_count;
	u32           m_flag_count;
	u32           m_columns;
	u32           m_rows;
	Tile          *m_tiles;
	u32           m_tiles_size;
};

#endif //GAME_H
