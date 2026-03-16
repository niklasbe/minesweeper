
// TODO(nb): need to cache this

#include "font.h"

#define FONT_ATLAS_SIZE 1024
#define FONT_SIZE 48 * 96.0f / 72.0f

internal void
font_bake_ascii_atlas()
{
  const u8 text[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz 1234567890!-_/\\':;,.+-=*%";
  const u32 count = sizeof(text) / sizeof(text[0]) - 1;
  
  u8 *atlas_buffer = (u8*)arena_push(font_dwrite_state->arena, FONT_ATLAS_SIZE * FONT_ATLAS_SIZE * 4);
  Temp temp = temp_begin(font_dwrite_state->frame_arena);
  u32 *codepoints = (u32*)arena_push(temp.arena, sizeof(u32) * count);
  for(u32 i = 0; i < count; i++)
  {
    codepoints[i] = text[i];
  }
  
  u16 *glyph_idx = (u16*)arena_push(temp.arena, sizeof(u16) * count);
  font_dwrite_state->font_face->GetGlyphIndices(codepoints, count, glyph_idx);
  
  DWRITE_GLYPH_METRICS *glyph_metrics =  (DWRITE_GLYPH_METRICS*)arena_push(temp.arena, sizeof(DWRITE_GLYPH_METRICS) * count);
  font_dwrite_state->font_face->GetDesignGlyphMetrics(glyph_idx, count, glyph_metrics);
  
  DWRITE_FONT_METRICS font_metrics = {0};
  font_dwrite_state->font_face->GetMetrics(&font_metrics);
  
  f32 scale = FONT_SIZE / (float)font_metrics.designUnitsPerEm;
  f32 ascent  = font_metrics.ascent * scale;
  f32 descent = font_metrics.descent * scale;
  
  HDC dc = font_dwrite_state->bitmap_render_target->GetMemoryDC();
  HBRUSH black_brush = CreateSolidBrush(RGB(0, 0, 0));
  DIBSECTION dib = {0};
  HBITMAP bitmap = (HBITMAP)GetCurrentObject(dc, OBJ_BITMAP);
  GetObject(bitmap, sizeof(dib), &dib);
  u8 *src_pixels = (u8*)dib.dsBm.bmBits;
  u32 src_pitch  = dib.dsBm.bmWidthBytes;
  
  // TODO(nb): store in atlas struct for multiple atlases
  u32 shelf_x = 0;
  u32 shelf_y = 0;
  u32 shelf_h = 0;
  u32 padding = 2;
  for(u32 i = 0; i < count; i++)
  {
    // if (is_cached(glyph_idx[i])) continue;
    
    f32 lsb     = (f32)glyph_metrics[i].leftSideBearing * scale;
    f32 tsb     = (f32)glyph_metrics[i].topSideBearing * scale;
    f32 bsb     = (f32)glyph_metrics[i].bottomSideBearing * scale;
    f32 advance = (f32)glyph_metrics[i].advanceWidth * scale;
    f32 glyph_w = (f32)(glyph_metrics[i].advanceWidth - 
                        glyph_metrics[i].leftSideBearing - 
                        glyph_metrics[i].rightSideBearing) * scale;
    f32 glyph_h = (f32)(font_metrics.ascent + font_metrics.descent) * scale - tsb - bsb;
    u32 padded_w  = (u32)ceilf(glyph_w)  + (padding * 2);
    u32 padded_h  = (u32)ceilf(glyph_h)  + (padding * 2);
    
    if(shelf_x + padded_w > FONT_ATLAS_SIZE)
    {
      shelf_x = 0;
      shelf_y += shelf_h;
      shelf_h = 0;
    }
    if(shelf_y + padded_h > FONT_ATLAS_SIZE)
    {
      // TODO(nb): atlas is full, create a new one
      //__debugbreak();
      break;
    }
    if(padded_h > shelf_h)
    {
      shelf_h = padded_h;
    }
    // nb: store metrics
    const f32 uv_size = 1.0f / FONT_ATLAS_SIZE;
    font_glyph_metrics[text[i]].left_bearing = roundf(lsb - padding);
    font_glyph_metrics[text[i]].top_bearing  = roundf(tsb - padding);
    font_glyph_metrics[text[i]].advance      = advance;
    font_glyph_metrics[text[i]].width        = padded_w;
    font_glyph_metrics[text[i]].height       = padded_h;
    font_glyph_metrics[text[i]].u0           = (f32)shelf_x * uv_size;
    font_glyph_metrics[text[i]].v0           = (f32)shelf_y * uv_size;
    font_glyph_metrics[text[i]].u1           = (f32)(shelf_x + padded_w) * uv_size;
    font_glyph_metrics[text[i]].v1           = (f32)(shelf_y + padded_h) * uv_size;
    
    ////////////////////////////////
    RECT fill_rect = {0, 0, (LONG)padded_w, (LONG)padded_h};
    FillRect(dc, &fill_rect, black_brush);
    DWRITE_GLYPH_RUN glyph_run = {0};
    {
      glyph_run.fontFace = font_dwrite_state->font_face;
      glyph_run.fontEmSize = FONT_SIZE;
      glyph_run.glyphCount = 1;
      glyph_run.glyphIndices = &glyph_idx[i];
    }
    f32 draw_x = padding - lsb;
    f32 draw_y = padding + ascent - tsb;
    RECT rect = {0};
    font_dwrite_state->bitmap_render_target->DrawGlyphRun(draw_x,
                                                          draw_y,
                                                          DWRITE_MEASURING_MODE_NATURAL,
                                                          &glyph_run,
                                                          font_dwrite_state->base_rendering_params,
                                                          0xFFFFFF,
                                                          &rect);
    for(u32 y = 0; y < padded_h; y++)
    {
      for(u32 x = 0; x < padded_w; x++)
      {
        u8 *src_pixel = src_pixels + (y * src_pitch) + (x * 4);
        u8 intensity = src_pixel[0];
        u32 atlas_idx = ((shelf_y + y) * FONT_ATLAS_SIZE + (shelf_x + x)) * 4;
        atlas_buffer[atlas_idx + 0] = 255;
        atlas_buffer[atlas_idx + 1] = 255;
        atlas_buffer[atlas_idx + 2] = 255;
        atlas_buffer[atlas_idx + 3] = intensity;
      }
    }
    shelf_x += padded_w;
  }
  
  DeleteObject(black_brush);
  temp_end(temp);
  
  // Update the GPU texture with the new buffer contents
  R_Handle handle = r_tex2d_alloc({FONT_ATLAS_SIZE, FONT_ATLAS_SIZE}, atlas_buffer);
  font_dwrite_state->ascii_atlas = handle;
}


// NOTE(nb): very cude, dont use this
void
draw_ascii_text(const char *str, f32 start_x, f32 start_y)
{
  u32 len = strlen(str);
  InstanceData *instance_data = (InstanceData*)arena_push(font_dwrite_state->frame_arena, sizeof(InstanceData) * len);
  
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

void font_frame()
{
  arena_clear(font_dwrite_state->frame_arena);
}

void 
font_init()
{
  Arena *arena = arena_alloc();
  font_dwrite_state = (Font_DWrite_State*)arena_push(arena, sizeof(Font_DWrite_State));
  font_dwrite_state->arena = arena;
  
  Arena *frame_arena = arena_alloc();
  font_dwrite_state->frame_arena = frame_arena;
  
  //- nb: Create factory
  HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                   __uuidof(IDWriteFactory3),
                                   (IUnknown**)(&font_dwrite_state->factory));
  if(FAILED(hr))
    __debugbreak();
  
  font_dwrite_state->factory->CreateRenderingParams(&font_dwrite_state->base_rendering_params);
  font_dwrite_state->factory->GetGdiInterop(&font_dwrite_state->gdi_interop);
  font_dwrite_state->bitmap_render_target_dim = DirectX::XMINT2(2048, 256);
  font_dwrite_state->gdi_interop->CreateBitmapRenderTarget(0, 
                                                           font_dwrite_state->bitmap_render_target_dim.x, 
                                                           font_dwrite_state->bitmap_render_target_dim.y, 
                                                           &font_dwrite_state->bitmap_render_target);
  font_dwrite_state->bitmap_render_target->SetPixelsPerDip(1.0);
  
  
  //- nb: Get font
  IDWriteFontCollection* system_collection = NULL;
  font_dwrite_state->factory->GetSystemFontCollection(&system_collection, FALSE);
  uint32_t index = 0;
  BOOL exists = FALSE;
  system_collection->FindFamilyName(L"Segoe UI", &index, &exists);
  
  // nb: Font fallback
  if(!exists) 
  {
    index = 0; 
  }
  
  IDWriteFontFamily* family = NULL;
  system_collection->GetFontFamily(index, &family);
  IDWriteFont* font = NULL;
  family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL, 
                               DWRITE_FONT_STRETCH_NORMAL, 
                               DWRITE_FONT_STYLE_NORMAL, 
                               &font);
  //- nb: Create font face
  IDWriteFontFace* font_face = NULL;
  font->CreateFontFace(&font_face);
  font_dwrite_state->font_face = font_face;
  ////////////////////////////////
  
  font->Release();
  family->Release();
  system_collection->Release();
  
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
  r_tex2d_release(font_dwrite_state->atlas);
  arena_release(font_dwrite_state->frame_arena);
  arena_release(font_dwrite_state->arena);
}
