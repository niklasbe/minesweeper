#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "ole32")

#include <intrin.h>
#define Assert(cond) do{ if(!(cond)) __debugbreak(); } while(0)

//#define _DEBUG

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

#define internal    static
#define global      static

#define Kilobytes(x) x*1024
#define Megabytes(x) x*1024*1024

#define RESERVE_SIZE Megabytes(64)
#define COMMIT_SIZE  Kilobytes(64)
#define PAGE_SIZE    4096

#define AlignPow2(pos, align) (((pos) + (align) - 1) & ~((align) - 1))
#define Min(A,B) (((A)<(B))?(A):(B))
#define Max(A,B) (((A)>(B))?(A):(B))
#define ClampTop(A,X) Min(A,X)
#define ClampBot(X,B) Max(X,B)
#define Clamp(A,X,B) (((X)<(A))?(A):((X)>(B))?(B):(X))

////////////////////////////////
//~ nb: Arena
typedef struct Arena Arena;
struct Arena 
{
	void *base_ptr;
	u64  reserved;
	u64  committed;
	u64  pos;
	
	u64 base_pos;
	u64 reserve_size;
	u64 commit_size;
};
Arena *arena_alloc()
{
	void *ptr = VirtualAlloc(0, RESERVE_SIZE, MEM_RESERVE, PAGE_READWRITE);
	VirtualAlloc(ptr, COMMIT_SIZE, MEM_COMMIT, PAGE_READWRITE);
	
	Arena *arena = (Arena*)ptr;
	{
		arena->pos          = AlignPow2(sizeof(Arena), 16);
		arena->base_pos     = 0;
		arena->reserved     = RESERVE_SIZE;
		arena->reserve_size = RESERVE_SIZE;
		arena->committed    = COMMIT_SIZE;
		arena->commit_size  = COMMIT_SIZE;
	}
	
	return arena;
}
void arena_release(Arena *arena)
{
	VirtualFree(arena, 0, MEM_RELEASE);
}
void *arena_push(Arena *arena, u64 size)
{
	u64 pos_pre = AlignPow2(arena->pos, 16);
	u64 pos_pst = pos_pre + size;
	// TODO(nb): chain more arenas
	if(pos_pst > arena->reserved)
	{
		__debugbreak();
	}
	// nb: commit new pages
	if(arena->committed < pos_pst)
	{
		__debugbreak();
		u64 cmt_pst_aligned = pos_pst + arena->commit_size - 1;
    cmt_pst_aligned -= cmt_pst_aligned % arena->commit_size;
    u64 cmt_pst_clamped = ClampTop(cmt_pst_aligned, arena->reserved);
    u64 cmt_size = cmt_pst_clamped - arena->committed;
    u8 *cmt_ptr = (u8 *)arena + arena->committed;
		VirtualAlloc(cmt_ptr, cmt_size, MEM_COMMIT, PAGE_READWRITE);
		arena->committed = cmt_pst_clamped;
	}
	// nb: return the start of the allocation, then update the cursor
	void *result = (u8*)arena + pos_pre;
	arena->pos = pos_pst;
	return result;
}
void arena_pop_to(Arena *arena, u64 pos)
{
	u64 big_pos = ClampBot(AlignPow2(sizeof(Arena), 16), pos);
	u64 new_pos = big_pos - arena->base_pos;
	Assert(new_pos <= arena->pos);
	arena->pos = new_pos;
}
void arena_clear(Arena *arena)
{
	arena_pop_to(arena, 0);
}
////////////////////////////////

#include "render.cpp"
//#include "font.cpp"
#include "game.cpp"

////////////////////////////////
//~ nb: Message callback
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
			game_reset();
		}
		break;
		
		case WM_LBUTTONDOWN:
		game_on_mouse_down(MouseButton::LEFT_CLICK, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
		case WM_LBUTTONUP:
		game_on_mouse_up(MouseButton::LEFT_CLICK, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
		
		case WM_RBUTTONDOWN:
		game_on_mouse_down(MouseButton::RIGHT_CLICK, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		
		break;
		case WM_RBUTTONUP:
		game_on_mouse_up(MouseButton::RIGHT_CLICK, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
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
			game_on_size_changed(LOWORD(lParam), HIWORD(lParam));
		}
		break;
		
		case WM_ENTERSIZEMOVE:
		in_sizemove = 1;
		break;
		
		case WM_EXITSIZEMOVE:
		in_sizemove = 0;
		RECT rc;
		GetClientRect(hwnd, &rc);
		game_on_size_changed(rc.right - rc.left, rc.bottom - rc.top);
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
	
	////////////////////////////////
	//- nb: Game Creation
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
		// nb: system inits
		r_init();
		game_init();
		//font_init();
		
		game_set_window(hwnd, 1280, 720);
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
			game_render();
			WaitMessage();
		}
	}
	
	//font_destroy();
	game_destroy();
	r_destroy();
	CoUninitialize();
	return 0;
}