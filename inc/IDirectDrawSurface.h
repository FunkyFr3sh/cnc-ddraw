#ifndef IDIRECTDRAWSURFACE_H
#define IDIRECTDRAWSURFACE_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "ddraw.h"
#include "IDirectDrawPalette.h"
#include "IDirectDrawClipper.h"
#include "IDirectDraw.h"


typedef struct DDBITMAPINFO{
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD bmiColors[256];
} DDBITMAPINFO;

struct IDirectDrawSurfaceImpl;
struct IDirectDrawSurfaceImplVtbl;

typedef struct IDirectDrawSurfaceImpl
{
    struct IDirectDrawSurfaceImplVtbl* lpVtbl;

    ULONG ref;

    DWORD bpp;
    DWORD width;
    DWORD height;
    DWORD size;
    DWORD flags;
    DWORD caps;
    DWORD backbuffer_count;
    CRITICAL_SECTION cs;

    IDirectDrawPaletteImpl* palette;
    PALETTEENTRY selected_pal[256];
    UINT selected_pal_count;

    void* surface;
    HANDLE mapping;
    DWORD pitch;
    DWORD bytes_pp;
    BOOL custom_buf;

    PBITMAPINFO bmi;
    HBITMAP bitmap;
    HDC hdc;
    int dc_state;
    LONG lock_acquired;
    LONG dc_acquired;
    DDCOLORKEY color_key;
    DWORD last_flip_tick;
    DWORD last_blt_tick;
    BOOL queried;
    BOOL skip_flip; /* Quest for Glory 5 */

    struct IDirectDrawSurfaceImpl* backbuffer;
    struct IDirectDrawClipperImpl* clipper;
    struct IDirectDrawImpl* ddraw;

} IDirectDrawSurfaceImpl;

typedef struct IDirectDrawSurfaceImplVtbl IDirectDrawSurfaceImplVtbl;

#undef INTERFACE
#define INTERFACE IDirectDrawSurfaceImpl
struct IDirectDrawSurfaceImplVtbl
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface) (THIS_ REFIID riid, LPVOID FAR* ppvObj) PURE;
    STDMETHOD_(ULONG, AddRef) (THIS)  PURE;
    STDMETHOD_(ULONG, Release) (THIS) PURE;
    /*** IDirectDrawSurface methods ***/
    STDMETHOD(AddAttachedSurface)(THIS_ LPDIRECTDRAWSURFACE7) PURE;
    STDMETHOD(AddOverlayDirtyRect)(THIS_ LPRECT) PURE;
    STDMETHOD(Blt)(THIS_ LPRECT, LPDIRECTDRAWSURFACE7, LPRECT, DWORD, LPDDBLTFX) PURE;
    STDMETHOD(BltBatch)(THIS_ LPDDBLTBATCH, DWORD, DWORD) PURE;
    STDMETHOD(BltFast)(THIS_ DWORD, DWORD, LPDIRECTDRAWSURFACE7, LPRECT, DWORD) PURE;
    STDMETHOD(DeleteAttachedSurface)(THIS_ DWORD, LPDIRECTDRAWSURFACE7) PURE;
    STDMETHOD(EnumAttachedSurfaces)(THIS_ LPVOID, LPDDENUMSURFACESCALLBACK7) PURE;
    STDMETHOD(EnumOverlayZOrders)(THIS_ DWORD, LPVOID, LPDDENUMSURFACESCALLBACK7) PURE;
    STDMETHOD(Flip)(THIS_ LPDIRECTDRAWSURFACE7, DWORD) PURE;
    STDMETHOD(GetAttachedSurface)(THIS_ LPDDSCAPS2, LPDIRECTDRAWSURFACE7 FAR*) PURE;
    STDMETHOD(GetBltStatus)(THIS_ DWORD) PURE;
    STDMETHOD(GetCaps)(THIS_ LPDDSCAPS2) PURE;
    STDMETHOD(GetClipper)(THIS_ LPDIRECTDRAWCLIPPER FAR*) PURE;
    STDMETHOD(GetColorKey)(THIS_ DWORD, LPDDCOLORKEY) PURE;
    STDMETHOD(GetDC)(THIS_ HDC FAR*) PURE;
    STDMETHOD(GetFlipStatus)(THIS_ DWORD) PURE;
    STDMETHOD(GetOverlayPosition)(THIS_ LPLONG, LPLONG) PURE;
    STDMETHOD(GetPalette)(THIS_ LPDIRECTDRAWPALETTE FAR*) PURE;
    STDMETHOD(GetPixelFormat)(THIS_ LPDDPIXELFORMAT) PURE;
    STDMETHOD(GetSurfaceDesc)(THIS_ LPDDSURFACEDESC2) PURE;
    STDMETHOD(Initialize)(THIS_ LPDIRECTDRAW, LPDDSURFACEDESC2) PURE;
    STDMETHOD(IsLost)(THIS) PURE;
    STDMETHOD(Lock)(THIS_ LPRECT, LPDDSURFACEDESC2, DWORD, HANDLE) PURE;
    STDMETHOD(ReleaseDC)(THIS_ HDC) PURE;
    STDMETHOD(Restore)(THIS) PURE;
    STDMETHOD(SetClipper)(THIS_ LPDIRECTDRAWCLIPPER) PURE;
    STDMETHOD(SetColorKey)(THIS_ DWORD, LPDDCOLORKEY) PURE;
    STDMETHOD(SetOverlayPosition)(THIS_ LONG, LONG) PURE;
    STDMETHOD(SetPalette)(THIS_ LPDIRECTDRAWPALETTE) PURE;
    STDMETHOD(Unlock)(THIS_ LPRECT) PURE;
    STDMETHOD(UpdateOverlay)(THIS_ LPRECT, LPDIRECTDRAWSURFACE7, LPRECT, DWORD, LPDDOVERLAYFX) PURE;
    STDMETHOD(UpdateOverlayDisplay)(THIS_ DWORD) PURE;
    STDMETHOD(UpdateOverlayZOrder)(THIS_ DWORD, LPDIRECTDRAWSURFACE7) PURE;
    /*** Added in the v2 interface ***/
    STDMETHOD(GetDDInterface)(THIS_ LPVOID FAR*) PURE;
    STDMETHOD(PageLock)(THIS_ DWORD) PURE;
    STDMETHOD(PageUnlock)(THIS_ DWORD) PURE;
    /*** Added in the v3 interface ***/
    STDMETHOD(SetSurfaceDesc)(THIS_ LPDDSURFACEDESC2, DWORD) PURE;
    /*** Added in the v4 interface ***/
    STDMETHOD(SetPrivateData)(THIS_ REFGUID, LPVOID, DWORD, DWORD) PURE;
    STDMETHOD(GetPrivateData)(THIS_ REFGUID, LPVOID, LPDWORD) PURE;
    STDMETHOD(FreePrivateData)(THIS_ REFGUID) PURE;
    STDMETHOD(GetUniquenessValue)(THIS_ LPDWORD) PURE;
    STDMETHOD(ChangeUniquenessValue)(THIS) PURE;
    /*** Added in the v7 interface ***/
    STDMETHOD(SetPriority)(THIS_ DWORD) PURE;
    STDMETHOD(GetPriority)(THIS_ LPDWORD) PURE;
    STDMETHOD(SetLOD)(THIS_ DWORD) PURE;
    STDMETHOD(GetLOD)(THIS_ LPDWORD) PURE;
};

extern struct IDirectDrawSurfaceImplVtbl g_dds_vtbl;

#endif
