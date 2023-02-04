//#include "window_demo.h"

#include "drawing_window.h"
#include "bitmap.h"

float DPIScale::s_scale = 1.0f;

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR pCmdLine, _In_ int nCmdShow)
{
	
	DrawingWindowDemo(hInstance, hPrevInstance, pCmdLine, nCmdShow);

	return 0;
}