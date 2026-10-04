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
#pragma once
#include "SimpleDlg.h"
#include "resource.h"
#include "HeifImage.h"


// flags for VIEWINFO.scrollH and scrollV
#define VI_SCROLLER_VISIBLE 0x0001
#define VI_SCROLLER_SHADED 0x0002
#define VI_SCROLLER_CAPTURED 0x0004

struct VIEWINFO
{
	POINT ptCapture;
	POINT ptPage;
	SIZE szPage;
	SIZE szImage;
	SIZE szAuto;
	SIZE szScreen;
	int zoomRatio;
	int zoomAuto;
	USHORT scrollH;
	USHORT scrollV;
	RECT rectH;
	RECT rectV;
	RECT rectCapture;

	VIEWINFO() : ptCapture{ 0 }, ptPage{ 0 }, szPage{ 0 }, szImage{ 0 }, szAuto{ 0 }, szScreen{ 0 }, rectH{ 0 }, rectV{ 0 }, zoomRatio(0), zoomAuto(0), scrollH(0), scrollV(0)
	{
		szScreen.cx = GetSystemMetrics(SM_CXFULLSCREEN);
		szScreen.cy = GetSystemMetrics(SM_CYFULLSCREEN);
	}

	void onMouseMove(HWND hwnd, UINT state, POINT pt);
	bool onMouseWheel(short nButtonType, short nWheelDelta, POINT pt);
	bool startScroll(HWND hwnd, UINT buttonType, POINT pt);
	bool stopScroll(HWND hwnd, UINT buttonType, POINT pt);

	void drawScrollerH(HDC hdc, BYTE transparency);
	void drawScrollerV(HDC hdc, BYTE transparency);
	BOOL drawScrollers(HDC hdc, BYTE transparency);
	BOOL blendRect(HDC hdc, int x, int y, int cx, int cy, BYTE transparency);
};


class MainDlg : public SimpleDlg
{
public:
	MainDlg(const std::wstring &path) : SimpleDlg(IDD_MAIN), _nextFilename(path), _heif(NULL), _hbmpImage(NULL), _heifFlags(0), _frameMargins{ 0 }, _hbrBack(NULL), _wheelDelta(0), _photoIndex(0), _photoCount(0),
		_fitMode(false), _userSized(false), _inSizeMove(false), _sizeBeforeMove{ 0 }, _sizingWindow(false), _panning(false), _panStart{ 0 }, _panOrigin{ 0 } {}

protected:
	VIEWINFO _vi;
	std::wstring _filename; // the photo being shown. empty until the first photo opens.
	std::wstring _nextFilename; // the photo IDM_DECODE should open.
	HBITMAP _hbmpImage;
	HeifImage *_heif;
	ULONG _heifFlags;
	SIZE _frameMargins;
	HBRUSH _hbrBack;
	int _wheelDelta; // ctrl+wheel rotation not yet turned into a zoom step. touchpads report less than a notch at a time.
	int _photoIndex, _photoCount; // 1-based position of the photo among the heic photos in its folder, and their number.
	bool _fitMode; // the zoom fits the photo in the window. resizing the window fits it again.
	bool _userSized; // the user has resized the window. keep that size for the next photos.
	bool _inSizeMove; // the user is dragging the window frame.
	SIZE _sizeBeforeMove; // the page size when the frame drag started.
	bool _sizingWindow; // _sizeWindow is resizing the window. the resulting WM_SIZE is not the user's doing.
	bool _panning; // the user is dragging the photo.
	POINT _panStart, _panOrigin; // cursor position and ptPage when the drag started.

	virtual BOOL OnInitDialog();
	virtual void OnDestroy();
	virtual BOOL OnOK();
	virtual BOOL OnCancel();
	virtual void OnSize(UINT nType, int cx, int cy);
	virtual BOOL OnGetMinMaxInfo(LPMINMAXINFO pMMI);
	virtual void OnEnterSizeMove();
	virtual void OnExitSizeMove();
	virtual BOOL OnLButtonDown(INT_PTR nButtonType, POINT pt);
	virtual BOOL OnLButtonUp(INT_PTR nButtonType, POINT pt);
	virtual BOOL OnMouseMove(UINT state, POINT pt);
	virtual BOOL OnMouseWheel(short nButtonType, short nWheelDelta, POINT pt);
	virtual BOOL OnCommand(WPARAM wp_, LPARAM lp_);
	virtual BOOL OnPaint();
	virtual BOOL OnEraseBkgnd(HDC hdc);

	void OnButtonOpen();
	void OnButtonSaveAs();
	void OnButtonPrint();
	void OnButtonRotate();

	void _openFile(const std::wstring &path);
	void _openSibling(int step);
	void _updatePhotoPosition();
	void _setZoom(int ratio, POINT anchor);
	void _refit();
	bool _keepWindowSize();
	void _stepZoom(int steps, POINT anchor);
	int _fitZoom();
	POINT _pageCenter();
	void _layoutPage();
	void _sizeWindow(SIZE imageSize);
	void _autoZoom(bool syncParams=false);
	void _setCaption(ULONG flags=0);
	int _queryFileTitle(LPWSTR buf, int bufSize);
};

