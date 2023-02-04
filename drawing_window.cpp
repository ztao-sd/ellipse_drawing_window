#include "drawing_window.h"

template <class DerivedWindow>
BOOL BasicWindow<DerivedWindow>::Create(
	PCWSTR lpWindowName, // 
	DWORD dwStyle,
	DWORD dwExStyle,
	int x,
	int y,
	int nWidth,
	int nHeight,
	HWND hWndParent,
	HMENU hMenu
)
{
	m_wc = {};
	m_wc.lpfnWndProc = DerivedWindow::WindowProc;	// Window message handler
	m_wc.hInstance = GetModuleHandle(NULL);			// Application instance handle
	m_wc.lpszClassName = ClassName();				// Class name

	RegisterClass(&m_wc);	// Register Window class

	m_hwnd = CreateWindowEx(	// Create new instance of a window
		dwExStyle,		// Extended Window style
		ClassName(),	// Class name (optional)
		lpWindowName,	// Window text
		dwStyle,		// Window style
		x,				// X coordinate
		y,				// Y coordinate
		nWidth,			// Width
		nHeight,		// Height
		hWndParent,		// Parent Window
		hMenu,			// Menu Window
		GetModuleHandle(NULL),	// Instance handle
		this			// Additional data
	);

	return (m_hwnd ? TRUE : FALSE);
}

template <class T> 
void SafeRelease(T** ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = NULL;
	}
}

#pragma region ShapeList

template<class Shape>
inline std::shared_ptr<Shape> ShapeList<Shape>::SelectedShape()
{
	return pShape;
}

template<class Shape>
HRESULT ShapeList<Shape>::InsertShape(float dipX, float dipY)
{
	try
	{
		shapes.insert(shapes.end(),
			std::shared_ptr<Shape>(new Shape(D2D1::Point2F(dipX, dipY), D2D1::ColorF(colors[colorIndex]))));
		colorIndex = (colorIndex + 1) % ARRAYSIZE(colors);
		pShape = shapes.back();
	}
	catch (std::bad_alloc)
	{
		return E_OUTOFMEMORY;
	}

	return S_OK;
}

template<class Shape>
BOOL ShapeList<Shape>::SelectShape(float dipX, float dipY)
{
	for (auto i{ shapes.rbegin() }; i != shapes.rend(); ++i) // Use reverse iterator
	{
		if ((*i)->HitTest(dipX, dipY))
		{
			// shapeIterator = (++i).base();	// Return the base iterator form reverse iterator
			pShape = *i;
			return TRUE;
		}
	}
	return FALSE;
}

template<typename Shape>
void ShapeList<Shape>::Draw(ID2D1RenderTarget* pRenderTarget, ID2D1SolidColorBrush* pBrush)
{
	for (auto i{ shapes.begin() }; i != shapes.end(); ++i)
	{
		(*i)->Draw(pRenderTarget, pBrush);
	}
}

#pragma endregion

#pragma region DrawingWindow

LRESULT DrawingWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		// On window creation
	case WM_CREATE:
		if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory)))	// Create factory
		{
			return -1;
		}
		DPIScale::Init(m_hwnd);	// Initialize DPI scale class
		SetMode(Mode::DrawMode);

		return 0;

		// On window destruction
	case WM_DESTROY:
		DiscardGraphicsResources();
		SafeRelease(&pFactory);
		PostQuitMessage(0);
		return 0;

		// On window resize
	case WM_SIZE:
		Resize();
		return 0;

		// 2D drawing
	case WM_PAINT:
		OnPaint();
		return 0;

		// Mouse input
		// lParam containes the x and y coordinates of the mouse pointer.
	case WM_LBUTTONDOWN:
		OnLeftButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (DWORD)wParam);
		return 0;
	case WM_LBUTTONUP:
		OnLeftButtonUp();
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (DWORD)wParam);
		return 0;
	case WM_MOUSEWHEEL:
		if (GetSystemMetrics(SM_MOUSEWHEELPRESENT) != 0)
		{
			OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
		}
		return 0;

		// Set cursor
	case WM_SETCURSOR:
		if (LOWORD(lParam) == HTCLIENT)
		{
			SetCursor(hCursor);
			return TRUE;
		}
		break;

		// Keyboard input
	case WM_KEYDOWN:
	case WM_COMMAND: // Accelerator table
		switch (LOWORD(wParam))
		{
		case ID_DRAW_MODE:
			SetMode(Mode::DragMode);
			break;
		case ID_SELECT_MODE:
			SetMode(Mode::SelectionMode);
			break;
		case ID_TOGGLE_MODE:
			if (mode == Mode::DrawMode) SetMode(Mode::SelectionMode);
			else SetMode(Mode::DrawMode);
			break;
		}
		return 0;

	default:
		return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
	}
}

void DrawingWindow::SetMode(Mode mode)
{
	{
		LPWSTR cursor = IDC_ARROW;
		this->mode = mode;
		switch (this->mode)
		{
		case Mode::DrawMode:
			cursor = IDC_CROSS;
			break;
		case Mode::SelectionMode:
			cursor = IDC_HAND;
			break;
		case Mode::DragMode:
			cursor = IDC_SIZEALL;
			break;
		}

		hCursor = LoadCursor(NULL, cursor);
		SetCursor(hCursor);
	}
}

HRESULT DrawingWindow::CreateGraphicsResources()
{
	HRESULT hr = S_OK;
	if (pRenderTarget == NULL) // Only create resources once
	{
		RECT rc;
		GetClientRect(m_hwnd, &rc);

		D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

		hr = pFactory->CreateHwndRenderTarget(
			D2D1::RenderTargetProperties(),					// Default options
			D2D1::HwndRenderTargetProperties(m_hwnd, size),	// Window handle & size of render target
			&pRenderTarget									// ID2D1HwndRenderTarget pointer
		);

		if (SUCCEEDED(hr))
		{
			const D2D1_COLOR_F color = D2D1::ColorF(1.0f, 1.0f, 0);		// Color
			hr = pRenderTarget->CreateSolidColorBrush(color, &pBrush);	//

			if (SUCCEEDED(hr))
			{
				//CalculateLayout();
			}
		}

	}
	return hr;
}

void DrawingWindow::DiscardGraphicsResources()
{
	SafeRelease(&pRenderTarget);
	SafeRelease(&pBrush);
}

//void DrawingWindow::CalculateLayout()
//{
//	if (pRenderTarget != NULL)
//	{
//		D2D1_SIZE_F size = pRenderTarget->GetSize();
//		const float x = size.width / 2;
//		const float y = size.height / 2;
//		const float radius = min(x / 2, y / 2);
//		ellipse = D2D1::Ellipse(D2D1::Point2F(x, y), radius * 1.5f, radius);
//	}
//}

void DrawingWindow::Resize()
{
	if (pRenderTarget != NULL)
	{
		RECT rc;
		GetClientRect(m_hwnd, &rc);

		D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

		pRenderTarget->Resize(size);
		//CalculateLayout();
		InvalidateRect(m_hwnd, NULL, FALSE);
	}
}

void DrawingWindow::OnPaint()
{
	HRESULT hr = CreateGraphicsResources();
	if (SUCCEEDED(hr))
	{
		PAINTSTRUCT ps;
		BeginPaint(m_hwnd, &ps);

		pRenderTarget->BeginDraw(); // Start drawing

		pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::SkyBlue)); // Fill the entire render target with a solid color
		//pRenderTarget->FillEllipse(ellipse, pBrush); // Draw a filled ellipse

		ellipses.Draw(pRenderTarget, pBrush);

		hr = pRenderTarget->EndDraw(); // End drawing (error signal from here)
		if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
		{
			DiscardGraphicsResources();
		}

		EndPaint(m_hwnd, &ps);
	}
}

void DrawingWindow::OnLeftButtonDown(int pixelX, int pixelY, DWORD flags)
{

	ptMouse = DPIScale::PixelsToDIPs(pixelX, pixelY);

	if (mode == Mode::DrawMode)
	{
		POINT pt { pixelX, pixelY };
		if (DragDetect(m_hwnd, pt))
		{
			drawStartPos = ptMouse;
			SetCapture(m_hwnd);	// Capture the mouse
			ellipses.InsertShape(drawStartPos.x, drawStartPos.y);
		}
	}
	else
	{
		ellipses.ClearSelection();
		if (ellipses.SelectShape(ptMouse.x, ptMouse.y))
		{
			SetCapture(m_hwnd);
			dragObjRelPos = ellipses.SelectedShape()->Shape().point;
			dragObjRelPos.x -= ptMouse.x;
			dragObjRelPos.y -= ptMouse.y;

			SetMode(Mode::DragMode);
		}
	}
	pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity()); // Rotate and translate
	
	InvalidateRect(m_hwnd, NULL, FALSE); // Force window to be repainted
}

void DrawingWindow::OnMouseMove(int pixelX, int pixelY, DWORD flags)
{
	const D2D1_POINT_2F dips = DPIScale::PixelsToDIPs(pixelX, pixelY);
	if (flags & MK_LBUTTON)
	{
		if (mode == Mode::DrawMode)
		{
			const float width = (dips.x - drawStartPos.x) / 2;
			const float height = (dips.y - drawStartPos.y) / 2;
			const float x1 = drawStartPos.x + width;
			const float y1 = drawStartPos.y + height;
			//ellipse = D2D1::Ellipse(D2D1::Point2F(x1, y1), width, height);

			if (ellipses.SelectedShape())
			{
				ellipses.SelectedShape()->Shape() = D2D1::Ellipse(D2D1::Point2F(x1, y1), width, height);
			}
			
		}
		else if (mode == Mode::DragMode)
		{
			if (ellipses.SelectedShape())
			{
				ellipses.SelectedShape()->Shape().point.x = dips.x + dragObjRelPos.x;
				ellipses.SelectedShape()->Shape().point.y = dips.y + dragObjRelPos.y;
			}
		}		
		InvalidateRect(m_hwnd, NULL, FALSE);
	}
}

void DrawingWindow::OnLeftButtonUp()
{
	if ((mode == Mode::DrawMode))
	{
		if (ellipses.SelectedShape()) ellipses.ClearSelection();
		InvalidateRect(m_hwnd, NULL, FALSE);
	}
	else if (mode == Mode::DragMode)
	{
		SetMode(Mode::SelectionMode);
	}
	ReleaseCapture();
}

void DrawingWindow::OnMouseWheel(int delta)
{
	if (ellipses.SelectedShape())
	{
		ellipses.SelectedShape()->Rotation() += static_cast<float>(delta) / 120.0f * 4; // Rotation angle in degrees
	}

	InvalidateRect(m_hwnd, NULL, FALSE);
}

void DrawingWindow::OnKeyDown(UINT vkey)
{
	switch (vkey)
	{
	case VK_BACK:
		break;
	case VK_DELETE:
		break;
	case VK_LEFT:
		break;
	case VK_RIGHT:
		break;
	case VK_UP:
		break;
	case VK_DOWN:
		break;
	default:
		return;
	}
}

#pragma endregion

#pragma region ColorEllipse

ColorEllipse::ColorEllipse(D2D1_POINT_2F cursorPosition, D2D1_COLOR_F color)
{
	ellipse.point = cursorPosition;
	ellipse.radiusX = 1.0f;
	ellipse.radiusY = 1.0f;
	this->color = color;
}

void ColorEllipse::Draw(ID2D1RenderTarget* pRenderTarget, ID2D1SolidColorBrush* pBrush)
{
	pRenderTarget->SetTransform(D2D1::Matrix3x2F::Rotation(rotation, ellipse.point));
	pBrush->SetColor(color);
	pRenderTarget->FillEllipse(ellipse, pBrush);
	pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
	pRenderTarget->DrawEllipse(ellipse, pBrush, 1.0f);
	pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
}

BOOL ColorEllipse::HitTest(float dipX, float dipY)
{
	const float a = ellipse.radiusX;
	const float b = ellipse.radiusY;
	const float x = dipX - ellipse.point.x;
	const float y = dipY - ellipse.point.y;
	const float xR = x * cos(-rotation * pi / 180) - y * sin(-rotation * pi / 180);
	const float yR = x * sin(-rotation * pi / 180) + y * cos(-rotation * pi / 180);

	return (xR * xR) / (a * a) + (yR * yR) / (b * b) <= 1.0f;
}

void ColorEllipse::Move(float deltaX, float deltaY)
{
	ellipse.point.x += deltaX;
	ellipse.point.y += deltaY;
}

#pragma endregion


// ------------------------------------------------- Demo ----------------------------------

int DrawingWindowDemo(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	DrawingWindow drawingWin{};

	if (!drawingWin.Create(L"Drawing Ellipse", WS_OVERLAPPEDWINDOW))
	{
		return 0;
	}

	// Acceleration table
	HACCEL hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR1));

	ShowWindow(drawingWin.Window(), nCmdShow);

	// Message loop
	MSG msg{};
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(drawingWin.Window(), hAccel, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

	}
	return 0;
}
