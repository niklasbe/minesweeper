#ifndef RENDERER_H
#define RENDERER_H

#include <d3d11_3.h>
#include <wincodec.h>
#include <DirectXMath.h>


struct Board; 


////////////////////////////////
//~ nb: Per Instance Data
struct InstanceData
{
	DirectX::XMFLOAT2 ipos; // instance pos
    DirectX::XMFLOAT2 iuv;  // offset 
};


////////////////////////////////
//~ nb: D3D11 Renderer
class R_D3D11_State
{
	//-
	public:
	
	void Init();
	void Destroy();
	void CreateDeviceResources();
	void CreateWindowSizeDependentResources();
	void SetWindow(void *window_handle, UINT, UINT);
	void WindowSizeChanged(UINT, UINT);
	void HandleDeviceLost();
	
	void Clear(const float *color);
	void Present();
	
	void SubmitBatch(const InstanceData *, u32, u32);
	
	u32 LoadTexture(const wchar_t *filename, Arena &arena);
	
	void SetSpritesheet(ID3D11ShaderResourceView *texture);
	
	//-
	private:
	void CreateWICFactory();
	void CreateWICTextureFromFile(const wchar_t *filename, ID3D11ShaderResourceView **texture_view, Arena &arena);
	
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
	DirectX::XMMATRIX       m_projection;
	DirectX::XMMATRIX       m_translation;
	DirectX::XMMATRIX       m_scale;
	DirectX::XMMATRIX       m_world;
	DirectX::XMMATRIX       m_final_transform;
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
	ID3D11ShaderResourceView *m_textures[4];
	u32                       m_textures_count;
	
};

#endif //RENDERER_H
