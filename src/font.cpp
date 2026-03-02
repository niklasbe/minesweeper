
#include "font.h"

// Implements two font rendering mechanism:
// 1. ASCII atlas with instanced rendering
// TODO(nb): 2. Cached UNICODE renderer


internal void
font_bake_ascii_atlas()
{
	// TODO(nb): don't hardcode these
#define ATLAS_SIZE 512
#define FONT_SIZE 48
	
	u8 *atlas_buffer = (u8*)arena_push(font_dwrite_state->arena, ATLAS_SIZE * ATLAS_SIZE * 4);
	
	const u8 text[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz !-_/\\':;,.+-=*%";
	const u32 count = sizeof(text) / sizeof(text[0]) - 1;
	u32 text_glyphs[count];
	for(int i = 0; i < count; i++)
	{
		text_glyphs[i] = text[i];
	}
	
	u16 glyph_idx[count];
	font_dwrite_state->font_face->GetGlyphIndices(text_glyphs, count, glyph_idx);
	
	DWRITE_GLYPH_METRICS dwrite_metrics[count];
	font_dwrite_state->font_face->GetDesignGlyphMetrics(glyph_idx, count, dwrite_metrics);
	
	DWRITE_FONT_METRICS font_metrics;
	font_dwrite_state->font_face->GetMetrics(&font_metrics);
	f32 scale = FONT_SIZE / (float)font_metrics.designUnitsPerEm;
	
	////////////////////////////////
	u32 cursor_x   = 0;
	u32 cursor_y   = 0;
	u32 row_h      = 0;
	u32 padding    = 2;
	// nb: we need the metrics for each glyph
	for(u32 i = 0; i < count; i += 1)
	{
		DWRITE_GLYPH_RUN glyph_run = {0};
		{
			glyph_run.fontFace     = font_dwrite_state->font_face;
			glyph_run.fontEmSize   = FONT_SIZE;
			glyph_run.glyphCount   = 1;
			glyph_run.glyphIndices = &glyph_idx[i];
		}
		IDWriteGlyphRunAnalysis *analysis;
		font_dwrite_state->factory->CreateGlyphRunAnalysis(&glyph_run,
																											 1.0f,
																											 NULL,
																											 DWRITE_RENDERING_MODE_ALIASED,
																											 DWRITE_MEASURING_MODE_NATURAL,
																											 0.0f,
																											 0.0f,
																											 &analysis);
		// nb: store metrics
		font_glyph_metrics[text[i]].left_bearing = (s32)(dwrite_metrics[i].leftSideBearing * scale);
		font_glyph_metrics[text[i]].top_bearing  = (s32)(dwrite_metrics[i].topSideBearing * scale);
		font_glyph_metrics[text[i]].advance      = (s32)(dwrite_metrics[i].advanceWidth * scale);
		////////////////////////////////
		
		RECT rect;
		analysis->GetAlphaTextureBounds(DWRITE_TEXTURE_ALIASED_1x1, &rect);
		u32 glyph_w = rect.right - rect.left;
		u32 glyph_h = rect.bottom - rect.top;
		// nb: we might not need to draw it if it's too small
		if(glyph_w > 0 && glyph_h > 0)
		{
			// nb: if we need to move to a new row
			if(cursor_x + glyph_w + padding > ATLAS_SIZE)
			{
				cursor_x = 0;
				cursor_y += row_h + padding;
				row_h = 0;
			}
			
			// nb: store more metrics
			font_glyph_metrics[text[i]].u0 = (f32)cursor_x / ATLAS_SIZE;
			font_glyph_metrics[text[i]].v0 = (f32)cursor_y / ATLAS_SIZE;
			font_glyph_metrics[text[i]].u1 = (f32)(cursor_x + glyph_w) / ATLAS_SIZE;
			font_glyph_metrics[text[i]].v1 = (f32)(cursor_y + glyph_h) / ATLAS_SIZE;
			font_glyph_metrics[text[i]].width        = glyph_w;
			font_glyph_metrics[text[i]].height       = glyph_h;
			
			
			////////////////////////////////
			// nb: create a temp buffer to store the texture, then blit it
			Temp temp = temp_begin(font_dwrite_state->arena);
			u32 glyph_size = glyph_w * glyph_h;
			u8 *temp_alpha = (u8*)arena_push(font_dwrite_state->arena, glyph_size);
			analysis->CreateAlphaTexture(DWRITE_TEXTURE_ALIASED_1x1,
																	 &rect,
																	 temp_alpha,
																	 glyph_size);
			
			// nb: bake the texture data
			for (int y = 0; y < glyph_h; y++) 
			{
				for (int x = 0; x < glyph_w; x++) 
				{
					u32 atlas_idx = ((cursor_y + y) * ATLAS_SIZE + (cursor_x + x)) * 4;
					u8 alpha = temp_alpha[y * glyph_w + x];
					
					atlas_buffer[atlas_idx + 0] = 255; // R
					atlas_buffer[atlas_idx + 1] = 255; // G
					atlas_buffer[atlas_idx + 2] = 255; // B
					atlas_buffer[atlas_idx + 3] = alpha; // A
				}
			}
			temp_end(temp);
			////////////////////////////////
			
			// nb: update the tallest glyph in the current row
			if(glyph_h > row_h)
			{
				row_h = glyph_h;
			}
			cursor_x += glyph_w + padding;
		}
		analysis->Release();
	}
	
	R_Handle handle = r_tex2d_alloc({ATLAS_SIZE, ATLAS_SIZE}, atlas_buffer);
	font_dwrite_state->ascii_atlas = handle;
}

// NOTE(nb): very cude, dont use this
void
draw_ascii_text(const char *str, f32 start_x, f32 start_y)
{
	u32 len = strlen(str);
	InstanceData *instance_data = (InstanceData*)arena_push(font_dwrite_state->arena, sizeof(InstanceData) * len);
	
	f32 cursor_x = start_x;
	for(int i = 0; str[i] != '\0'; i++)
	{
		Font_Glyph_Metrics *glyph = &font_glyph_metrics[str[i]];
		
		f32 x = cursor_x + (f32)glyph->left_bearing;
		f32 y = start_y + (f32)glyph->top_bearing;
		
		f32 uv_w = glyph->u1 - glyph->u0;
		f32 uv_h = glyph->v1 - glyph->v0;
		
		instance_data[i] = 
		{
			{x, y},
			{(f32)glyph->width, (f32)glyph->height}, 
			{glyph->u0, glyph->v0, uv_w, uv_h}
		};
		
		cursor_x += (f32)glyph->advance;
	}
	r_submit_batch(instance_data, len, font_dwrite_state->ascii_atlas);
}

void 
font_init()
{
	Arena *arena = arena_alloc();
	font_dwrite_state = (Font_DWrite_State*)arena_push(arena, sizeof(Font_DWrite_State));
	font_dwrite_state->arena = arena;
	HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
																	 __uuidof(IDWriteFactory3),
																	 (IUnknown**)(&font_dwrite_state->factory));
	if(FAILED(hr))
		__debugbreak();
	
	hr = font_dwrite_state->factory->CreateRenderingParams(&font_dwrite_state->base_rendering_params);
	font_dwrite_state->factory->GetGdiInterop(&font_dwrite_state->gdi_interop);
	font_dwrite_state->bitmap_render_target_dim = DirectX::XMINT2(2048, 256);
	font_dwrite_state->gdi_interop->CreateBitmapRenderTarget(0, 
																													 font_dwrite_state->bitmap_render_target_dim.x, 
																													 font_dwrite_state->bitmap_render_target_dim.y, 
																													 &font_dwrite_state->bitmap_render_target);
	font_dwrite_state->bitmap_render_target->SetPixelsPerDip(1.0);
	
	
	IDWriteFontCollection* systemCollection = NULL;
	font_dwrite_state->factory->GetSystemFontCollection(&systemCollection, FALSE);
	uint32_t index = 0;
	BOOL exists = FALSE;
	systemCollection->FindFamilyName(L"Segoe UI", &index, &exists);
	
	// nb: fallback
	if(!exists) 
	{
		index = 0; 
	}
	
	IDWriteFontFamily* family = NULL;
	systemCollection->GetFontFamily(index, &family);
	IDWriteFont* font = NULL;
	family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL, 
															 DWRITE_FONT_STRETCH_NORMAL, 
															 DWRITE_FONT_STYLE_NORMAL, 
															 &font);
	IDWriteFontFace* font_face = NULL;
	font->CreateFontFace(&font_face);
	font_dwrite_state->font_face = font_face;
	
	font->Release();
	family->Release();
	systemCollection->Release();
	
	font_bake_ascii_atlas();
}

void
font_destroy()
{
	font_dwrite_state->font_face->Release();
	font_dwrite_state->factory->Release();
	font_dwrite_state->base_rendering_params->Release();
	font_dwrite_state->gdi_interop->Release();
	font_dwrite_state->bitmap_render_target->Release();
	r_tex2d_release(font_dwrite_state->ascii_atlas);
	arena_release(font_dwrite_state->arena);
}
