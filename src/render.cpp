#include "render.h"
#include <d3dcompiler.h>
#include <strsafe.h>
#include <wrl/client.h> 

#include "game.h"


#pragma comment(lib, "d3d11")
#pragma comment(lib, "dxguid")
#pragma comment(lib, "d3dcompiler")


////////////////////////////////
//~ nb: Shader
const char hlsl[] =
"                                                           \n"
"struct VS_INPUT                                            \n"
"{                                                          \n"
"     float3 pos   : POS;                                   \n" 
"     float3 uv    : TEX;                                   \n"
"     float4 color : COL;                                   \n"
"                                                           \n"
"     float2 ipos  : IPOS;                                  \n"
"     float2 iuv   : IUV;                                   \n"
"     float2 isize : ISIZE;                                 \n"
"};                                                         \n"
"                                                           \n"
"struct PS_INPUT                                            \n"
"{                                                          \n"
"    float4 pos   : SV_POSITION;                            \n" 
"    float2 uv    : TEXCOORD;                               \n"
"    float4 color : COLOR;                                  \n"
"};                                                         \n"
"                                                           \n"
"cbuffer Camera : register(b0)                              \n" 
"{                                                          \n"
"    float4x4 projection;                                   \n"
"}                                                          \n"
"                                                           \n"
"sampler sampler0 : register(s0);                           \n" 
"                                                           \n"
"Texture2D<float4> texture0 : register(t0);                 \n" 
"                                                           \n"
"PS_INPUT vs(VS_INPUT input)                                \n"
"{                                                          \n"
"    PS_INPUT output;                                       \n"
"    float2 local_pos = input.pos.xy * input.isize;         \n"
"    float2 world_pos = local_pos + input.ipos;             \n"
"    output.pos = mul(projection, float4(world_pos, input.pos.z, 1.0f)); \n"
"    output.uv = (input.uv.xy * 0.25f) + input.iuv.xy;            \n"
"    output.color = input.color;                            \n"
"    return output;                                         \n"
"}                                                          \n"
"                                                           \n"
"float4 ps(PS_INPUT input) : SV_TARGET                      \n"
"{                                                          \n"
"    float4 tex = texture0.Sample(sampler0, input.uv);      \n"
"    return input.color * tex;                              \n"
"}                                                          \n";

////////////////////////////////
// nb: Helper macros
#define SAFE_RELEASE(COM) \
do{\
if(COM != NULL) {\
(COM)->Release();\
COM = NULL;\
}\
}while(0)

////////////////////////////////
//- nb: Per vertex buffer structure
typedef struct
{
	DirectX::XMMATRIX uTransform;
} TransformBuffer;

//- nb: Vertex structure
typedef struct
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 tex;
	DirectX::XMFLOAT4 color; 
} Vertex;

//- nb: A map of the spritesheet tiles
enum TileKind
{
	TILE_ONE,
	TILE_TWO,
	TILE_THREE,
	TILE_FOUR,
	TILE_FIVE,
	TILE_SIX,
	TILE_SEVEN,
	TILE_EIGHT,
	TILE_EMPTY,
	TILE_DEFAULT,
	TILE_FLAG,
	TILE_MINECROSS,
	TILE_QUESTIONMARK,
	TILE_DEFAULTQUESTIONMARK,
	TILE_MINE,
	TILE_MINERED,
	TILE_END
};


internal void r_create_wic_factory();
internal void r_create_wic_texture_from_file(const wchar_t *filename, ID3D11ShaderResourceView **texture_view, Arena *arena);

//- nb: Static uv offsets for every tile in the sheet
static const DirectX::XMFLOAT2 sprites[] =
{
	{0.0f, 0.0f},   {0.25f, 0.0f},   {0.50f, 0.0f},  {0.75f, 0.0f},
	{0.0f, 0.25f},  {0.25f, 0.25f},  {0.50f, 0.25f}, {0.75f, 0.25f},
	{0.0f, 0.50f},  {0.25f, 0.50f},  {0.50f, 0.50f}, {0.75f, 0.50f},
	{0.0f, 0.75f},  {0.25f, 0.75f},  {0.50f, 0.75f}, {0.75f, 0.75f},
};

void 
r_init(Arena *arena)
{
	r_d3d11_state = (R_D3D11_State*)arena_push(arena, sizeof(R_D3D11_State));
	r_create_device_resources();
	r_create_wic_factory();
}

void
r_destroy()
{
	if(r_d3d11_state->context)
	{
		r_d3d11_state->context->OMSetRenderTargets(0, NULL, NULL);
		r_d3d11_state->context->ClearState();
		r_d3d11_state->context->Flush();
	}
	
	for(int i = 0; i < r_d3d11_state->textures_count; i++)
		SAFE_RELEASE(r_d3d11_state->textures[i]);
	
	SAFE_RELEASE(r_d3d11_state->framebuffer_rtv);
	SAFE_RELEASE(r_d3d11_state->framebuffer);
	SAFE_RELEASE(r_d3d11_state->swapchain);
	
	// nb: shader frees
	SAFE_RELEASE(r_d3d11_state->constant_buffers[0]);
	SAFE_RELEASE(r_d3d11_state->pixel_shaders[0]);
	SAFE_RELEASE(r_d3d11_state->input_layouts[0]);
	SAFE_RELEASE(r_d3d11_state->vertex_shaders[0]);
	
	SAFE_RELEASE(r_d3d11_state->vertex_buffer);
	SAFE_RELEASE(r_d3d11_state->index_buffer);
	SAFE_RELEASE(r_d3d11_state->instance_buffer);
	
	// nb: depth/stencil states
	SAFE_RELEASE(r_d3d11_state->plain_depth_stencil);
	SAFE_RELEASE(r_d3d11_state->noop_depth_stencil);
	
	// nb: samplers
	SAFE_RELEASE(r_d3d11_state->linear_sampler);
	SAFE_RELEASE(r_d3d11_state->point_sampler);
	
	// nb: blend states
	SAFE_RELEASE(r_d3d11_state->no_blend_state);
	SAFE_RELEASE(r_d3d11_state->main_blend_state);
	
	SAFE_RELEASE(r_d3d11_state->main_rasterizer);
	
	// nb: dxgi
	SAFE_RELEASE(r_d3d11_state->dxgi_device);
	SAFE_RELEASE(r_d3d11_state->dxgi_adapter);
	SAFE_RELEASE(r_d3d11_state->dxgi_factory);
	
	// nb: d3d contexts
	SAFE_RELEASE(r_d3d11_state->context);
	SAFE_RELEASE(r_d3d11_state->device);
	SAFE_RELEASE(r_d3d11_state->base_context);
	SAFE_RELEASE(r_d3d11_state->base_device);
	
	SAFE_RELEASE(r_d3d11_state->wic_factory);
}


void 
r_create_device_resources()
{
	OutputDebugString("CreateDeviceResources\n");
	
	u32 creation_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
	creation_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	
	// nb:  Device creation
	D3D_FEATURE_LEVEL feature_levels[] = {D3D_FEATURE_LEVEL_11_0};
	D3D_DRIVER_TYPE driver_type = D3D_DRIVER_TYPE_HARDWARE;
	HRESULT hr  = D3D11CreateDevice(0,
									driver_type,
									0,
									creation_flags,
									feature_levels, ARRAYSIZE(feature_levels),
									D3D11_SDK_VERSION,
									&r_d3d11_state->base_device, 0, &r_d3d11_state->base_context);
#ifdef _DEBUG
	// nb: enable debug break-on-error
	ID3D11InfoQueue *info = 0;
	hr = r_d3d11_state->base_device->QueryInterface(IID_ID3D11InfoQueue, (void **)(&info));
	if(SUCCEEDED(hr))
	{
		hr = info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		hr = info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
		info->Release();
	}
#endif
	
	
	// nb: get main device
	r_d3d11_state->base_device->QueryInterface(IID_ID3D11Device1, (void**)(&r_d3d11_state->device));
	r_d3d11_state->base_context->QueryInterface(IID_ID3D11DeviceContext1, (void**)(&r_d3d11_state->context));
	
	// nb: get dxgi device/adapter/factory
	r_d3d11_state->device->QueryInterface(IID_IDXGIDevice1, (void**)(&r_d3d11_state->dxgi_device));
	r_d3d11_state->dxgi_device->GetAdapter(&r_d3d11_state->dxgi_adapter);
	r_d3d11_state->dxgi_adapter->GetParent(IID_IDXGIFactory2, (void**)(&r_d3d11_state->dxgi_factory));
	
	ASSERT(r_d3d11_state->device);
	ASSERT(r_d3d11_state->context);
	ASSERT(r_d3d11_state->dxgi_device);
	ASSERT(r_d3d11_state->dxgi_adapter);
	ASSERT(r_d3d11_state->dxgi_factory);
	
	// nb: create main rasterizer
	{
		D3D11_RASTERIZER_DESC1 desc = {0};
		{
			desc.FillMode = D3D11_FILL_SOLID;
			desc.CullMode = D3D11_CULL_BACK;
			desc.ScissorEnable = 1;
		}
		r_d3d11_state->device->CreateRasterizerState1(&desc, &r_d3d11_state->main_rasterizer);
	}
	
	// nb: create main blend state
	{
		D3D11_BLEND_DESC desc = {0};
		{
			desc.RenderTarget[0].BlendEnable            = 1;
			desc.RenderTarget[0].SrcBlend               = D3D11_BLEND_SRC_ALPHA;
			desc.RenderTarget[0].DestBlend              = D3D11_BLEND_INV_SRC_ALPHA; 
			desc.RenderTarget[0].BlendOp                = D3D11_BLEND_OP_ADD;
			desc.RenderTarget[0].SrcBlendAlpha          = D3D11_BLEND_ONE;
			desc.RenderTarget[0].DestBlendAlpha         = D3D11_BLEND_ZERO;
			desc.RenderTarget[0].BlendOpAlpha           = D3D11_BLEND_OP_ADD;
			desc.RenderTarget[0].RenderTargetWriteMask  = D3D11_COLOR_WRITE_ENABLE_ALL;
		}
		r_d3d11_state->device->CreateBlendState(&desc, &r_d3d11_state->main_blend_state);
	};
	
	// nb: create empty blend state
	{
		D3D11_BLEND_DESC desc = {0};
		{
			desc.RenderTarget[0].BlendEnable = 0;
		}
		r_d3d11_state->device->CreateBlendState(&desc, &r_d3d11_state->no_blend_state);
	}
	
	// nb: create nearest-neighbor sampler
	{
		D3D11_SAMPLER_DESC desc = {0};
		{
			desc.Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT;
			desc.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		}
		r_d3d11_state->device->CreateSamplerState(&desc, &r_d3d11_state->point_sampler);
	}
	
	// nb: create bilinear sampler
	{
		D3D11_SAMPLER_DESC desc = {0};
		{
			desc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			desc.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		}
		r_d3d11_state->device->CreateSamplerState(&desc, &r_d3d11_state->linear_sampler);
	}
	
	// nb: create noop depth/stencil state
	{
		D3D11_DEPTH_STENCIL_DESC desc = {0};
		{
			desc.DepthEnable    = FALSE;
			desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			desc.DepthFunc      = D3D11_COMPARISON_LESS;
		}
		r_d3d11_state->device->CreateDepthStencilState(&desc, &r_d3d11_state->noop_depth_stencil);
	}
	
	// nb: create plain depth/stencil state
	{
		D3D11_DEPTH_STENCIL_DESC desc = {0};
		{
			desc.DepthEnable    = TRUE;
			desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			desc.DepthFunc      = D3D11_COMPARISON_LESS;
		}
		r_d3d11_state->device->CreateDepthStencilState(&desc, &r_d3d11_state->plain_depth_stencil);
	}
	
	// nb: build vertex shaders and input layouts
	{
		// nb: vertex shader
		ID3DBlob *vshad_source_blob = 0;
		ID3DBlob *vshad_source_errors = 0;
		ID3D11VertexShader *vshad = 0;
		{
			hr = D3DCompile(hlsl, 
							sizeof(hlsl),
							0,
							0,
							0,
							"vs",
							"vs_5_0",
							0,
							0,
							&vshad_source_blob,
							&vshad_source_errors);
			if(FAILED(hr))
			{
				// error printing
				const char* error_msg = (const char*)vshad_source_errors->GetBufferPointer();
				char buffer[256];
				StringCchPrintfA(buffer, sizeof(buffer), "Vertex shader compilation failed: %s\n", error_msg);
				MessageBoxA(0, "Vertex shader compilation failture", error_msg, MB_OK);
			}
			else
			{
				r_d3d11_state->device->CreateVertexShader(vshad_source_blob->GetBufferPointer(),
														  vshad_source_blob->GetBufferSize(),
														  0,
														  &vshad);
			}
		}
		
		// nb: input layout
		ID3D11InputLayout *ilay = 0;
		{
			D3D11_INPUT_ELEMENT_DESC desc[] =
			{
				{ "POS", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,                            0, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{ "TEX", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{ "COL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
				
				{ "IPOS", 0, DXGI_FORMAT_R32G32_FLOAT,      1,                            0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
				{ "IUV",  0, DXGI_FORMAT_R32G32_FLOAT,      1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
				{ "ISIZE",0, DXGI_FORMAT_R32G32_FLOAT,      1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1}
			};
			r_d3d11_state->device->CreateInputLayout(desc,
													 ARRAYSIZE(desc),
													 vshad_source_blob->GetBufferPointer(),
													 vshad_source_blob->GetBufferSize(),
													 &ilay);
		}
		vshad_source_blob->Release();
		
		r_d3d11_state->vertex_shaders[0] = vshad;
		r_d3d11_state->input_layouts[0] = ilay;
	}
	
	// nb: build pixel shaders
	{
		ID3DBlob *pshad_source_blob = 0;
		ID3DBlob *pshad_source_errors = 0;
		ID3D11PixelShader *pshad = 0;
		{
			hr = D3DCompile(hlsl, 
							sizeof(hlsl),
							0,
							0,
							0,
							"ps",
							"ps_5_0",
							0,
							0,
							&pshad_source_blob,
							&pshad_source_errors);
			if(FAILED(hr))
			{
				// error printing
				const char* error_msg = (const char*)pshad_source_errors->GetBufferPointer();
				char buffer[256];
				StringCchPrintfA(buffer, sizeof(buffer), "Vertex shader compilation failed: %s\n", error_msg);
				MessageBoxA(0, "Pixel shader compilation failture", error_msg, MB_OK);
			}
			else
			{
				r_d3d11_state->device->CreatePixelShader(pshad_source_blob->GetBufferPointer(),
														 pshad_source_blob->GetBufferSize(),
														 0,
														 &pshad);
			}
			
			pshad_source_blob->Release();
			r_d3d11_state->pixel_shaders[0] = pshad;
		}
	}
	
	// nb: build constant buffers
	{
		
		ID3D11Buffer *buffer = 0;
		{
			D3D11_BUFFER_DESC desc = {0};
			{
				desc.ByteWidth      = sizeof(TransformBuffer);
				desc.ByteWidth     += 15;
				desc.ByteWidth     -= desc.ByteWidth % 16;
				desc.Usage          = D3D11_USAGE_DYNAMIC;
				desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
				desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			}
			r_d3d11_state->device->CreateBuffer(&desc, 0, &buffer);
		}
		r_d3d11_state->constant_buffers[0] = buffer;
	}
	
	// nb: build vertex buffers
	{
		
		Vertex vertices[] =
		{
			//  Position (XYZ)          UV (UVW)              Color (RGBA)
			{ { 0.0f, 0.0f, 0.0f},  {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f} }, // Top-Left
			{ { 1.0f, 0.0f, 0.0f},  {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f} }, // Top-Right
			{ { 1.0f, 1.0f, 0.0f},  {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f} }, // Bottom-Right
			{ { 0.0f, 1.0f, 0.0f},  {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f} }, // Bottom-Left
		};
		
		
		D3D11_BUFFER_DESC desc = {0};
		{
			desc.ByteWidth = sizeof(vertices);
			desc.Usage = D3D11_USAGE_IMMUTABLE;
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		}
		
		D3D11_SUBRESOURCE_DATA data = {vertices};
		r_d3d11_state->device->CreateBuffer(&desc, &data, &r_d3d11_state->vertex_buffer);
	}
	
	// nb: build index buffer
	{
		uint32_t indices[] =
		{
			0, 1, 2,
			2, 3, 0
		};
		
		D3D11_BUFFER_DESC desc = {0};
		{
			desc.ByteWidth = sizeof(indices);
			desc.Usage = D3D11_USAGE_IMMUTABLE;
			desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		}
		
		D3D11_SUBRESOURCE_DATA data= {indices};
		r_d3d11_state->device->CreateBuffer(&desc, &data, &r_d3d11_state->index_buffer);
	}
	// nb: build instance buffer
	{
		ID3D11Buffer *buffer;
		{
			D3D11_BUFFER_DESC desc = {0};
			{
				desc.ByteWidth      = sizeof(InstanceData) * 1024 * 64; // 65536 tiles?
				desc.Usage          = D3D11_USAGE_DYNAMIC;
				desc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
				desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			}
			r_d3d11_state->device->CreateBuffer(&desc, 0, &buffer);
		}
		r_d3d11_state->instance_buffer = buffer;
	}
	
}

void
r_create_window_size_dependent_resources()
{
	OutputDebugString("CreateWindowSizeDependentResources\n");
	ASSERT(r_d3d11_state->hwnd);
	
	r_d3d11_state->context->OMSetRenderTargets(0, NULL, NULL);
	SAFE_RELEASE(r_d3d11_state->framebuffer_rtv);
	SAFE_RELEASE(r_d3d11_state->framebuffer);
	
	r_d3d11_state->context->Flush();
	// if swapchain is already created, resize it
	if(r_d3d11_state->swapchain)
	{
		HRESULT hr = r_d3d11_state->swapchain->ResizeBuffers(0, //preserve buffer count
															 r_d3d11_state->width, 
															 r_d3d11_state->height,
															 DXGI_FORMAT_UNKNOWN,
															 DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
		// if device gets lost somehow (driver crash?)
		if(hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
#ifdef _DEBUG
			char buff[64];
			u32 reason = (hr == DXGI_ERROR_DEVICE_REMOVED) ? r_d3d11_state->device->GetDeviceRemovedReason() : hr;
            sprintf_s(buff, sizeof(buff), "Device Lost on ResizeBuffers: Reason code 0x%08X\n", reason);
            OutputDebugString(buff);
#endif
			// If the device was removed for any reason, a new device and swap chain will need to be created
			r_handle_device_lost();
			// Exit for now. HandleDeviceLost() will re-enter this function and properly set up the new device
			return;
		}
		ASSERT(SUCCEEDED(hr));
	}
	// nb: swapchain creation
	// if no swapchain exists, create it
	else
	{
		DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {0};
		{
			swapchain_desc.Width              = r_d3d11_state->width; 
			swapchain_desc.Height             = r_d3d11_state->height;
			//swapchain_desc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM; 
			swapchain_desc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
			swapchain_desc.Stereo             = FALSE;
			swapchain_desc.SampleDesc.Count   = 1;
			swapchain_desc.SampleDesc.Quality = 0;
			swapchain_desc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapchain_desc.BufferCount        = 2; // Minimum 2 for flip model
			swapchain_desc.Scaling            = DXGI_SCALING_STRETCH;
			swapchain_desc.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			swapchain_desc.AlphaMode          = DXGI_ALPHA_MODE_UNSPECIFIED;
			swapchain_desc.Flags              = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
		}
		HRESULT hr = r_d3d11_state->dxgi_factory->CreateSwapChainForHwnd((IUnknown *)r_d3d11_state->device,
																		 r_d3d11_state->hwnd,
																		 &swapchain_desc,
																		 0, // no fullscreen descriptor
																		 0, 
																		 &r_d3d11_state->swapchain);
		ASSERT(SUCCEEDED(hr)); 
		
		// This program does not support exclusive fullscreen, has no fullscreen descriptor. Prevent DXGI from responding to ALT+ENTER keybind
		r_d3d11_state->dxgi_factory->MakeWindowAssociation(r_d3d11_state->hwnd, DXGI_MWA_NO_ALT_ENTER);
	}
	
	// nb: get the frame buffer and rtv
	r_d3d11_state->swapchain->GetBuffer(0, IID_ID3D11Texture2D, (void**)&r_d3d11_state->framebuffer);
	r_d3d11_state->device->CreateRenderTargetView(r_d3d11_state->framebuffer, 0, &r_d3d11_state->framebuffer_rtv);
	
	r_d3d11_state->viewport = D3D11_VIEWPORT{0.0f, 0.0f, (float)r_d3d11_state->width, (float)r_d3d11_state->height, 0.0f, 1.0f};
	
	
	////////////////////////////////
	//- nb: Matrix updates
	r_d3d11_state->projection = DirectX::XMMatrixOrthographicOffCenterLH(0.0f,
																		 (float)r_d3d11_state->width,
																		 (float)r_d3d11_state->height,
																		 0.0f, 
																		 0.0f, 
																		 1.0f);
	
	/*r_d3d11_state->scale = DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f);
	r_d3d11_state->translation = DirectX::XMMatrixTranslation(0.0f, 0.0f, 0.0f);
	r_d3d11_state->world = r_d3d11_state->scale * r_d3d11_state->translation;
	r_d3d11_state->final_transform = r_d3d11_state->world * r_d3d11_state->projection;*/
	r_set_transform(0, 0, 1, 1);
	////////////////////////////////
	
	
	// nb: render pipeline
	r_d3d11_state->context->RSSetViewports(1, &r_d3d11_state->viewport);
	D3D11_RECT rect = {0, 0, (LONG)r_d3d11_state->width, (LONG)r_d3d11_state->height};
	r_d3d11_state->context->RSSetScissorRects(1, &rect);
	
	r_d3d11_state->context->OMSetDepthStencilState(r_d3d11_state->plain_depth_stencil, 1);
	r_d3d11_state->context->RSSetState(r_d3d11_state->main_rasterizer);
	
	r_d3d11_state->context->IASetIndexBuffer(r_d3d11_state->index_buffer, DXGI_FORMAT_R32_UINT, 0);
	r_d3d11_state->context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	r_d3d11_state->context->IASetInputLayout(r_d3d11_state->input_layouts[0]);
	r_d3d11_state->context->VSSetShader(r_d3d11_state->vertex_shaders[0], NULL, 0);
	r_d3d11_state->context->PSSetShader(r_d3d11_state->pixel_shaders[0], NULL, 0);
	r_d3d11_state->context->PSSetSamplers(0, 1, &r_d3d11_state->point_sampler);
	r_d3d11_state->context->OMSetRenderTargets(1, &r_d3d11_state->framebuffer_rtv, NULL);
	
	
} 

void
r_set_window(void *window_handle, u32 width, u32 height)
{
	r_d3d11_state->hwnd = (HWND)window_handle;
	r_d3d11_state->width = width;
	r_d3d11_state->height = height;
}

void
r_window_size_changed(u32 width, u32 height)
{
	// only resize if the size differs
	if(r_d3d11_state->width != width || r_d3d11_state->height != height)
	{
		r_d3d11_state->width = width;
		r_d3d11_state->height = height;
		r_create_window_size_dependent_resources();
	}
	
	/*
// update color space?
	else
	{
		
	}*/
}

void
r_handle_device_lost()
{
#ifdef _DEBUG
	ID3D11Debug *d3d_debug;
	r_d3d11_state->device->QueryInterface(IID_ID3D11Debug, (void **)&d3d_debug);
	d3d_debug->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY);
	d3d_debug->Release();
#endif
	
	r_destroy();
	r_create_device_resources();
	r_create_window_size_dependent_resources();
}

void
r_clear(const float *color)
{
	r_d3d11_state->context->OMSetRenderTargets(1, &r_d3d11_state->framebuffer_rtv, NULL);
	r_d3d11_state->context->ClearRenderTargetView(r_d3d11_state->framebuffer_rtv, color);
}

void 
r_present()
{
	HRESULT hr = r_d3d11_state->swapchain->Present(1, 0);
	////////////////////////////////
	
	if(hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
#ifdef _DEBUG
		char buff[64];
		u32 reason = (hr == DXGI_ERROR_DEVICE_REMOVED) ? r_d3d11_state->device->GetDeviceRemovedReason() : hr;
		sprintf_s(buff, sizeof(buff), "Device Lost on ResizeBuffers: Reason code 0x%08X\n", reason);
		OutputDebugString(buff);
#endif
		// If the device was removed for any reason, a new device and swap chain will need to be created
		r_handle_device_lost();
	}
}

void 
r_set_transform(f32 x, f32 y, f32 scale_x, f32 scale_y)
{
	r_d3d11_state->translation = DirectX::XMMatrixTranslation(x, y, 0.0f);
	r_d3d11_state->scale       = DirectX::XMMatrixScaling(scale_x, scale_y, 1.0f);
	r_d3d11_state->world = r_d3d11_state->scale * r_d3d11_state->translation;
	r_d3d11_state->final_transform = r_d3d11_state->world * r_d3d11_state->projection;
	
	//- nb: Constant Transform Buffer
    {
		TransformBuffer transform{r_d3d11_state->final_transform};
		
		D3D11_MAPPED_SUBRESOURCE mapped;
		{
			r_d3d11_state->context->Map(r_d3d11_state->constant_buffers[0], 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
			CopyMemory(mapped.pData, &transform, sizeof(TransformBuffer));
			r_d3d11_state->context->Unmap(r_d3d11_state->constant_buffers[0], 0);
		}
		r_d3d11_state->context->VSSetConstantBuffers(0, 1, &r_d3d11_state->constant_buffers[0]);
	}
	
}

void
r_submit_batch(const InstanceData *instance_data, u32 length, u32 texture_id)
{
	//- nb: Fill instance buffer
	D3D11_MAPPED_SUBRESOURCE mapped;
	{
		r_d3d11_state->context->Map(r_d3d11_state->instance_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		CopyMemory(mapped.pData, instance_data, sizeof(InstanceData) * length);
		r_d3d11_state->context->Unmap(r_d3d11_state->instance_buffer, 0);
	}
	
	//- nb: Set buffers
	{
		UINT strides[2] = { sizeof(Vertex), sizeof(InstanceData) };
		UINT offsets[2] = { 0, 0 };
		ID3D11Buffer *buffers[2] = { r_d3d11_state->vertex_buffer, r_d3d11_state->instance_buffer };
		r_d3d11_state->context->IASetVertexBuffers(0, 2, buffers, strides, offsets);
	}
	
	
	r_d3d11_state->context->PSSetShaderResources(0, 1, &r_d3d11_state->textures[texture_id]);
	r_d3d11_state->context->DrawIndexedInstanced(6,             // indices,
												 length,        // num
												 0,             // start index loc
												 0,             // base vertex loc
												 0);            // start instance loc
}

R_Handle
r_load_texture(const wchar_t *filename, Arena *arena)
{
	R_Handle result = {0};
	u32 id = r_d3d11_state->textures_count++;
	r_create_wic_texture_from_file(filename, &r_d3d11_state->textures[id], arena);
	result.U32[0] = id;
	return result;
}


internal void
r_create_wic_texture_from_file(const wchar_t *filename, 
							   ID3D11ShaderResourceView **texture_view,
							   Arena *arena)
{
	// TODO(nb): error checking
	HRESULT hr = S_OK;
    // nb: create decoder
    IWICBitmapDecoder *decoder;
    hr = r_d3d11_state->wic_factory->CreateDecoderFromFilename(filename, 
															   nullptr, 
															   GENERIC_READ, 
															   WICDecodeMetadataCacheOnDemand, 
															   &decoder);
	if(FAILED(hr))
		return;
	
    // nb: get the first frame
	IWICBitmapFrameDecode *frame;
    hr = decoder->GetFrame(0, &frame);
	
    // nb: convert to RGBA
	IWICFormatConverter *converter;
    hr = r_d3d11_state->wic_factory->CreateFormatConverter(&converter);
	
    hr = converter->Initialize(frame,
							   GUID_WICPixelFormat32bppRGBA,
							   WICBitmapDitherTypeNone,
							   nullptr, 0.0f,
							   WICBitmapPaletteTypeCustom);
	
    // nb: copy pixels to buffer
    UINT width, height;
    hr = converter->GetSize(&width, &height);
	
    UINT stride = width * 4; // 4 bytes per pixel (RGBA)
    UINT buffer_size = stride * height;
	void *pixels = (void*)arena_push(arena, buffer_size);
	
    hr = converter->CopyPixels(nullptr, stride, buffer_size, (BYTE*)pixels);
	
    // nb: create d3d11 texture
    D3D11_TEXTURE2D_DESC desc = {0};
	{
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	}
    
    D3D11_SUBRESOURCE_DATA data = {0};
	{
		data.pSysMem = pixels;
		data.SysMemPitch = stride;
	}
    
    ID3D11Texture2D *texture;
    hr = r_d3d11_state->device->CreateTexture2D(&desc, &data, &texture);
	
    // nb: create the shader resource view
    hr = r_d3d11_state->device->CreateShaderResourceView(texture, nullptr, texture_view);
}

internal void
r_create_wic_factory()
{
	HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory2, 
								  NULL, 
								  CLSCTX_INPROC_SERVER,
								  IID_PPV_ARGS(&r_d3d11_state->wic_factory));
}
