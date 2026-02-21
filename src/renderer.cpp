#include "renderer.h"
#include <d3dcompiler.h>
#include <strsafe.h>
#include <wrl/client.h> 

#include "game.h"


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
"};                                                         \n"
"                                                           \n"
"struct PS_INPUT                                            \n"
"{                                                          \n"
"    float4 pos   : SV_POSITION;                            \n" 
"    float2 uv    : TEXCOORD;                               \n"
"    float4 color : COLOR;                                  \n"
"};                                                         \n"
"                                                           \n"
"cbuffer PerFrame : register(b0)                            \n" 
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
"    float2 world_pos = input.pos.xy + input.ipos; \n"
"    output.pos = mul(projection, float4(world_pos, input.pos.z, 1.0f)); \n"
"    //output.pos = mul(float4(input.pos, 1.0f), uTransform); \n"
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

//- nb: Static uv offsets for every tile in the sheet
static const DirectX::XMFLOAT2 sprites[] =
{
	{0.0f, 0.0f},   {0.25f, 0.0f},   {0.50f, 0.0f},  {0.75f, 0.0f},
	{0.0f, 0.25f},  {0.25f, 0.25f},  {0.50f, 0.25f}, {0.75f, 0.25f},
	{0.0f, 0.50f},  {0.25f, 0.50f},  {0.50f, 0.50f}, {0.75f, 0.50f},
	{0.0f, 0.75f},  {0.25f, 0.75f},  {0.50f, 0.75f}, {0.75f, 0.75f},
};

void R_D3D11_State::Init()
{
	CreateDeviceResources();
	CreateWICFactory();
}

void R_D3D11_State::Destroy()
{
	
	if(context)
	{
		context->OMSetRenderTargets(0, NULL, NULL);
		context->ClearState();
		context->Flush();
	}
	
	for(int i = 0; i < m_textures_count; i++)
		SAFE_RELEASE(m_textures[i]);
	
	SAFE_RELEASE(framebuffer_rtv);
	SAFE_RELEASE(framebuffer);
	SAFE_RELEASE(swapchain);
	
	// nb: shader frees
	SAFE_RELEASE(constant_buffers[0]);
	SAFE_RELEASE(pixel_shaders[0]);
	SAFE_RELEASE(input_layouts[0]);
	SAFE_RELEASE(vertex_shaders[0]);
	
	SAFE_RELEASE(vertex_buffer);
	SAFE_RELEASE(index_buffer);
	SAFE_RELEASE(instance_buffer);
	
	// nb: depth/stencil states
	SAFE_RELEASE(plain_depth_stencil);
	SAFE_RELEASE(noop_depth_stencil);
	
	// nb: samplers
	SAFE_RELEASE(linear_sampler);
	SAFE_RELEASE(point_sampler);
	
	// nb: blend states
	SAFE_RELEASE(no_blend_state);
	SAFE_RELEASE(main_blend_state);
	
	SAFE_RELEASE(main_rasterizer);
	
	// nb: dxgi
	SAFE_RELEASE(dxgi_device);
	SAFE_RELEASE(dxgi_adapter);
	SAFE_RELEASE(dxgi_factory);
	
	// nb: d3d contexts
	SAFE_RELEASE(context);
	SAFE_RELEASE(device);
	SAFE_RELEASE(base_context);
	SAFE_RELEASE(base_device);
	
	SAFE_RELEASE(wic_factory);
}


void R_D3D11_State::CreateDeviceResources()
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
									&base_device, 0, &base_context);
#ifdef _DEBUG
	// nb: enable debug break-on-error
	ID3D11InfoQueue *info = 0;
	hr = base_device->QueryInterface(IID_ID3D11InfoQueue, (void **)(&info));
	if(SUCCEEDED(hr))
	{
		hr = info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		hr = info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
		info->Release();
	}
#endif
	
	
	// nb: get main device
	base_device->QueryInterface(IID_ID3D11Device1, (void**)(&device));
	base_context->QueryInterface(IID_ID3D11DeviceContext1, (void**)(&context));
	
	// nb: get dxgi device/adapter/factory
	device->QueryInterface(IID_IDXGIDevice1, (void**)(&dxgi_device));
	dxgi_device->GetAdapter(&dxgi_adapter);
	dxgi_adapter->GetParent(IID_IDXGIFactory2, (void**)(&dxgi_factory));
	
	ASSERT(device);
	ASSERT(context);
	ASSERT(dxgi_device);
	ASSERT(dxgi_adapter);
	ASSERT(dxgi_factory);
	
	// nb: create main rasterizer
	{
		D3D11_RASTERIZER_DESC1 desc = {0};
		{
			desc.FillMode = D3D11_FILL_SOLID;
			desc.CullMode = D3D11_CULL_BACK;
			desc.ScissorEnable = 1;
		}
		device->CreateRasterizerState1(&desc, &main_rasterizer);
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
		device->CreateBlendState(&desc, &main_blend_state);
	};
	
	// nb: create empty blend state
	{
		D3D11_BLEND_DESC desc = {0};
		{
			desc.RenderTarget[0].BlendEnable = 0;
		}
		device->CreateBlendState(&desc, &no_blend_state);
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
		device->CreateSamplerState(&desc, &point_sampler);
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
		device->CreateSamplerState(&desc, &linear_sampler);
	}
	
	// nb: create noop depth/stencil state
	{
		D3D11_DEPTH_STENCIL_DESC desc = {0};
		{
			desc.DepthEnable    = FALSE;
			desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			desc.DepthFunc      = D3D11_COMPARISON_LESS;
		}
		device->CreateDepthStencilState(&desc, &noop_depth_stencil);
	}
	
	// nb: create plain depth/stencil state
	{
		D3D11_DEPTH_STENCIL_DESC desc = {0};
		{
			desc.DepthEnable    = TRUE;
			desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			desc.DepthFunc      = D3D11_COMPARISON_LESS;
		}
		device->CreateDepthStencilState(&desc, &plain_depth_stencil);
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
				device->CreateVertexShader(vshad_source_blob->GetBufferPointer(),
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
				{ "IUV",  0,DXGI_FORMAT_R32G32_FLOAT,       1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1}
			};
			device->CreateInputLayout(desc,
									  ARRAYSIZE(desc),
									  vshad_source_blob->GetBufferPointer(),
									  vshad_source_blob->GetBufferSize(),
									  &ilay);
		}
		vshad_source_blob->Release();
		
		vertex_shaders[0] = vshad;
		input_layouts[0] = ilay;
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
				device->CreatePixelShader(pshad_source_blob->GetBufferPointer(),
										  pshad_source_blob->GetBufferSize(),
										  0,
										  &pshad);
			}
			
			pshad_source_blob->Release();
			pixel_shaders[0] = pshad;
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
			device->CreateBuffer(&desc, 0, &buffer);
		}
		constant_buffers[0] = buffer;
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
		device->CreateBuffer(&desc, &data, &vertex_buffer);
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
		device->CreateBuffer(&desc, &data, &index_buffer);
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
			device->CreateBuffer(&desc, 0, &buffer);
		}
		instance_buffer = buffer;
	}
	
}

void R_D3D11_State::CreateWindowSizeDependentResources()
{
	OutputDebugString("CreateWindowSizeDependentResources\n");
	ASSERT(hwnd);
	
	context->OMSetRenderTargets(0, NULL, NULL);
	SAFE_RELEASE(framebuffer_rtv);
	SAFE_RELEASE(framebuffer);
	
	context->Flush();
	// if swapchain is already created, resize it
	if(swapchain)
	{
		HRESULT hr = swapchain->ResizeBuffers(0, //preserve buffer count
											  width, height,
											  DXGI_FORMAT_UNKNOWN,
											  DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
		// if device gets lost somehow (driver crash?)
		if(hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
#ifdef _DEBUG
			char buff[64];
			u32 reason = (hr == DXGI_ERROR_DEVICE_REMOVED) ? device->GetDeviceRemovedReason() : hr;
            sprintf_s(buff, sizeof(buff), "Device Lost on ResizeBuffers: Reason code 0x%08X\n", reason);
            OutputDebugString(buff);
#endif
			// If the device was removed for any reason, a new device and swap chain will need to be created
			HandleDeviceLost();
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
			swapchain_desc.Width              = width; 
			swapchain_desc.Height             = height;
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
		HRESULT hr = dxgi_factory->CreateSwapChainForHwnd((IUnknown *)device,
														  hwnd,
														  &swapchain_desc,
														  0, // no fullscreen descriptor
														  0, 
														  &swapchain);
		ASSERT(SUCCEEDED(hr)); 
		
		// This program does not support exclusive fullscreen, has no fullscreen descriptor. Prevent DXGI from responding to ALT+ENTER keybind
		dxgi_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
	}
	
	// nb: get the frame buffer and rtv
	swapchain->GetBuffer(0, IID_ID3D11Texture2D, (void**)&framebuffer);
	device->CreateRenderTargetView(framebuffer, 0, &framebuffer_rtv);
	
	viewport = D3D11_VIEWPORT{0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f};
	
	// nb: update matrices
	m_projection = DirectX::XMMatrixOrthographicOffCenterLH(0.0f, (float)width, 
															(float)height, 0.0f, 
															0.0f, 1.0f);
	m_scale = DirectX::XMMatrixScaling(32.0f, 32.0f, 1.0f);
	m_translation = DirectX::XMMatrixTranslation(0.0f, 0.0f, 0.0f);
	m_world = m_scale * m_translation;
	m_final_transform = m_world * m_projection;
	
	
	
	// nb: render pipeline
	context->RSSetViewports(1, &viewport);
	D3D11_RECT rect = {0, 0, (LONG)width, (LONG)height};
	context->RSSetScissorRects(1, &rect);
	
	context->OMSetDepthStencilState(plain_depth_stencil, 1);
	context->RSSetState(main_rasterizer);
	
	context->IASetIndexBuffer(index_buffer, DXGI_FORMAT_R32_UINT, 0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	context->IASetInputLayout(input_layouts[0]);
	context->VSSetShader(vertex_shaders[0], NULL, 0);
	context->PSSetShader(pixel_shaders[0], NULL, 0);
	context->PSSetSamplers(0, 1, &point_sampler);
	context->OMSetRenderTargets(1, &framebuffer_rtv, NULL);
}

void R_D3D11_State::SetWindow(void *window_handle, UINT width, UINT height)
{
	this->hwnd = (HWND)window_handle;
	this->width = width;
	this->height = height;
}

void R_D3D11_State::WindowSizeChanged(UINT width, UINT height)
{
	// only resize if the size differs
	if(this->width != width || this->height != height)
	{
		this->width = width;
		this->height = height;
		CreateWindowSizeDependentResources();
	}
	
	/*
// update color space?
	else
	{
		
	}*/
}

void R_D3D11_State::HandleDeviceLost()
{
#ifdef _DEBUG
	ID3D11Debug *d3d_debug;
	device->QueryInterface(IID_ID3D11Debug, (void **)&d3d_debug);
	d3d_debug->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY);
	d3d_debug->Release();
#endif
	
	Destroy();
	CreateDeviceResources();
	CreateWindowSizeDependentResources();
}

void R_D3D11_State::Clear(const float *color)
{
	context->OMSetRenderTargets(1, &framebuffer_rtv, NULL);
	context->ClearRenderTargetView(framebuffer_rtv, color);
	
}

void R_D3D11_State::Present()
{
	
	//- nb: Constant Transform Buffer
    {
		TransformBuffer transform{m_final_transform};
		
		D3D11_MAPPED_SUBRESOURCE mapped;
		{
			context->Map(constant_buffers[0], 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
			CopyMemory(mapped.pData, &transform, sizeof(TransformBuffer));
			context->Unmap(constant_buffers[0], 0);
		}
		context->VSSetConstantBuffers(0, 1, &constant_buffers[0]);
	}
	
	HRESULT hr = swapchain->Present(1, 0);
	////////////////////////////////
	
	if(hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
#ifdef _DEBUG
		char buff[64];
		u32 reason = (hr == DXGI_ERROR_DEVICE_REMOVED) ? device->GetDeviceRemovedReason() : hr;
		sprintf_s(buff, sizeof(buff), "Device Lost on ResizeBuffers: Reason code 0x%08X\n", reason);
		OutputDebugString(buff);
#endif
		// If the device was removed for any reason, a new device and swap chain will need to be created
		HandleDeviceLost();
	}
}

void R_D3D11_State::SubmitBatch(const InstanceData *instance_data, u32 length, u32 textureID)
{
	//- nb: Fill instance buffer
	D3D11_MAPPED_SUBRESOURCE mapped;
	{
		context->Map(instance_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		CopyMemory(mapped.pData, instance_data, sizeof(InstanceData) * length);
		context->Unmap(instance_buffer, 0);
	}
	
	//- nb: Set buffers
	{
		UINT strides[2] = { sizeof(Vertex), sizeof(InstanceData) };
		UINT offsets[2] = { 0, 0 };
		ID3D11Buffer *buffers[2] = { vertex_buffer, instance_buffer };
		context->IASetVertexBuffers(0, 2, buffers, strides, offsets);
	}
	
	
	context->PSSetShaderResources(0, 1, &m_textures[textureID]);
	context->DrawIndexedInstanced(6,             // indices,
								  length,        // num
								  0,             // start index loc
								  0,             // base vertex loc
								  0);            // start instance loc
}

// TODO(nb): Decide on this
using Microsoft::WRL::ComPtr;

u32 R_D3D11_State::LoadTexture(const wchar_t *filename, Arena &arena)
{
	u32 id = m_textures_count++;
	CreateWICTextureFromFile(filename, &m_textures[id], arena);
	return id;
}

void R_D3D11_State::CreateWICTextureFromFile(const wchar_t *filename,
											 ID3D11ShaderResourceView **texture_view,
											 Arena &arena)
{
	// TODO(nb): error checking
	HRESULT hr = S_OK;
    // nb: create decoder
    ComPtr<IWICBitmapDecoder> decoder;
    hr = wic_factory->CreateDecoderFromFilename(filename, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
	if(FAILED(hr))
		return;
	
    // nb: get the first frame
    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
	
    // nb: convert to RGBA
    ComPtr<IWICFormatConverter> converter;
    hr = wic_factory->CreateFormatConverter(&converter);
	
    hr = converter->Initialize(frame.Get(),
							   GUID_WICPixelFormat32bppRGBA,
							   WICBitmapDitherTypeNone,
							   nullptr, 0.0f,
							   WICBitmapPaletteTypeCustom);
	
    // nb: copy pixels to buffer
    UINT width, height;
    hr = converter->GetSize(&width, &height);
	
    UINT stride = width * 4; // 4 bytes per pixel (RGBA)
    UINT buffer_size = stride * height;
	void *pixels = (void*)arena_push(&arena, buffer_size);
	
    hr = converter->CopyPixels(nullptr, stride, buffer_size, (BYTE*)pixels);
	
    // nb: create d3d11 texture
    D3D11_TEXTURE2D_DESC desc = {};
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
    
    D3D11_SUBRESOURCE_DATA data = {};
	{
		data.pSysMem = pixels;
		data.SysMemPitch = stride;
	}
    
    ComPtr<ID3D11Texture2D> texture;
    hr = device->CreateTexture2D(&desc, &data, texture.GetAddressOf());
	
    // nb: create the shader resource view
    hr = device->CreateShaderResourceView(texture.Get(), nullptr, texture_view);
}

void R_D3D11_State::CreateWICFactory()
{
	HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory2, 
								  NULL, 
								  CLSCTX_INPROC_SERVER,
								  IID_PPV_ARGS(&wic_factory));
}
