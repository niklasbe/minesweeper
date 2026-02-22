#ifndef RENDERER_H
#define RENDERER_H

#include <d3d11_3.h>
#include <wincodec.h>
#include <DirectXMath.h>

////////////////////////////////
//~ nb: Per Instance Data
typedef struct InstanceData InstanceData;
struct InstanceData
{
	DirectX::XMFLOAT2 ipos; // instance pos
    DirectX::XMFLOAT2 iuv;  // offset 
    DirectX::XMFLOAT2 isize;// size
};

typedef union R_Handle R_Handle;
union R_Handle
{
	u64 U64[1];
	u32 U32[2];
	u16 U16[4];
};

////////////////////////////////
//~ nb: D3D11 Renderer
typedef struct R_D3D11_State R_D3D11_State;
struct R_D3D11_State
{
	//-
	// TODO(nb): reset on device lost
	ID3D11ShaderResourceView *m_spritesheet;
	Arena                    *m_arena;
	Arena                    *m_scratch_arena;
	
	////////////////////////////////
	//- nb: Main Window
	HWND                    hwnd;
    IDXGISwapChain1         *swapchain;
    ID3D11Texture2D         *framebuffer;
    ID3D11RenderTargetView  *framebuffer_rtv;
    u32                     width;
    u32                     height;
    D3D11_VIEWPORT          viewport;
	////////////////////////////////
	DirectX::XMMATRIX       projection;
	DirectX::XMMATRIX       translation;
	DirectX::XMMATRIX       scale;
	DirectX::XMMATRIX       world;
	DirectX::XMMATRIX       final_transform;
	////////////////////////////////
    //- nb: D3D11 context
	ID3D11Device            *base_device;
    ID3D11DeviceContext     *base_context;
    ID3D11Device3           *device;
    ID3D11DeviceContext3    *context;
	
    IDXGIDevice3            *dxgi_device;
    IDXGIAdapter            *dxgi_adapter;
    IDXGIFactory3           *dxgi_factory;
	////////////////////////////////
    ID3D11RasterizerState1  *main_rasterizer;
    ID3D11BlendState        *main_blend_state;
    ID3D11BlendState        *no_blend_state;
    ID3D11SamplerState      *point_sampler;
    ID3D11SamplerState      *linear_sampler;
    ID3D11DepthStencilState *noop_depth_stencil;
    ID3D11DepthStencilState *plain_depth_stencil;
	////////////////////////////////
	//- nb: Shaders
    ID3D11VertexShader      *vertex_shaders[1];
    ID3D11InputLayout       *input_layouts[1];
    ID3D11PixelShader       *pixel_shaders[1];
	ID3D11Buffer            *constant_buffers[1];
	ID3D11Buffer            *vertex_buffer;
	ID3D11Buffer            *index_buffer;
	ID3D11Buffer            *instance_buffer;
	////////////////////////////////
	IWICImagingFactory       *wic_factory;
	ID3D11ShaderResourceView *textures[4];
	u32                       textures_count;
	
};

void r_init();
void r_destroy();
void r_create_device_resources();
void r_create_window_size_dependent_resources();
void r_set_window(void *window_handle, u32 width, u32 height);
void r_window_size_changed(u32 width, u32 height);
void r_handle_device_lost();

void r_set_transform(f32 x, f32 y, f32 scale_x, f32 scale_y);

R_Handle r_load_texture(const wchar_t *filename, Arena *arena);

void r_submit_batch(const InstanceData *data, u32 data_len, u32 texture_id);
void r_clear(const float *color);
void r_present();

global R_D3D11_State *r_d3d11_state = {0};


#endif //RENDERER_H
