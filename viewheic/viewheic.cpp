/*
  Copyright (c) 2022 Makoto Tanabe <mtanabe.sj@outlook.com>
  Licensed under the MIT License.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/
// viewheic.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "viewheic.h"
#include "app.h"
#include "AboutDlg.h"
#include <shlwapi.h> // StrCmpLogicalW
#include <algorithm>


// this embeds a manifest for using v6 common controls of win32.
// it is needed for a SysLink control used in the about box.
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


#define VIEWHEIC_FLAG_CONVERT_TO_JPEG 0x00000001

#ifdef _DEBUG
#define PHOTOMARGIN_CX 0
#define PHOTOMARGIN_CY 0
#else//#ifdef _DEBUG
#define PHOTOMARGIN_CX 0
#define PHOTOMARGIN_CY 0
#endif//#ifdef _DEBUG


// global variables
HMODULE AppInstanceHandle;
ULONG LibRefCount;

bool parseCommandline(LPCWSTR cmdline, std::vector<std::wstring> &paths, ULONG &outflags);
HRESULT convertHeifToJpeg(LPCWSTR srcfile, LPCWSTR destfile);

//////////////////////////////////////////////////////////////////////////

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	AppInstanceHandle = hInstance;
	LibRefCount = 0;

	ULONG flags = 0;
	std::vector<std::wstring> args;
	if (!parseCommandline(lpCmdLine, args, flags))
		return ERROR_INVALID_PARAMETER;

	if (flags & VIEWHEIC_FLAG_CONVERT_TO_JPEG)
	{
		if (args.size() != 2)
			return ERROR_INVALID_PARAMETER;
		return convertHeifToJpeg(args[0].c_str(), args[1].c_str());
	}

	// run the viewer as a modeless dialog with our own message loop. a modal dialog box would not let us use the keyboard accelerators.
	MainDlg dlg(args[0]);
	if (!dlg.Create())
		return GetLastError();
	ShowWindow(dlg.GetHwnd(), nCmdShow);
	HACCEL haccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_MAINFRAME));
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0)
	{
		HWND hwnd = dlg.GetHwnd();
		if (hwnd && (TranslateAccelerator(hwnd, haccel, &msg) || IsDialogMessage(hwnd, &msg)))
			continue;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return ERROR_SUCCESS;
}

bool parseCommandline(LPCWSTR cmdline, std::vector<std::wstring> &args, ULONG &outflags)
{
	strvalenum senum(' ', cmdline);
	LPVOID pos = senum.getHeadPosition();
	while (pos)
	{
		LPCWSTR p = senum.getNext(pos);
		if (*p == '-')
		{
			if (p[1] == 'j')
				outflags |= VIEWHEIC_FLAG_CONVERT_TO_JPEG;
			else
				return false;
		}
		else
			args.push_back(p);
	}
	if (args.size() == 0)
		args.push_back(L"");
	return true;
}

HRESULT convertHeifToJpeg(LPCWSTR srcfile, LPCWSTR destfile)
{
	HRESULT hr = CoInitialize(NULL);
	if (SUCCEEDED(hr))
	{
		HeifImage heif2;
		if (heif2.init(NULL, srcfile, 0))
		{
			HBITMAP hbmp = heif2.decode({ 0 });
			if (hbmp)
			{
				DeleteObject(hbmp);
				hr = heif2.convert(destfile);
			}
			else
				hr = E_FAIL;
		}
		else
			hr = E_UNEXPECTED;
		CoUninitialize();
	}
	return hr;
}


////////////////////////////////////////////////////////////////
#define SCROLLER_MORE_TRANSPARENT 0x7f
#define SCROLLER_LESS_TRANSPARENT 0xaa
#define SCROLLER_MARGIN 2
#define SCROLLER_WIDTH 8
#define SCROLLER_WIDTHF 8.0
#define SCROLLER_STEP_DISTANCE 10

BOOL VIEWINFO::blendRect(HDC hdc, int x, int y, int cx, int cy, BYTE transparency)
{
	if (cx <= 0 || cy <= 0) return FALSE;
	HDC hdc2 = CreateCompatibleDC(hdc);
	if (hdc2) {
		BITMAPINFO bmi = { sizeof(BITMAPINFOHEADER), cx, cy, 1, 32, BI_RGB, (DWORD)(cx*cy * 4) };
		UINT32* bits;
		HBITMAP hbmp = CreateDIBSection(hdc2, &bmi, DIB_RGB_COLORS, (LPVOID*)&bits, NULL, 0);
		if (hbmp) {
			RECT rc = {0,0,cx,cy};
			FillRect(hdc2, &rc, GetStockBrush(GRAY_BRUSH));
			SelectObject(hdc2, hbmp);
			BLENDFUNCTION bf = { AC_SRC_OVER, 0, transparency, 0 };
			AlphaBlend(hdc, x, y, cx, cy, hdc2, 0, 0, cx, cy, bf);
			DeleteObject(hbmp);
		}
		DeleteDC(hdc2);
	}
	return TRUE;
}

void VIEWINFO::drawScrollerH(HDC hdc, BYTE transparency)
{
	float rco, rci, rcw, r, rd;
	rci = (float)szImage.cx;
	rcw = (float)szPage.cx;
	rco = (float)(-ptPage.x);
	rcw -= SCROLLER_WIDTHF;
	r = rco / rci;
	rd = rcw / rci;
	int x = (int)(r*rcw) + SCROLLER_MARGIN;
	int cx = (int)(rd*rcw);
	rectH = { x, szPage.cy - SCROLLER_MARGIN - SCROLLER_WIDTH, cx, SCROLLER_WIDTH };
	blendRect(hdc, rectH.left, rectH.top, rectH.right, rectH.bottom, transparency);
	rectH.right += rectH.left;
	rectH.bottom += rectH.top;
}

void VIEWINFO::drawScrollerV(HDC hdc, BYTE transparency)
{
	float rco, rci, rcw, r, rd;
	rci = (float)szImage.cy;
	rcw = (float)szPage.cy;
	rco = (float)(-ptPage.y);
	rcw -= SCROLLER_WIDTHF;
	r = rco / rci;
	rd = rcw / rci;
	int y = (int)(r*rcw) + SCROLLER_MARGIN;
	int cy = (int)(rd*rcw);
	rectV = { szPage.cx - SCROLLER_MARGIN - SCROLLER_WIDTH, y, SCROLLER_WIDTH, cy };
	blendRect(hdc, rectV.left, rectV.top, rectV.right, rectV.bottom, transparency);
	rectV.right += rectV.left;
	rectV.bottom += rectV.top;
}

BOOL VIEWINFO::drawScrollers(HDC hdc, BYTE transparency)
{
	if (scrollH & VI_SCROLLER_VISIBLE)
		drawScrollerH(hdc, transparency);

	if (scrollV & VI_SCROLLER_VISIBLE)
		drawScrollerV(hdc, transparency);

	return TRUE;
}

void VIEWINFO::onMouseMove(HWND hwnd, UINT state, POINT pt)
{
	if (scrollH & VI_SCROLLER_VISIBLE)
	{
		if (scrollH & VI_SCROLLER_CAPTURED)
		{
			int dx = pt.x - ptCapture.x;
			int cx = MulDiv(szImage.cx, dx, szPage.cx);
			// a negative change in cursor position means a scrollbar moving left. a positive change a scrollbar moving right.
			ptPage.x -= cx;
			int xmin = szPage.cx - szImage.cx;
			if (xmin <= ptPage.x && ptPage.x < 0)
				InvalidateRect(hwnd, NULL, TRUE);
			if (ptPage.x < xmin)
				ptPage.x = xmin;
			else if (ptPage.x > 0)
				ptPage.x = 0;
			ptCapture = pt;
			return;
		}
		if (PtInRect(&rectH, pt))
		{
			// cursor has moved into the scrollbar area. increase opacity. make it less transparent.
			HDC hdc = GetDC(hwnd);
			drawScrollerH(hdc, SCROLLER_LESS_TRANSPARENT);
			ReleaseDC(hwnd, hdc);
			scrollH |= VI_SCROLLER_SHADED;
		}
		else if (scrollH & VI_SCROLLER_SHADED)
		{
			// cursor has moved out. reduce opacity.
			InvalidateRect(hwnd, &rectH, TRUE);
			scrollH &= ~VI_SCROLLER_SHADED;
		}
	}
	if (scrollV & VI_SCROLLER_VISIBLE)
	{
		if (scrollV & VI_SCROLLER_CAPTURED)
		{
			int dy = pt.y - ptCapture.y;
			int cy = MulDiv(szImage.cy, dy, szPage.cy);
			// a negative change in cursor position means a scrollbar moving up. a positive change a scrollbar moving down.
			ptPage.y -= cy;
			int ymin = szPage.cy - szImage.cy;
			if (ymin <= ptPage.y && ptPage.y < 0)
				InvalidateRect(hwnd, NULL, TRUE);
			if (ptPage.y < ymin)
				ptPage.y = ymin;
			else if (ptPage.y > 0)
				ptPage.y = 0;
			ptCapture = pt;
			return;
		}
		if (PtInRect(&rectV, pt))
		{
			HDC hdc = GetDC(hwnd);
			drawScrollerV(hdc, SCROLLER_LESS_TRANSPARENT);
			ReleaseDC(hwnd, hdc);
			scrollV |= VI_SCROLLER_SHADED;
		}
		else if (scrollV & VI_SCROLLER_SHADED)
		{
			InvalidateRect(hwnd, &rectV, TRUE);
			scrollV &= ~VI_SCROLLER_SHADED;
		}
	}
}

bool VIEWINFO::onMouseWheel(short nButtonType, short nWheelDelta, POINT pt)
{
	if (scrollH & VI_SCROLLER_VISIBLE)
	{
		// wheel sideways if the mouse cursor is in a region near the bottom edge of the window (including the horizontal scrollbar) 
		RECT rc = { 0, rectH.top - 10, szPage.cx, szPage.cy };
		if (PtInRect(&rc, pt))
		{
			int x0 = ptPage.x;
			int dx = szImage.cx - szPage.cx;
			if (nWheelDelta > 0)
			{
				// scroll sideways to the left
				do {
					// note x is negative. so stop if it's no longer negative.
					ptPage.x += SCROLLER_STEP_DISTANCE;
					if (ptPage.x > 0)
					{
						ptPage.x = 0;
						break;
					}
				} while ((nWheelDelta -= WHEEL_DELTA) > 0);
			}
			else if (nWheelDelta < 0)
			{
				// scroll to the right
				do {
					ptPage.x -= SCROLLER_STEP_DISTANCE;
					if (ptPage.x < -dx)
					{
						ptPage.x = -dx;
						break;
					}
				} while ((nWheelDelta += WHEEL_DELTA) < 0);
			}
			return (x0 != ptPage.x);
		}
	}
	if (scrollV & VI_SCROLLER_VISIBLE)
	{
		int y0 = ptPage.y;
		int dy = szImage.cy - szPage.cy;
		if (nWheelDelta > 0)
		{
			// wheel is moving forward; scroll up
			do {
				// note y is negative. so stop if it's no longer negative.
				ptPage.y += SCROLLER_STEP_DISTANCE;
				if (ptPage.y > 0)
				{
					ptPage.y = 0;
					break;
				}
			} while ((nWheelDelta -= WHEEL_DELTA) > 0);
		}
		else if (nWheelDelta < 0)
		{
			// wheeling backward; scroll down
			do {
				ptPage.y -= SCROLLER_STEP_DISTANCE;
				if (ptPage.y < -dy)
				{
					ptPage.y = -dy;
					break;
				}
			} while ((nWheelDelta += WHEEL_DELTA) < 0);
		}
		return (y0 != ptPage.y);
	}
	return FALSE;
}

bool VIEWINFO::startScroll(HWND hwnd, UINT buttonType, POINT pt)
{
	if (PtInRect(&rectH, pt))
	{
		scrollH |= VI_SCROLLER_CAPTURED;
		ptCapture = pt;
		rectCapture = rectH;
		SetCapture(hwnd);
		return true;
	}
	if (PtInRect(&rectV, pt))
	{
		scrollV |= VI_SCROLLER_CAPTURED;
		ptCapture = pt;
		rectCapture = rectV;
		SetCapture(hwnd);
		return true;
	}
	return false;
}

bool VIEWINFO::stopScroll(HWND hwnd, UINT buttonType, POINT pt)
{
	if (scrollH & VI_SCROLLER_CAPTURED)
	{
		scrollH &= ~VI_SCROLLER_CAPTURED;
		ptCapture = { 0 };
		ReleaseCapture();
		return true;
	}
	if (scrollV & VI_SCROLLER_CAPTURED)
	{
		scrollV &= ~VI_SCROLLER_CAPTURED;
		ptCapture = { 0 };
		ReleaseCapture();
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

BOOL MainDlg::OnInitDialog()
{
	SimpleDlg::OnInitDialog();

	INITCOMMONCONTROLSEX ccx = { sizeof(INITCOMMONCONTROLSEX), ICC_TREEVIEW_CLASSES };
	InitCommonControlsEx(&ccx);

	// set up a custom icon for the app.
	HICON hicon = LoadIcon(AppInstanceHandle, MAKEINTRESOURCE(IDI_APP)); // note we don't save the handle in _hicon.
	SendMessage(_hdlg, WM_SETICON, ICON_BIG, (LPARAM)hicon);
	hicon = LoadIcon(AppInstanceHandle, MAKEINTRESOURCE(IDI_APP_SMALL));
	SendMessage(_hdlg, WM_SETICON, ICON_SMALL, (ULONG_PTR)hicon);

	// load optional settings from the user's registry.
	DWORD val;
	if (ERROR_SUCCESS == Registry_GetDwordValue(HKEY_CURRENT_USER, APPREGKEY, L"ViewFlags", &val))
		_heifFlags = val;

	// the area around a photo smaller than the window is painted in this color.
	_hbrBack = CreateSolidBrush(RGB(32, 32, 32));

	// find the margins, the difference between the window frame and the image display area. they include the menu bar.
	RECT rcFrame, rcClient;
	GetWindowRect(_hdlg, &rcFrame);
	GetClientRect(_hdlg, &rcClient);
	_frameMargins.cx = rcFrame.right - rcFrame.left - rcClient.right;
	_frameMargins.cy = rcFrame.bottom - rcFrame.top - rcClient.bottom;

	// if an image filename has been given, open it and display the image. otherwise, run a file browse dialog and let the user choose an image to show.
	if (_nextFilename.empty())
		PostMessage(_hdlg, WM_COMMAND, MAKEWPARAM(IDM_OPEN, 0), 0);
	else
		_openFile(_nextFilename);
	return TRUE;
}

BOOL MainDlg::OnOK()
{
	// the dialog manager sends IDOK for the Enter key. there is nothing to confirm in a viewer.
	return TRUE;
}

BOOL MainDlg::OnCancel()
{
	// Esc, the close button and File > Exit end up here. we are modeless, so destroy the window instead of calling EndDialog.
	DestroyWindow(_hdlg);
	return TRUE;
}

void MainDlg::OnDestroy()
{
	if (_heif)
		delete _heif;
	if (_hbmpImage)
		DeleteObject(_hbmpImage);
	if (_hbrBack)
		DeleteObject(_hbrBack);
	SimpleDlg::OnDestroy();
	// the main window is gone. end the message loop in wWinMain.
	PostQuitMessage(0);
}

void MainDlg::_setCaption(ULONG flags)
{
	LPCWSTR caption = wcsrchr(_filename.c_str(), '\\');
	if (caption)
		caption++;
	else
		caption = _filename.c_str();
	std::wstring c;
	LPCWSTR extension = wcsrchr(caption, '.');
	if (extension)
		c.assign(caption, extension - caption);
	else
		c = caption;
	int deg = 0;
	if (_heif && _heif->_data.rotated > 0)
		deg = (_heif->_data.rotated % 4) * 90;
	if (deg)
	{
		resstring rotationAngle(resstring(AppInstanceHandle, IDS_ROTATION_ANGLE_FMT), deg);
		c += rotationAngle;
	}
	if (_heif && _vi.zoomRatio && ((flags & 1) || (_vi.zoomRatio != 100)))
	{
		resstring percentZoom(L" (%d%%)", _vi.zoomRatio);
		c += percentZoom;
	}
	// show where the photo is among the photos of its folder.
	if (_photoIndex && _photoCount > 1)
	{
		resstring position(L"  \x00B7  %d / %d", _photoIndex, _photoCount);
		c += position;
	}
	SetWindowText(_hdlg, c.c_str());
}

void MainDlg::_autoZoom(bool syncParams)
{
	// the largest image area that fits on the monitor, after the window frame and the menu bar.
	MONITORINFO mi = { sizeof(mi) };
	GetMonitorInfo(MonitorFromWindow(_hdlg, MONITOR_DEFAULTTONEAREST), &mi);
	_vi.szScreen.cx = mi.rcWork.right - mi.rcWork.left - _frameMargins.cx;
	_vi.szScreen.cy = mi.rcWork.bottom - mi.rcWork.top - _frameMargins.cy;

	SIZE szView;
	int zoom = 0;
	if (_heif->_imageSize.cx > _vi.szScreen.cx || _heif->_imageSize.cy > _vi.szScreen.cy)
	{
		RECT rc;
		int z = PreserveAspectRatio(_heif->_imageSize.cx, _heif->_imageSize.cy, _vi.szScreen.cx, _vi.szScreen.cy, &rc);
		zoom = (z / 5) * 5;
		if (zoom < z)
		{
			double fz = 0.01*(double)zoom;
			SIZE s2 = _heif->_imageSize;
			s2.cx = (int)(fz*(double)s2.cx);
			s2.cy = (int)(fz*(double)s2.cy);
			PreserveAspectRatio(s2.cx, s2.cy, _vi.szScreen.cx, _vi.szScreen.cy, &rc);
		}
		szView.cx = rc.right - rc.left;
		szView.cy = rc.bottom - rc.top;
	}
	else
	{
		szView = _heif->_imageSize;
		zoom = 100; // 100 percent; no zoom
	}
	if (syncParams)
	{
		_vi.zoomAuto = _vi.zoomRatio = zoom;
		_vi.szImage = _vi.szAuto = _vi.szPage = szView;
		_vi.ptPage = { 0 };
	}
	_sizeWindow(szView);
}

// the smallest image area. it keeps the window wide enough for the menu bar to fit on one line.
#define MIN_VIEW_CX 400
#define MIN_VIEW_CY 240

void MainDlg::_sizeWindow(SIZE viewSize)
{
	if (viewSize.cx < MIN_VIEW_CX)
		viewSize.cx = MIN_VIEW_CX;
	if (viewSize.cy < MIN_VIEW_CY)
		viewSize.cy = MIN_VIEW_CY;
	SIZE szDlg = { viewSize.cx + 2 * PHOTOMARGIN_CX + _frameMargins.cx, viewSize.cy + 2 * PHOTOMARGIN_CY + _frameMargins.cy };
	// keep the window on its monitor. the next photo may be taller and would otherwise extend past the bottom of the screen.
	RECT rc;
	GetWindowRect(_hdlg, &rc);
	MONITORINFO mi = { sizeof(mi) };
	GetMonitorInfo(MonitorFromWindow(_hdlg, MONITOR_DEFAULTTONEAREST), &mi);
	POINT pos = { rc.left, rc.top };
	if (pos.x + szDlg.cx > mi.rcWork.right)
		pos.x = mi.rcWork.right - szDlg.cx;
	if (pos.y + szDlg.cy > mi.rcWork.bottom)
		pos.y = mi.rcWork.bottom - szDlg.cy;
	if (pos.x < mi.rcWork.left)
		pos.x = mi.rcWork.left;
	if (pos.y < mi.rcWork.top)
		pos.y = mi.rcWork.top;
	SetWindowPos(_hdlg, NULL, pos.x, pos.y, szDlg.cx, szDlg.cy, SWP_NOZORDER | SWP_NOACTIVATE);
	// WM_SIZE does not arrive if the size has not changed. so, update the page here too.
	GetClientRect(_hdlg, &rc);
	_vi.szPage = { rc.right - 2 * PHOTOMARGIN_CX, rc.bottom - 2 * PHOTOMARGIN_CY };
	_layoutPage();
}

// centers the image if it is smaller than the page. otherwise, keeps the scroll position in range and shows the scrollers.
void MainDlg::_layoutPage()
{
	if (_vi.szImage.cx <= _vi.szPage.cx)
	{
		_vi.ptPage.x = (_vi.szPage.cx - _vi.szImage.cx) / 2;
		_vi.scrollH &= ~VI_SCROLLER_VISIBLE;
	}
	else
	{
		if (_vi.ptPage.x > 0)
			_vi.ptPage.x = 0;
		else if (_vi.ptPage.x < _vi.szPage.cx - _vi.szImage.cx)
			_vi.ptPage.x = _vi.szPage.cx - _vi.szImage.cx;
		_vi.scrollH |= VI_SCROLLER_VISIBLE;
	}
	if (_vi.szImage.cy <= _vi.szPage.cy)
	{
		_vi.ptPage.y = (_vi.szPage.cy - _vi.szImage.cy) / 2;
		_vi.scrollV &= ~VI_SCROLLER_VISIBLE;
	}
	else
	{
		if (_vi.ptPage.y > 0)
			_vi.ptPage.y = 0;
		else if (_vi.ptPage.y < _vi.szPage.cy - _vi.szImage.cy)
			_vi.ptPage.y = _vi.szPage.cy - _vi.szImage.cy;
		_vi.scrollV |= VI_SCROLLER_VISIBLE;
	}
}

POINT MainDlg::_pageCenter()
{
	return { _vi.szPage.cx / 2, _vi.szPage.cy / 2 };
}

// the zoom ratio at which the whole image fits in the window. it is never more than 100 percent.
int MainDlg::_fitZoom()
{
	SIZE sz = _heif->_imageSize;
	if (_heif->_data.rotated & 1)
		std::swap(sz.cx, sz.cy);
	if (sz.cx <= 0 || sz.cy <= 0)
		return 100;
	// round down so that the image does not overflow the window by a pixel.
	int zx = (int)((LONGLONG)100 * _vi.szPage.cx / sz.cx);
	int zy = (int)((LONGLONG)100 * _vi.szPage.cy / sz.cy);
	int z = zx < zy ? zx : zy;
	if (z > 100)
		z = 100;
	return z < 1 ? 1 : z;
}

// zoom steps used by zoom in and out. the ratio that fits the window is added as one more step.
static const int ZoomSteps[] = { 10, 15, 20, 25, 33, 50, 67, 75, 100, 125, 150, 200 };

void MainDlg::_stepZoom(int steps, POINT anchor)
{
	if (!_heif || !_hbmpImage)
		return;
	int fit = _fitZoom();
	int ratio = _vi.zoomRatio;
	for (; steps > 0; steps--)
	{
		// the smallest step above the current ratio.
		int next = (fit > ratio) ? fit : INT_MAX;
		for (int z : ZoomSteps)
			if (z > ratio && z < next)
				next = z;
		if (next == INT_MAX)
			break;
		ratio = next;
	}
	for (; steps < 0; steps++)
	{
		// the largest step below the current ratio.
		int prev = (fit < ratio) ? fit : 0;
		for (int z : ZoomSteps)
			if (z < ratio && z > prev)
				prev = z;
		if (prev == 0)
			break;
		ratio = prev;
	}
	_setZoom(ratio, anchor);
}

// rescales the image to ratio percent of its full size. the image point under anchor stays where it is.
void MainDlg::_setZoom(int ratio, POINT anchor)
{
	if (!_heif || !_hbmpImage || ratio == _vi.zoomRatio)
		return;
	SetCursor(LoadCursor(NULL, IDC_WAIT));
	// find where the anchor is on the image, as a fraction of the image extents.
	double fx = _vi.szImage.cx ? (double)(anchor.x - _vi.ptPage.x) / _vi.szImage.cx : 0.5;
	double fy = _vi.szImage.cy ? (double)(anchor.y - _vi.ptPage.y) / _vi.szImage.cy : 0.5;
	DeleteObject(_hbmpImage);
	// zoom keeps the rotation, and reports the extents of the new bitmap in szImage.
	_hbmpImage = _heif->zoom(ratio, _vi.szImage);
	_vi.zoomRatio = ratio;
	_vi.ptPage.x = anchor.x - (int)(fx * _vi.szImage.cx);
	_vi.ptPage.y = anchor.y - (int)(fy * _vi.szImage.cy);
	_layoutPage();
	_setCaption(1);
	SetCursor(NULL);
	InvalidateRect(_hdlg, NULL, FALSE);
}

// asks IDM_DECODE to open the photo at path.
void MainDlg::_openFile(const std::wstring &path)
{
	// use the full path. finding the next photo needs the folder, and the command line may have given a relative path.
	DWORD cc = GetFullPathName(path.c_str(), 0, NULL, NULL);
	if (cc)
	{
		std::wstring full(cc, 0);
		cc = GetFullPathName(path.c_str(), cc, &full[0], NULL);
		full.resize(cc);
		_nextFilename = full;
	}
	else
		_nextFilename = path;
	PostMessage(_hdlg, WM_COMMAND, MAKEWPARAM(IDM_DECODE, 0), 0);
}

// lists the heic photos in folder, sorted the way Explorer sorts file names.
static void _listPhotos(const std::wstring &folder, std::vector<std::wstring> &names)
{
	WIN32_FIND_DATA fd;
	HANDLE hf = FindFirstFileEx((folder + L"*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
	if (hf == INVALID_HANDLE_VALUE)
		return;
	do
	{
		if (fd.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_HIDDEN))
			continue;
		LPCWSTR extension = wcsrchr(fd.cFileName, '.');
		if (extension && (0 == _wcsicmp(extension, L".heic") || 0 == _wcsicmp(extension, L".heif")))
			names.push_back(fd.cFileName);
	} while (FindNextFile(hf, &fd));
	FindClose(hf);
	std::sort(names.begin(), names.end(), [](const std::wstring &a, const std::wstring &b) { return StrCmpLogicalW(a.c_str(), b.c_str()) < 0; });
}

// finds the position of the current photo in its folder for the caption.
void MainDlg::_updatePhotoPosition()
{
	_photoIndex = _photoCount = 0;
	LPCWSTR title = GetFileTitlePtr(_filename.c_str());
	std::vector<std::wstring> names;
	_listPhotos(std::wstring(_filename.c_str(), title - _filename.c_str()), names);
	for (size_t i = 0; i < names.size(); i++)
	{
		if (0 == _wcsicmp(names[i].c_str(), title))
			_photoIndex = (int)i + 1;
	}
	_photoCount = (int)names.size();
}

// opens the photo step positions after (or, if step is negative, before) the current one in the same folder.
void MainDlg::_openSibling(int step)
{
	if (_filename.empty())
		return;
	// holding an arrow key queues key repeats while a photo decodes. fold them into this step, so that the viewer stops flipping when the key is released.
	MSG msg;
	while (PeekMessage(&msg, _hdlg, WM_KEYDOWN, WM_KEYDOWN, PM_NOREMOVE) && (msg.wParam == VK_LEFT || msg.wParam == VK_RIGHT))
	{
		PeekMessage(&msg, _hdlg, WM_KEYDOWN, WM_KEYDOWN, PM_REMOVE);
		step += (msg.wParam == VK_RIGHT) ? 1 : -1;
	}
	if (step == 0)
		return;
	// list the folder again. photos may have been added, renamed or deleted since the last look.
	LPCWSTR title = GetFileTitlePtr(_filename.c_str());
	std::wstring folder(_filename.c_str(), title - _filename.c_str());
	std::vector<std::wstring> names;
	_listPhotos(folder, names);
	// find where the current photo sorts. this works even if it is no longer in the folder.
	int count = (int)names.size();
	int i = 0;
	while (i < count && StrCmpLogicalW(names[i].c_str(), title) < 0)
		i++;
	bool found = i < count && 0 == _wcsicmp(names[i].c_str(), title);
	// if the current photo is gone, names[i] is the photo after it. one step forward is then names[i] itself.
	int target = i + step - ((!found && step > 0) ? 1 : 0);
	if (target < 0)
		target = 0;
	if (target >= count)
		target = count - 1;
	if (target < 0 || (found && target == i))
	{
		// already at the first or the last photo.
		MessageBeep(MB_OK);
		return;
	}
	_openFile(folder + names[target]);
}

BOOL MainDlg::OnCommand(WPARAM wp_, LPARAM lp_)
{
	switch ((LOWORD(wp_)))
	{
	case IDM_DECODE:
		if (lp_ == 0)
		{
			HeifImage *h = new HeifImage();
			if (h->init(_hdlg, _nextFilename.c_str(), _heifFlags))
			{
				if (_heif)
					delete _heif;
				_heif = h;
				_filename = _nextFilename;
				_updatePhotoPosition();
				_autoZoom(true);
				_setCaption();
				PostMessage(_hdlg, WM_COMMAND, MAKEWPARAM(IDM_DECODE, 0), 1);
			}
			else
			{
				delete h;
				// init has reported the error. quit if this was the first photo. otherwise, show the name of the photo that failed on an empty page, so that the arrow keys can move past it.
				if (_filename.empty())
				{
					DestroyWindow(_hdlg);
					return TRUE;
				}
				if (_heif)
				{
					delete _heif;
					_heif = NULL;
				}
				if (_hbmpImage)
				{
					DeleteObject(_hbmpImage);
					_hbmpImage = NULL;
				}
				_filename = _nextFilename;
				_vi.szImage = { 0 };
				_vi.zoomRatio = 0;
				_layoutPage();
				_updatePhotoPosition();
				_setCaption();
				InvalidateRect(_hdlg, NULL, FALSE);
			}
		}
		else if (lp_ == 1 && _heif)
		{
			SetCursor(LoadCursor(NULL, IDC_WAIT));
			if (_hbmpImage)
				DeleteObject(_hbmpImage);
			_hbmpImage = _heif->decode(_vi.szImage);
			// the decoder reports the extents of the bitmap it actually made.
			_vi.szImage = _heif->_data.dims;
			_layoutPage();
			SetCursor(NULL);
			InvalidateRect(_hdlg, NULL, FALSE);
		}
		return TRUE;
	case IDM_OPEN:
		OnButtonOpen();
		return TRUE;
	case IDM_SAVEAS:
		OnButtonSaveAs();
		return TRUE;
	case IDM_PRINT:
		OnButtonPrint();
		return TRUE;
	case IDM_EXIT:
		DestroyWindow(_hdlg);
		return TRUE;

	case IDM_ZOOM_IN:
		_stepZoom(1, _pageCenter());
		return TRUE;
	case IDM_ZOOM_OUT:
		_stepZoom(-1, _pageCenter());
		return TRUE;
	case IDM_ZOOM_FIT:
		if (_heif)
			_setZoom(_fitZoom(), _pageCenter());
		return TRUE;
	case IDM_ZOOM_100:
		_setZoom(100, _pageCenter());
		return TRUE;
	case IDM_ROTATE:
		OnButtonRotate();
		return TRUE;
	case IDM_PREV:
		_openSibling(-1);
		return TRUE;
	case IDM_NEXT:
		_openSibling(1);
		return TRUE;

	case IDM_ABOUT:
		{
		AboutDlg dlg(_hdlg);
		dlg.DoModal();
		return TRUE;
		}
	}
	return SimpleDlg::OnCommand(wp_, lp_);
}

void MainDlg::OnButtonRotate()
{
	if (!_heif || !_hbmpImage)
		return;
	// put up a wait cursor.
	SetCursor(LoadCursor(NULL, IDC_WAIT));
	// free the old bitmap.
	if (_hbmpImage)
		DeleteObject(_hbmpImage);
	// get the heic image rotated by 45 degrees.
	_hbmpImage = _heif->rotate(_vi.szImage);
	// update the window size so that the rotated image fits.
	// note that szAuto has optimal viewport extents. it's the reference size we go back to after each operation of zoom or rotation, although the cx and cy may have to be swapped. szImage is the actual image extents after the rotation. if the orientations of the two sets of extents differ, swap the extents of szAuto to correct the orientation of the reference size.
	_vi.szPage = _vi.szImage;
	SIZE szRef = _vi.szAuto;
	if (_vi.szAuto.cy > _vi.szAuto.cx && _vi.szImage.cy < _vi.szImage.cx)
		std::swap(szRef.cx, szRef.cy);
	// szPage has the extents of the displayable portion of the image. if the rotated image exceeds the reference extent, limit the displayable to it.
	if (_vi.szPage.cx > szRef.cx)
		_vi.szPage.cx = szRef.cx;
	if (_vi.szPage.cy > szRef.cy)
		_vi.szPage.cy = szRef.cy;
	// ptPage.x is the distance from the left endpoint of the full image width and the starting point of the displayable width. it says how much of the image is pushed to the left of the viewport and so is hidden from view.
	_vi.ptPage.x = (_vi.szPage.cx - _vi.szImage.cx) / 2;
	_vi.ptPage.y = (_vi.szPage.cy - _vi.szImage.cy) / 2;
	// correct the window size. this also lays out the page and shows the scrollers if needed.
	_sizeWindow(_vi.szPage);
	// print the new rotation angle next to the file name on the caption bar.
	_setCaption();
	// take out the wait cursor.
	SetCursor(NULL);
	// paint the rotated image in the updated viewport.
	InvalidateRect(_hdlg, NULL, FALSE);
}

void MainDlg::OnSize(UINT nType, int cx, int cy)
{
	if (nType == SIZE_MINIMIZED)
		return;
	// keep the changed frame size.
	_vi.szPage.cx = cx-2* PHOTOMARGIN_CX;
	_vi.szPage.cy = cy-2* PHOTOMARGIN_CY;
	_layoutPage();
}

BOOL MainDlg::OnMouseWheel(short nButtonType, short nWheelDelta, POINT pt)
{
	// the cursor position in pt is in screen coordinates. so, translate to dialog's client coords.
	ScreenToClient(_hdlg, &pt);
	if (nButtonType & MK_CONTROL)
	{
		// ctrl+wheel zooms around the cursor. take in the wheel messages already queued, so that a fast spin rescales the image only once.
		int delta = nWheelDelta;
		MSG msg;
		while (PeekMessage(&msg, _hdlg, WM_MOUSEWHEEL, WM_MOUSEWHEEL, PM_NOREMOVE) && (GET_KEYSTATE_WPARAM(msg.wParam) & MK_CONTROL))
		{
			PeekMessage(&msg, _hdlg, WM_MOUSEWHEEL, WM_MOUSEWHEEL, PM_REMOVE);
			delta += GET_WHEEL_DELTA_WPARAM(msg.wParam);
		}
		_wheelDelta += delta;
		int steps = _wheelDelta / WHEEL_DELTA;
		_wheelDelta -= steps * WHEEL_DELTA;
		if (steps)
			_stepZoom(steps, pt);
		return TRUE;
	}
	if (_vi.onMouseWheel(nButtonType, nWheelDelta, pt))
		InvalidateRect(_hdlg, NULL, FALSE);
	return TRUE;
}

BOOL MainDlg::OnMouseMove(UINT state, POINT pt)
{
	// report the mouse event to the viewinfo so it can make the scrollbars visible.
	_vi.onMouseMove(_hdlg, state, pt);
	return FALSE;
}

BOOL MainDlg::OnLButtonDown(INT_PTR nButtonType, POINT pt)
{
	// if a sustained click-down occurs on a scrollbar, prepare to scroll the image. then, change the cursor shape to a hand with a pointing fingure.
	if (_vi.startScroll(_hdlg, (UINT)nButtonType, pt))
		SetCursor(LoadCursor(NULL, IDC_HAND));
	return TRUE;
}

BOOL MainDlg::OnLButtonUp(INT_PTR nButtonType, POINT pt)
{
	// the click is over. stop the scrolling. switch back to the standard cursor.
	if (_vi.stopScroll(_hdlg, (UINT)nButtonType, pt))
		SetCursor(NULL);
	return TRUE;
}

BOOL MainDlg::OnPaint()
{
	// to render the heic image, we bitblt the decoded bitmap on to the screen device context.
	PAINTSTRUCT	ps;
	BeginPaint(_hdlg, (LPPAINTSTRUCT)&ps);

	// pt is the origin, the upper-left corner of the image.
	POINT pt = _vi.ptPage;
#if PHOTOMARGIN_CX > 0
	// paint the margin strips in white that surround the image.
	//TODO: if the image has been zoomed in, some or all of the margin strips may fall outside the viewport. they should not be painted at all.
	RECT rc = { pt.x, pt.y, pt.x + 2 * PHOTOMARGIN_CX + _vi.szImage.cx, pt.y + PHOTOMARGIN_CY };
	FillRect(ps.hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
	rc = { pt.x, pt.y, pt.x + PHOTOMARGIN_CX, pt.y + 2 * PHOTOMARGIN_CY + _vi.szImage.cy };
	FillRect(ps.hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
	rc = { pt.x + PHOTOMARGIN_CX+_vi.szImage.cx, pt.y, pt.x + 2 * PHOTOMARGIN_CX + _vi.szImage.cx, pt.y + 2 * PHOTOMARGIN_CY + _vi.szImage.cy };
	FillRect(ps.hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
	rc = { pt.x, pt.y + PHOTOMARGIN_CY + _vi.szImage.cy, pt.x + 2 * PHOTOMARGIN_CX + _vi.szImage.cx, pt.y + 2 * PHOTOMARGIN_CY + _vi.szImage.cy };
	FillRect(ps.hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
#endif//#if PHOTOMARGIN_CX > 0
	RECT rcImage = { 0 };
	if (_hbmpImage)
	{
		BITMAP bm;
		GetObject(_hbmpImage, sizeof(bm), &bm);
#if PHOTOMARGIN_CX > 0
		pt.x += PHOTOMARGIN_CX;
		pt.y += PHOTOMARGIN_CY;
#endif//PHOTOMARGIN_CX
		HDC hdc2 = CreateCompatibleDC(ps.hdc);
		HBITMAP hbmp0 = (HBITMAP)SelectObject(hdc2, _hbmpImage);
		BitBlt(ps.hdc, pt.x, pt.y, bm.bmWidth, bm.bmHeight, hdc2, 0, 0, SRCCOPY);
		SelectObject(hdc2, hbmp0);
		DeleteDC(hdc2);
		rcImage = { pt.x, pt.y, pt.x + bm.bmWidth, pt.y + bm.bmHeight };
	}
	// paint the part of the window the image does not cover. we skip the background erase, so it would show leftovers otherwise.
	HRGN rgnBack = CreateRectRgnIndirect(&ps.rcPaint);
	HRGN rgnImage = CreateRectRgnIndirect(&rcImage);
	CombineRgn(rgnBack, rgnBack, rgnImage, RGN_DIFF);
	FillRgn(ps.hdc, rgnBack, _hbrBack);
	DeleteObject(rgnImage);
	DeleteObject(rgnBack);
	// overlay the scrollbars if they should be visible.
	if (_hbmpImage)
		_vi.drawScrollers(ps.hdc, SCROLLER_MORE_TRANSPARENT);

	EndPaint(_hdlg, (LPPAINTSTRUCT)&ps);
	return TRUE;
}

BOOL MainDlg::OnEraseBkgnd(HDC hdc)
{
	// return a true to report to the system that we've handled the background erase. it's to prevent the system from doing an actual erase. it would cause a bad screen flicker.
	return TRUE;
}

void MainDlg::OnButtonOpen()
{
	resstring filter(AppInstanceHandle, IDS_OPEN_FILTER);
	WCHAR path[_MAX_PATH] = { 0 };
	OPENFILENAME ofn = { sizeof(OPENFILENAME), 0 };
	ofn.hwndOwner = _hdlg;
	ofn.Flags = OFN_FILEMUSTEXIST;
	ofn.lpstrFilter = filter;
	ofn.lpstrDefExt = L".heic";
	ofn.lpstrFile = path;
	ofn.nMaxFile = ARRAYSIZE(path);
	if (!GetOpenFileName(&ofn))
	{
		DWORD cderr = CommDlgExtendedError(); // CDERR_DIALOGFAILURE, etc.
		if (cderr != 0) // 0 if user has canceled.
			MessageBox(_hdlg, resstring(resstring(AppInstanceHandle, IDS_CDERR), cderr), resstring(AppInstanceHandle, IDS_APP_TITLE), MB_OK | MB_ICONEXCLAMATION);
		// user canceled. if no photo has been opened yet, there is nothing to show. so, we're shutting down.
		if (_filename.empty())
			PostMessage(_hdlg, WM_CLOSE, 0, 0);
		return;
	}
	// we have a hiec image. issue a command to decode and display it.
	_openFile(path);
}

int MainDlg::_queryFileTitle(LPWSTR buf, int bufSize)
{
	if (_filename.empty())
		return -1;
	return CopyFileTitle(_filename.c_str(), buf, bufSize);
}

void MainDlg::OnButtonSaveAs()
{
	if (!_heif)
		return;
	WCHAR path[_MAX_PATH] = { 0 };
	_queryFileTitle(path, ARRAYSIZE(path));

	resstring filter(AppInstanceHandle, IDS_SAVEAS_FILTER);
	OPENFILENAME ofn = { sizeof(OPENFILENAME), 0 };
	ofn.hwndOwner = _hdlg;
	ofn.Flags = OFN_PATHMUSTEXIST;
	ofn.lpstrFilter = filter;
	ofn.lpstrDefExt = L".jpg";
	ofn.lpstrFileTitle;
	ofn.lpstrFile = path;
	ofn.nMaxFile = ARRAYSIZE(path);
	if (!GetSaveFileName(&ofn))
	{
		DWORD cderr = CommDlgExtendedError(); // CDERR_DIALOGFAILURE, etc.
		if (cderr != 0) // 0 if user has canceled.
			MessageBox(_hdlg, resstring(resstring(AppInstanceHandle, IDS_CDERR), cderr), resstring(AppInstanceHandle, IDS_APP_TITLE), MB_OK | MB_ICONEXCLAMATION);
		return;
	}
	// we have a destination filename. find the destination image encoding type based on the file extension name. then, we will convert the heic to it and save the converted image in the destination file.
	SetCursor(LoadCursor(NULL, IDC_WAIT));
	// if the current hief object holds a full-scale bitmap, we will use it and make the conversion in one quick step.
	if (_heif && _heif->hasFullscaleData())
	{
		// we have a full-scale image. just need to run the conversion.
		_heif->convert(path);
	}
	else
	{
		// the current hief object is not at full scale. we need to create a temp heif and decode the image at full scale from scratch.
		HeifImage heif2;
		if (heif2.init(_hdlg, _filename.c_str(), _heifFlags))
		{
			HBITMAP hbmp = heif2.decode({ 0 });
			if (hbmp)
			{
				//TODO: we don't need a win32 HBITMAP generated by the decoder in the first place.
				DeleteObject(hbmp);
				// we have a decoded bitmap. encode it in the destination format and save it it the file at path.
				heif2.convert(path);
			}
		}
	}
	SetCursor(NULL);
}


/* _setupDevMode
prepares a DEVMODE structure for a custom printer configuration.
currently, only paper orientation is supported.
*/
HGLOBAL _setupDevMode(short paperOrientation)
{
	HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE| GMEM_ZEROINIT, sizeof(DEVMODE));
	DEVMODE* pdm = (DEVMODE*)GlobalLock(hg);
	pdm->dmSize = sizeof(DEVMODE);
	pdm->dmFields |= DM_ORIENTATION;
	pdm->dmOrientation = paperOrientation; // e.g., DMORIENT_LANDSCAPE
	GlobalUnlock(hg);
	return hg;
}

/* _queryPrintDimensions
calculates paper margins and printed extents of an input bitmap image that are appropriate for a printer device context.
*/
int _queryPrintDimensions(PRINTDLGEX *pdx, HeifImage *heif, POINT &printMargin, SIZE &printExtents)
{
	// printer dpi (typically 600)
	SIZE devDpi = { GetDeviceCaps(pdx->hDC, LOGPIXELSX), GetDeviceCaps(pdx->hDC, LOGPIXELSY) };
	// width and height resolutions in pixels
	SIZE devRes = { GetDeviceCaps(pdx->hDC, HORZRES), GetDeviceCaps(pdx->hDC, VERTRES) };
	// dimensions of the paper, translated to 1/10000"
	SIZE paperExts = { MulDiv(10000,devRes.cx,devDpi.cx),MulDiv(10000,devRes.cy,devDpi.cy) };

#ifdef _DEBUG
	short orientation_ = 0;
	short paperSize_ = 0;
	short paperWidth_ = 0;
	short paperLength_ = 0;
	short scale_ = 0;
	if (pdx->hDevMode)
	{
		DEVMODE *pdm = (DEVMODE*)GlobalLock(pdx->hDevMode);
		if (pdm->dmFields & DM_YRESOLUTION)
		{
			devDpi.cx = pdm->dmPrintQuality; // x resolution
			devDpi.cy = pdm->dmYResolution; // y resolution
		}
		if (pdm->dmFields & DM_ORIENTATION)
			orientation_ = pdm->dmOrientation; // DMORIENT_PORTRAIT (1), DMORIENT_LANDSCAPE (2)
		if (pdm->dmFields & DM_PAPERSIZE)
			paperSize_ = pdm->dmPaperSize; // DMPAPER_LETTER (1), DMPAPER_A4 (9)
		if (pdm->dmFields & DM_PAPERWIDTH)
			paperWidth_ = pdm->dmPaperWidth; // width of the paper in 1/10 mm
		if (pdm->dmFields & DM_PAPERLENGTH)
			paperLength_ = pdm->dmPaperLength; // length of the paper in 1/10 mm
		if (pdm->dmFields & DM_SCALE)
			scale_ = pdm->dmScale; // scaling factor in percents
		GlobalUnlock(pdx->hDevMode);
	}
#endif//#ifdef _DEBUG

	int imageRes = heif->queryDPI();
	SIZE desiredExts;
	desiredExts.cx = MulDiv(10000, heif->_imageSize.cx, imageRes);
	desiredExts.cy = MulDiv(10000, heif->_imageSize.cy, imageRes);
	SIZE actualExts = desiredExts;
	if (desiredExts.cx > paperExts.cx || desiredExts.cy > paperExts.cy)
	{
		// the image width or height or both exceeds the corresponding paper dimension.
		// auto-scale the printed image. we are ignoring scale_.
		double rx = (double)desiredExts.cx / (double)paperExts.cx;
		double ry = (double)desiredExts.cy / (double)paperExts.cy;

		if (rx > 1.0 && ry > 1.0)
		{
			if (rx > ry)
			{
				ASSERT(FALSE);
				actualExts.cx = paperExts.cx;
				actualExts.cy = MulDiv(paperExts.cx, desiredExts.cy, desiredExts.cx);
			}
			else if (rx < ry)
			{
				ASSERT(FALSE);
				actualExts.cy = paperExts.cy;
				actualExts.cx = MulDiv(paperExts.cy, desiredExts.cx, desiredExts.cy);
			}
			else
			{
				actualExts.cx = paperExts.cx;
				actualExts.cy = paperExts.cy;
			}
		}
		else if (rx > 1.0)
		{
			actualExts.cx = paperExts.cx;
			actualExts.cy = MulDiv(paperExts.cx, desiredExts.cy, desiredExts.cx);
		}
		else if (ry > 1.0)
		{
			actualExts.cy = paperExts.cy;
			actualExts.cx = MulDiv(paperExts.cy, desiredExts.cx, desiredExts.cy);
		}
	}
	// determine the margins. center the image. take the difference between the paper width and image width. halve it to get the amount of margin that works for both left and right.
	POINT marginInchPt = { (paperExts.cx - actualExts.cx)/2, (paperExts.cy - actualExts.cy)/2 };
	// convert the width and height in inches to numbers of device pixels.
	printExtents.cx = MulDiv(devRes.cx, actualExts.cx, paperExts.cx);
	printExtents.cy = MulDiv(devRes.cy, actualExts.cy, paperExts.cy);
	printMargin.x = MulDiv(devRes.cx, marginInchPt.x, paperExts.cx);
	printMargin.y = MulDiv(devRes.cy, marginInchPt.y, paperExts.cy);
	return imageRes;
}

DWORD _printImage(PRINTDLGEX *pdx, HBITMAP _hbmpImage, HeifImage *heif)
{
	HDC hdc = pdx->hDC;
	DWORD ret = ERROR_SUCCESS;
	int cap = GetDeviceCaps(hdc, RC_BITBLT); //RC_STRETCHBLT

	POINT printMargin;
	SIZE printExtents;
	int imageRes = _queryPrintDimensions(pdx, heif, printMargin, printExtents);

	// map logical (screen) coordinates in pels to printer (device) coordinates in 1/100mm.
	SetMapMode(hdc, MM_ISOTROPIC);
	// logical origin and extents in ppi
	SetWindowOrgEx(hdc, 0, 0, NULL);
	SetWindowExtEx(hdc, heif->_imageSize.cx, heif->_imageSize.cy, NULL);
	// device origin and extents in device dpi
	SetViewportOrgEx(hdc, printMargin.x, printMargin.y, NULL);
	SetViewportExtEx(hdc, printExtents.cx, printExtents.cy, NULL);

	HDC hdc2 = CreateCompatibleDC(hdc);
	HBITMAP hbmp0 = (HBITMAP)SelectObject(hdc2, _hbmpImage);
	if (!BitBlt(hdc, 0, 0, heif->_imageSize.cx, heif->_imageSize.cy, hdc2, 0, 0, SRCCOPY))
		ret = GetLastError();
	SelectObject(hdc2, hbmp0);
	DeleteDC(hdc2);
	return ret;
}


#define HEICPRINT_MAX_RANGES 1

void MainDlg::OnButtonPrint()
{
	if (!_heif)
		return;
	HRESULT hr;
	PRINTDLGEX *pdx = (PRINTDLGEX*)GlobalAlloc(LPTR, sizeof(PRINTDLGEX));
	pdx->lStructSize = sizeof(PRINTDLGEX);
	pdx->hwndOwner = _hdlg;
	// we make one customization. it's for the paper orientation. if the image is wide, ask for landscape mode.
	if (_heif->_imageSize.cx > _heif->_imageSize.cy)
		pdx->hDevMode = _setupDevMode(DMORIENT_LANDSCAPE);
	pdx->hDC = NULL;
	pdx->Flags = PD_RETURNDC | PD_COLLATE | PD_CURRENTPAGE | PD_NOSELECTION;
	pdx->nMaxPageRanges = HEICPRINT_MAX_RANGES;
	pdx->lpPageRanges = (LPPRINTPAGERANGE)GlobalAlloc(LPTR, HEICPRINT_MAX_RANGES * sizeof(PRINTPAGERANGE));
	pdx->nMinPage = 1;
	pdx->nMaxPage = 1;
	pdx->nCopies = 1;
	pdx->nStartPage = START_PAGE_GENERAL;

	//  run the printer selection dialog.
	hr = PrintDlgEx(pdx);
	if ((hr == S_OK) && pdx->dwResultAction == PD_RESULT_PRINT)
	{
		SetCursor(LoadCursor(NULL, IDC_WAIT));
		// we need to obtain an HBITMAP of the full-scale image. if the current one, _heif, is not that, we will create a temp heif and run a full-scale decode.
		bool delHeif = false;
		HeifImage *heif = _heif;
		HBITMAP hbmp = _hbmpImage;
		if (!_heif->hasFullscaleData())
		{
			heif = new HeifImage;
			if (heif->init(_hdlg, _filename.c_str(), _heifFlags))
			{
				// decode the image at a full scale
				hbmp = heif->decode({ 0 });
			}
			delHeif = true;
		}
		if (hbmp)
		{
			// we now have the decoded bitmap. let's start the print job.
			DOCINFO docInfo = { sizeof(DOCINFO) };
			int docId = ::StartDoc(pdx->hDC, &docInfo);
			if (docId > 0)
			{
				if (::StartPage(pdx->hDC))
				{
					// print the image to the destination medium.
					_printImage(pdx, hbmp, heif);
					::EndPage(pdx->hDC);
				}
				::EndDoc(pdx->hDC);
			}
		}
		// if we used a temp heif decoder, delete it.
		if (delHeif)
		{
			if (hbmp)
				DeleteObject(hbmp);
			delete heif;
		}
		SetCursor(NULL);
	}
	// clean up. we're done.
	if (pdx->hDC != NULL)
		DeleteDC(pdx->hDC);
	if (pdx->lpPageRanges)
		GlobalFree((HGLOBAL)pdx->lpPageRanges);
	if (pdx->hDevMode)
		GlobalFree(pdx->hDevMode);
	if (pdx->hDevNames)
		GlobalFree(pdx->hDevNames);
	GlobalFree((HGLOBAL)pdx);
}

