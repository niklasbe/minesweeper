
#ifndef FONT_H
#define FONT_H

#pragma comment(lib, "gdi32")
#pragma comment(lib, "dwrite")

#include <dwrite_3.h>

typedef struct Font_Glyph_Metrics ont_Glyph_Metrics;
struct Font_Glyph_Metrics
{
	f32 u0, v0, u1, v1; // uv
	u32 width;
	u32 height;
	s32 left_bearing;
	s32 top_bearing;
	s32 offset_x;
	s32 offset_y;
	s32 advance;
};

typedef struct Font_DWrite_State Font_DWrite_State;
struct Font_DWrite_State
{
	Arena                     *arena;
  Arena                     *frame_arena;
	IDWriteFactory            *factory;
	IDWriteRenderingParams    *base_rendering_params;
	IDWriteGdiInterop         *gdi_interop;
	DirectX::XMINT2           bitmap_render_target_dim;
	IDWriteBitmapRenderTarget *bitmap_render_target;
	
	IDWriteFontFace           *font_face;
	R_Handle                  ascii_atlas;
	R_Handle                  atlas;
};

typedef struct Font_DWrite_Font Font_DWrite_Font;
struct Font_Font
{
	IDWriteFontFile  *file;
	IDWriteFontFace3 *face;
};

////////////////////////////////
//~ nb: Functions
void font_init();
void font_destroy();
void draw_ascii_text(const char *str, f32 x, f32 y);
void font_frame();

internal void font_bake_string_to_atlas(const char*);
internal void font_bake_ascii_atlas();
internal void font_bake_ascii_atlas_old();

////////////////////////////////
//~ nb: Globals
global Font_DWrite_State *font_dwrite_state = {0};
// ASCII lookup table
global Font_Glyph_Metrics font_glyph_metrics[128];

#endif //FONT_H
