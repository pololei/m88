// ---------------------------------------------------------------------------
//  M88 - PC8801 emulator
//  Copyright (C) cisc 1998, 2000.
// ---------------------------------------------------------------------------
//	DirectDraw �ɂ��S��ʕ`��
// ---------------------------------------------------------------------------
//	$Id: DrawDDS.cpp,v 1.16 2003/11/04 13:14:21 cisc Exp $

#include "headers.h"
#include "drawdds.h"
#include "misc.h"
#include "messages.h"

#define LOGNAME "drawdds"
#include "diag.h"
#include "dderr.h"

#define RELCOM(x)  if (x) x->Release(), x=0; else 0

// ---------------------------------------------------------------------------
//	�\�z
//
WinDrawDDS::WinDrawDDS(bool force480)
{
	ddraw = 0;
	ddsscrn = 0;
	ddcscrn = 0;
	ddpal = 0;
	palchanged = false;
	guimode = true;
	lines = force480 ? 480 : 0;
	image = 0;
}

// ---------------------------------------------------------------------------
//	�j��
//
WinDrawDDS::~WinDrawDDS()
{
	Cleanup();
}

// ---------------------------------------------------------------------------
//	������
//
bool WinDrawDDS::Init(HWND hwindow, uint w, uint h, GUID* drv)
{
	HRESULT hr;
	hwnd = hwindow;

	width = w;
	height = h;

	if (!CreateDD2(drv)) return false;

	hr = ddraw->SetCooperativeLevel(hwnd, 
		DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT);
	LOGDDERR("DirectDraw::SetCooperativeLevel()", hr);
	if (hr != DD_OK)
		return false;

	if (!SetScreenMode()) return false;

	if (!CreateDDS()) return false;

	CreateDDPalette();

	guimode = true;
	SetGUIMode(false);
	SendMessage(hwnd, WM_M88_CLIPCURSOR, CLIPCURSOR_WINDOW, 0);
	status |= Draw::shouldrefresh;
	
	return true;
}

// ---------------------------------------------------------------------------
//	Cleanup
//
bool WinDrawDDS::Cleanup()
{
	if (ddraw)
	{
		ddraw->SetCooperativeLevel(hwnd, DDSCL_NORMAL);
	}
	
	RELCOM(ddpal);
	RELCOM(ddcscrn);
	RELCOM(ddsscrn);
	RELCOM(ddraw);
	
	SendMessage(hwnd, WM_M88_CLIPCURSOR, -CLIPCURSOR_WINDOW, 0);

	delete[] image;
	return true;
}

// ---------------------------------------------------------------------------
//	DirectDraw2 ����
//
bool WinDrawDDS::CreateDD2(GUID* drv)
{
	if (FAILED(CoCreateInstance(CLSID_DirectDraw, 0, CLSCTX_ALL, IID_IDirectDraw2, (void**) &ddraw)))
		return false;
	if (FAILED(ddraw->Initialize(NULL)))//drv)))
		return false;
	return true;
}

// ---------------------------------------------------------------------------
//	Surface ����
//
bool WinDrawDDS::CreateDDS()
{
	HRESULT hr;
	
	// �\���T�[�t�F�X���쐬
	DDSURFACEDESC ddsd;
	memset(&ddsd, 0, sizeof(DDSURFACEDESC));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
	ddsd.dwBackBufferCount = 1;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_COMPLEX | DDSCAPS_FLIP;

	hr = ddraw->CreateSurface(&ddsd, &ddsscrn, 0);
	LOGDDERR("ddraw->CreateSurface(primary)", hr);
	if (DD_OK != hr)
		return false;

	DDSCAPS ddsc;
	ddsc.dwCaps = DDSCAPS_BACKBUFFER;
	ddsscrn->GetAttachedSurface(&ddsc, &ddsback);
	
	// �N���b�p�[���쐬
	hr = ddraw->CreateClipper(0, &ddcscrn, 0);
	LOGDDERR("ddraw->CreateClipper", hr);
	if (DD_OK != hr)
		return false;

	ddcscrn->SetHWnd(0, hwnd);

	return true;
}

// ---------------------------------------------------------------------------
//	�p���b�g����
//
bool WinDrawDDS::CreateDDPalette()
{
	HDC hdc = GetDC(hwnd);
	GetSystemPaletteEntries(hdc, 0, 256, palentry);
	ReleaseDC(hwnd, hdc);

	HRESULT hr = ddraw->CreatePalette(DDPCAPS_8BIT, palentry, &ddpal, 0);
	LOGDDERR("ddraw->CreatePalette", hr);
	if (DD_OK == hr)
	{
		ddsscrn->SetPalette(ddpal);
	}
	return true;
}

// ---------------------------------------------------------------------------
//	�`��
//	flip �� Flip() �ōs���̂��؂��Ƃ��������ǁCDrawScreen �ƃ��C��������
//	�ʃX���b�h�Ȃ̂ł������� flip ����
//
void WinDrawDDS::DrawScreen(const RECT& _rect, bool refresh)
{
	RECT rect = _rect;

	if (refresh)
	{
		rect.left = 0, rect.right = width;
		rect.top = 0, rect.bottom = height;
		FillBlankArea();
	}

	// ��Ɨ̈���X�V
	HRESULT r = ddsscrn->IsLost();
	if (r == DD_OK)
	{
		if (!guimode)
		{
			Update(ddsscrn, rect);
		}
		else
		{
			if (Update(ddsback, rect))
			{
				RECT rectdest;
				rectdest.left	= ltc.x + rect.left;
				rectdest.right	= ltc.x + rect.right;
				rectdest.top	= ltc.y + rect.top;
				rectdest.bottom = ltc.y + rect.bottom;
				r = ddsscrn->Blt(&rectdest, ddsback, &rectdest, 0, 0);
				LOGDDERR("ddsscrn->Blt", r);
			}
		}
		
		if (palchanged)
		{
			palchanged = false;
			ddpal->SetEntries(0, 0, 0x100, palentry);
		}
	}
	if (r == DDERR_SURFACELOST)
	{
		RestoreSurface();
	}
}

bool WinDrawDDS::Update(LPDIRECTDRAWSURFACE dds, const RECT& rect)
{
	if (rect.left >= rect.right || rect.top >= rect.bottom)
		return false;

	HRESULT r;
	
	DDSURFACEDESC ddsd;
	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	
	RECT rectdest;
	rectdest.left	= ltc.x + rect.left;
	rectdest.right	= ltc.x + rect.right;
	rectdest.top	= ltc.y + rect.top;
	rectdest.bottom = ltc.y + rect.bottom;

	r = dds->Lock(&rectdest, &ddsd, DDLOCK_WRITEONLY, 0);
	LOGDDERR("ddsk->Lock", r);
	if (r != DD_OK)
		return false;
	
	const uint8* src = image + rect.left + rect.top * width;
	uint8* dest = (uint8*) ddsd.lpSurface;
	
	for (int y = rect.top; y < rect.bottom; y++)
	{
		memcpy(dest, src, rect.right - rect.left);
		dest += ddsd.lPitch;
		src  += width;
	}

	dds->Unlock(ddsd.lpSurface);
	return true;
}

void WinDrawDDS::Flip()
{
}

// ---------------------------------------------------------------------------
//	WM_QUERYNEWPALETTE
//
void WinDrawDDS::QueryNewPalette()
{
}

// ---------------------------------------------------------------------------
//	�p���b�g��ݒ�
//
void WinDrawDDS::SetPalette(PALETTEENTRY* pe, int idx, int ent)
{
	for (; ent>0; ent--)
	{
		palentry[idx].peRed = pe->peRed;
		palentry[idx].peBlue = pe->peBlue;
		palentry[idx].peGreen = pe->peGreen;
		palentry[idx].peFlags = PC_RESERVED | PC_NOCOLLAPSE;
		idx++, pe++;
	}
	palchanged = true;
}

// ---------------------------------------------------------------------------
//	��ʃC���[�W�̎g�p�v��
//
bool WinDrawDDS::Lock(uint8** pimage, int* pbpl)
{
	*pimage = image;
	*pbpl   = width;
	return true;
}

// ---------------------------------------------------------------------------
//	��ʃC���[�W�̎g�p�I��
//
bool WinDrawDDS::Unlock()
{
	status &= ~Draw::shouldrefresh;
	return true;
}

// ---------------------------------------------------------------------------
//	��ʃ��[�h��؂�ւ���
//
bool WinDrawDDS::SetScreenMode()
{
	HRESULT hr;
	DDSURFACEDESC ddsd;
	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_WIDTH | DDSD_REFRESHRATE;
	ddsd.dwWidth = width;
	ddsd.dwRefreshRate = 0;
	
	if (!lines)
	{
		hr = ddraw->EnumDisplayModes(0, &ddsd, reinterpret_cast<void*>(this), EDMCallBack);
		LOGDDERR("ddraw->EnumDisplayModes", hr);
		if (DD_OK != hr)
			return false;
	}
	if (!lines)
		lines = 480;

	hr = ddraw->SetDisplayMode(width, lines, 8, 0, 0);
	LOGDDERR("ddraw->SetDisplayMode", hr);
	if (DD_OK != hr)
		return false;

	ltc.x = 0;
	ltc.y = Max(0, (lines - height) / 2);

	delete[] image;
	image = new uint8[height * width];
	if (!image)
		return false;

	return true;
}

HRESULT WINAPI WinDrawDDS::EDMCallBack(LPDDSURFACEDESC pddsd, LPVOID context)
{
	WinDrawDDS* wd = reinterpret_cast<WinDrawDDS*> (context);

	if (pddsd->ddpfPixelFormat.dwRGBBitCount == 8)
	{
		if (pddsd->dwHeight == wd->height)
		{
			wd->lines = pddsd->dwHeight;
			return DDENUMRET_CANCEL;
		}
		if (pddsd->dwHeight == 480 && !wd->lines)
		{
			wd->lines = 480;
		}
	}
	return DDENUMRET_OK;
}

// ---------------------------------------------------------------------------
//	���X�g�����T�[�t�F�X��߂�
//
bool WinDrawDDS::RestoreSurface()
{
	if (DD_OK != ddsscrn->Restore())
	{
		return false;
	}

	status |= Draw::shouldrefresh;
	FillBlankArea();
	return true;
}

// ---------------------------------------------------------------------------
//	��\���̈������
//
void WinDrawDDS::FillBlankArea()
{
	if (lines > height)
	{
		DDBLTFX ddbltfx;
		ddbltfx.dwSize = sizeof(ddbltfx);
		ddbltfx.dwFillColor = 0;
		
		RECT rect;
		rect.left = 0;		rect.top = 0;
		rect.right = width;	rect.bottom = ltc.y;
		ddsscrn->Blt(&rect, NULL, NULL, DDBLT_COLORFILL, &ddbltfx);
		
		rect.top = lines - ltc.y;
		rect.bottom = lines;
		ddsscrn->Blt(&rect, NULL, NULL, DDBLT_COLORFILL, &ddbltfx);
	}
}

// ---------------------------------------------------------------------------
//	GUI ���[�h�؂�ւ�
//
void WinDrawDDS::SetGUIMode(bool newguimode)
{
	if (newguimode != guimode)
	{
		guimode = newguimode;
		if (guimode)
		{
			ddraw->FlipToGDISurface();
			ddsscrn->SetClipper(ddcscrn);
			status |= Draw::shouldrefresh;
		}
		else
		{
			ddsscrn->SetClipper(0);

			FillBlankArea();

			RECT rectsrc;
			rectsrc.left = 0;		rectsrc.top = 0;
			rectsrc.right = width;	rectsrc.bottom = height;

			status |= Draw::shouldrefresh;
//			Update(rectsrc);
		}
	}
}

// ---------------------------------------------------------------------------
//	�\���̈��ݒ肷��
//
bool WinDrawDDS::Resize(uint w, uint h)
{
	height = h;
	return true;
}

// ---------------------------------------------------------------------------
//	�t���b�v���s�����ǂ����ݒ�
//
bool WinDrawDDS::SetFlipMode(bool f)
{
	return false;
}