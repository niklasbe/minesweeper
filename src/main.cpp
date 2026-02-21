#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#pragma comment(lib, "d3d11")
#pragma comment(lib, "user32")
#pragma comment(lib, "dxguid")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "ole32")

#include <intrin.h>
#define ASSERT(cond) do{ if(!(cond)) __debugbreak(); } while(0)

#define _DEBUG

#include <stdint.h>
typedef int8_t      s8;
typedef uint8_t     u8;
typedef int16_t     s16;
typedef uint16_t    u16;
typedef int32_t     s32;
typedef uint32_t    u32;
typedef int64_t     s64;
typedef uint64_t    u64;
typedef float       f32;
typedef double      f64; 

#define internal static

#define Kilobytes(x) x*1024
#define Megabytes(x) x*1024*1024

////////////////////////////////
//- nb: Arena
struct Arena 
{
	char *base_ptr;
	u32  size;
	u32  offset;
};
void arena_create(Arena *arena, u32 size, char *base_ptr)
{
	ASSERT(arena);
	arena->base_ptr = base_ptr;
	arena->size = size;
	arena->offset = 0;
}
void *arena_push(Arena *arena, u32 size)
{
	ASSERT((arena->offset + size) <= arena->size);
	void *result = arena->base_ptr + arena->offset;
	arena->offset += size;
	return result;
}
void arena_clear(Arena *arena)
{
	arena->offset = 0;
}
////////////////////////////////

#include "renderer.cpp"
#include "game.cpp"

Game     *g_game;

// NOTE(nb): message callback
LRESULT CALLBACK WndProc(HWND hwnd,
						 UINT message,
						 WPARAM wParam,
						 LPARAM lParam)
{
	static u8 in_sizemove = 0;
	static u8 minimized = 0;
	static u8 in_suspend = 0;
	
	switch(message)
	{
		case WM_PAINT:
		ValidateRect(hwnd, NULL);
		break;
		
		case WM_KEYDOWN:
		if(wParam == VK_ESCAPE)
		{
			PostQuitMessage(0);
		}else if(wParam == 'R')
		{
			game_reset(g_game);
		}
		break;
		
		case WM_LBUTTONDOWN:
		game_on_mouse_down(g_game, MouseButton::LEFT_CLICK, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
		case WM_LBUTTONUP:
		game_on_mouse_up(g_game, MouseButton::LEFT_CLICK, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
		
		case WM_RBUTTONDOWN:
		game_on_mouse_down(g_game, MouseButton::RIGHT_CLICK, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		
		break;
		case WM_RBUTTONUP:
		game_on_mouse_up(g_game, MouseButton::RIGHT_CLICK, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
		
		case WM_SIZE:
		if(wParam == SIZE_MINIMIZED)
		{
			if(!minimized)
			{
				minimized = 1;
				in_suspend = 1;
			}
		}
		else if(minimized)
		{
			minimized = 0;
			in_suspend = 0;
		}
		else if(!in_sizemove)
		{
			game_on_size_changed(g_game, LOWORD(lParam), HIWORD(lParam));
		}
		break;
		
		case WM_ENTERSIZEMOVE:
		in_sizemove = 1;
		break;
		
		case WM_EXITSIZEMOVE:
		in_sizemove = 0;
		RECT rc;
		GetClientRect(hwnd, &rc);
		game_on_size_changed(g_game, LOWORD(lParam), HIWORD(lParam));
		break;
		
		case WM_POWERBROADCAST:
		switch(wParam)
		{
			case PBT_APMQUERYSUSPEND:
			in_suspend = 1;
			return TRUE;
			
			case PBT_APMRESUMESUSPEND:
			if(!minimized)
			{
				in_suspend = 0;
			}
			return TRUE;
		}
		break;
		
		case WM_DESTROY:
		PostQuitMessage(0);
		break;
		
		default:
		return DefWindowProc(hwnd, message, wParam, lParam);
	}
	return 0;
}

////////////////////////////////
//~ nb: Entry Point
int WINAPI wWinMain(HINSTANCE hInstance,
					HINSTANCE hPrevInstance,
					PWSTR pCmdLine,
					int nCmdShow)
{
	CoInitializeEx(NULL, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
	
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(pCmdLine);
	
	const char *class_name = "D3D11Project";
	
	char *permanent_storage = (char*)VirtualAlloc(0, Megabytes(16), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	ASSERT(permanent_storage);
	
	//- nb: Main Arena setup
	Arena main_arena;
	arena_create(&main_arena, Megabytes(16), permanent_storage);
	
	
	
	////////////////////////////////
	//- nb: Game Creation
	Game game = {0};
	g_game = &game;
	{
		//- nb Window class registration
		WNDCLASSEX wc = {0};
		{
			wc.cbSize = sizeof(WNDCLASSEX);
			wc.style = CS_HREDRAW | CS_VREDRAW;
			wc.lpfnWndProc = WndProc;
			wc.hInstance = hInstance;
			wc.hCursor = LoadCursor(NULL, IDC_ARROW);
			wc.lpszClassName = class_name;
		}
		
		RegisterClassEx(&wc);
		
		//- nb: Window creation
		HWND hwnd = CreateWindowEx(WS_EX_NOREDIRECTIONBITMAP, // fix ugly resizing
								   class_name,
								   class_name,
								   WS_OVERLAPPEDWINDOW, //^ WS_THICKFRAME ^ WS_MAXIMIZEBOX,
								   CW_USEDEFAULT, CW_USEDEFAULT,
								   1280, 720,
								   NULL,
								   NULL,
								   hInstance,
								   NULL);
		
		game_init(&game);
		game_set_window(&game, hwnd, 1280, 720);
		// NOTE(nb): ShowWindow() issues a WM_SIZE event, which will create size dependant resources for us 
		ShowWindow(hwnd, nCmdShow);
	}
	
	MSG msg = {0};
	while(msg.message != WM_QUIT)
	{
		if(PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}else
		{
			game_render(&game);
		}
	}
	
	game_destroy(&game);
	VirtualFree(permanent_storage, 0, MEM_RELEASE);
	CoUninitialize();
	return 0;
}