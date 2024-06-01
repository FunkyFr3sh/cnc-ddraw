#include <windows.h>
#include <initguid.h>
#include "IDirectDraw.h"
#include "IDirect3D.h"
#include "IAMMediaStream.h"
#include "dd.h"
#include "ddclipper.h"
#include "ddpalette.h"
#include "ddsurface.h"
#include "debug.h"
#include "hook.h"


HRESULT __stdcall IDirectDraw__QueryInterface(IDirectDrawImpl* This, REFIID riid, LPVOID FAR* ppvObj)
{
    TRACE("-> %s(This=%p, riid=%08X, ppvObj=%p)\n", __FUNCTION__, This, (unsigned int)riid, ppvObj);

    HRESULT ret = E_NOINTERFACE;

    if (riid)
    {
        if (IsEqualGUID(&IID_IDirectDraw2, riid) ||
            IsEqualGUID(&IID_IDirectDraw4, riid) ||
            IsEqualGUID(&IID_IDirectDraw7, riid))
        {
            IDirectDrawImpl* dd =
                (IDirectDrawImpl*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirectDrawImpl));

            TRACE("     GUID = %08X (IID_IDirectDrawX), ddraw = %p\n", ((GUID*)riid)->Data1, dd);

            memcpy(&dd->guid, riid, sizeof(dd->guid));
            dd->lpVtbl = &g_dd_vtblx;
            IDirectDraw_AddRef(dd);

            *ppvObj = dd;

            ret = S_OK;
        }
        else if (IsEqualGUID(&IID_IDirectDraw, riid))
        {
            IDirectDrawImpl* dd =
                (IDirectDrawImpl*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirectDrawImpl));

            TRACE("     GUID = %08X (IID_IDirectDraw), ddraw = %p\n", ((GUID*)riid)->Data1, dd);

            memcpy(&dd->guid, riid, sizeof(dd->guid));
            dd->lpVtbl = &g_dd_vtbl1;
            IDirectDraw_AddRef(dd);

            *ppvObj = dd;

            ret = S_OK;
        }
        else if (IsEqualGUID(&IID_IDirect3D, riid))
        {
            IDirect3DImpl* d3d =
                (IDirect3DImpl*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirect3DImpl));

            TRACE("     GUID = %08X (IID_IDirect3D), d3d = %p\n", ((GUID*)riid)->Data1, d3d);

            d3d->lpVtbl = &g_d3d_vtbl;
            d3d->lpVtbl->AddRef(d3d);

            *ppvObj = d3d;

            ret = S_OK;
        }
        else if (IsEqualGUID(&IID_IDirect3D2, riid))
        {
            IDirect3D2Impl* d3d =
                (IDirect3D2Impl*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirect3D2Impl));

            TRACE("     GUID = %08X (IID_IDirect3D2), d3d = %p\n", ((GUID*)riid)->Data1, d3d);

            d3d->lpVtbl = &g_d3d2_vtbl;
            d3d->lpVtbl->AddRef(d3d);

            *ppvObj = d3d;

            ret = S_OK;
        }
        else if (IsEqualGUID(&IID_IDirect3D3, riid))
        {
            IDirect3D3Impl* d3d =
                (IDirect3D3Impl*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirect3D3Impl));

            TRACE("     GUID = %08X (IID_IDirect3D3), d3d = %p\n", ((GUID*)riid)->Data1, d3d);

            d3d->lpVtbl = &g_d3d3_vtbl;
            d3d->lpVtbl->AddRef(d3d);

            *ppvObj = d3d;

            ret = S_OK;
        }
        else if (IsEqualGUID(&IID_IDirect3D7, riid))
        {
            IDirect3D7Impl* d3d =
                (IDirect3D7Impl*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirect3D7Impl));

            TRACE("     GUID = %08X (IID_IDirect3D7), d3d = %p\n", ((GUID*)riid)->Data1, d3d);

            d3d->lpVtbl = &g_d3d7_vtbl;
            d3d->lpVtbl->AddRef(d3d);

            *ppvObj = d3d;

            ret = S_OK;
        } 
        else if (IsEqualGUID(&IID_IMediaStream, riid) || IsEqualGUID(&IID_IAMMediaStream, riid))
        {
            TRACE("NOT_IMPLEMENTED     GUID = %08X (IID_IXXMediaStream)\n", ((GUID*)riid)->Data1);

            ret = E_NOINTERFACE;

            /*
            IAMMediaStreamImpl* ms =
                (IAMMediaStreamImpl*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IAMMediaStreamImpl));

            TRACE("     GUID = %08X (IID_IXXMediaStream), ms = %p\n", ((GUID*)riid)->Data1, ms);

            ms->lpVtbl = &g_ms_vtbl;
            ms->lpVtbl->AddRef(ms);

            *ppvObj = ms;

            ret = S_OK;
            */
        }
        else
        {
            TRACE("NOT_IMPLEMENTED     GUID = %08X\n", ((GUID*)riid)->Data1);

            if (!g_ddraw->real_dll)
                g_ddraw->real_dll = real_LoadLibraryA("system32\\ddraw.dll");

            if (g_ddraw->real_dll && !g_ddraw->DirectDrawCreate)
                g_ddraw->DirectDrawCreate = (void*)real_GetProcAddress(g_ddraw->real_dll, "DirectDrawCreate");

            if (g_ddraw->DirectDrawCreate == DirectDrawCreate)
                g_ddraw->DirectDrawCreate = NULL;

            if (!g_ddraw->real_dd && g_ddraw->DirectDrawCreate)
                g_ddraw->DirectDrawCreate(NULL, &g_ddraw->real_dd, NULL);

            if (g_ddraw->real_dd)
                ret = IDirectDraw_QueryInterface(g_ddraw->real_dd, riid, ppvObj);
            else
                ret = E_NOINTERFACE;
        }
       
    }

    TRACE("<- %s(result=%08X)\n", __FUNCTION__, ret);
    return ret;
}

ULONG __stdcall IDirectDraw__AddRef(IDirectDrawImpl* This)
{
    TRACE("-> %s(This=%p)\n", __FUNCTION__, This);
    ULONG ret = ++This->ref;
    ULONG glob_ref = dd_AddRef();
    TRACE("<- %s(This ref=%u, global ref=%u)\n", __FUNCTION__, ret, glob_ref);
    return ret;
}

ULONG __stdcall IDirectDraw__Release(IDirectDrawImpl* This)
{
    TRACE("-> %s(This=%p)\n", __FUNCTION__, This);

    ULONG ret = --This->ref;

    if (This->ref == 0)
    {
        TRACE("     Released (%p)\n", This);

        HeapFree(GetProcessHeap(), 0, This);
    }

    ULONG glob_ref = dd_Release();

    TRACE("<- %s(This ref=%u, global ref=%u)\n", __FUNCTION__, ret, glob_ref);
    return ret;
}

HRESULT __stdcall IDirectDraw__Compact(IDirectDrawImpl* This)
{
    TRACE("NOT_IMPLEMENTED -> %s(This=%p)\n", __FUNCTION__, This);
    HRESULT ret = DD_OK;
    TRACE("NOT_IMPLEMENTED <- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__CreateClipper(
    IDirectDrawImpl* This,
    DWORD dwFlags,
    LPDIRECTDRAWCLIPPER FAR* lplpDDClipper,
    IUnknown FAR* pUnkOuter)
{
    TRACE(
        "-> %s(This=%p, dwFlags=%08X, lplpDDClipper=%p, unkOuter=%p)\n",
        __FUNCTION__,
        This,
        dwFlags,
        lplpDDClipper,
        pUnkOuter);

    HRESULT ret = dd_CreateClipper(dwFlags, (IDirectDrawClipperImpl**)lplpDDClipper, pUnkOuter);

    TRACE("<- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__CreatePalette(
    IDirectDrawImpl* This,
    DWORD dwFlags,
    LPPALETTEENTRY lpDDColorArray,
    LPDIRECTDRAWPALETTE FAR* lpDDPalette,
    IUnknown FAR* unkOuter)
{
    TRACE(
        "-> %s(This=%p, dwFlags=%08X, lpDDColorArray=%p, lpDDPalette=%p, unkOuter=%p)\n",
        __FUNCTION__,
        This,
        dwFlags,
        lpDDColorArray,
        lpDDPalette,
        unkOuter);

    HRESULT ret = dd_CreatePalette(dwFlags, lpDDColorArray, (IDirectDrawPaletteImpl**)lpDDPalette, unkOuter);

    TRACE("<- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__CreateSurface(
    IDirectDrawImpl* This,
    LPDDSURFACEDESC2 lpDDSurfaceDesc,
    LPDIRECTDRAWSURFACE7 FAR* lpDDSurface,
    IUnknown FAR* unkOuter)
{
    TRACE(
        "-> %s(This=%p, lpDDSurfaceDesc=%p, lpDDSurface=%p, unkOuter=%p)\n",
        __FUNCTION__,
        This,
        lpDDSurfaceDesc,
        lpDDSurface,
        unkOuter);

    HRESULT ret = 
        dd_CreateSurface(
            This, 
            (LPDDSURFACEDESC)lpDDSurfaceDesc, 
            (IDirectDrawSurfaceImpl**)lpDDSurface, 
            unkOuter);

    TRACE("<- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__DuplicateSurface(
    IDirectDrawImpl* This,
    LPDIRECTDRAWSURFACE7 lpDDSrcSurface,
    LPDIRECTDRAWSURFACE7* lpDDDestSurface)
{
    TRACE("NOT_IMPLEMENTED -> %s(This=%p)\n", __FUNCTION__, This);
    HRESULT ret = DDERR_CANTDUPLICATE;
    TRACE("NOT_IMPLEMENTED <- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__EnumDisplayModes(
    IDirectDrawImpl* This,
    DWORD dwFlags,
    LPDDSURFACEDESC2 lpDDSurfaceDesc,
    LPVOID lpContext,
    LPDDENUMMODESCALLBACK2 lpEnumModesCallback)
{
    TRACE(
        "-> %s(This=%p, dwFlags=%08X, lpDDSurfaceDesc=%p, lpContext=%p, lpEnumModesCallback=%p)\n",
        __FUNCTION__,
        This,
        dwFlags,
        lpDDSurfaceDesc,
        lpContext,
        lpEnumModesCallback);

    HRESULT ret = 
        dd_EnumDisplayModes(
            dwFlags, 
            (LPDDSURFACEDESC)lpDDSurfaceDesc, 
            lpContext, 
            (LPDDENUMMODESCALLBACK)lpEnumModesCallback);

    TRACE("<- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__EnumSurfaces(
    IDirectDrawImpl* This,
    DWORD dwFlags,
    LPDDSURFACEDESC2 lpDDSurfaceDesc,
    LPVOID lpContext,
    LPDDENUMSURFACESCALLBACK7 lpEnumSurfacesCallback)
{
    TRACE("NOT_IMPLEMENTED -> %s(This=%p)\n", __FUNCTION__, This);
    HRESULT ret = DD_OK;
    TRACE("NOT_IMPLEMENTED <- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__FlipToGDISurface(IDirectDrawImpl* This)
{
    TRACE("NOT_IMPLEMENTED -> %s(This=%p)\n", __FUNCTION__, This);
    HRESULT ret = DD_OK;
    TRACE("NOT_IMPLEMENTED <- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__GetCaps(IDirectDrawImpl* This, LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDEmulCaps)
{
    TRACE("-> %s(This=%p, lpDDDriverCaps=%p, lpDDEmulCaps=%p)\n", __FUNCTION__, This, lpDDDriverCaps, lpDDEmulCaps);
    HRESULT ret = dd_GetCaps((LPDDCAPS_DX1)lpDDDriverCaps, (LPDDCAPS_DX1)lpDDEmulCaps);
    TRACE("<- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__GetDisplayMode(IDirectDrawImpl* This, LPDDSURFACEDESC2 lpDDSurfaceDesc)
{
    TRACE("-> %s(This=%p)\n", __FUNCTION__, This);
    HRESULT ret = dd_GetDisplayMode((LPDDSURFACEDESC)lpDDSurfaceDesc);
    TRACE("<- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__GetFourCCCodes(IDirectDrawImpl* This, LPDWORD lpNumCodes, LPDWORD lpCodes)
{
    TRACE("NOT_IMPLEMENTED -> %s(This=%p)\n", __FUNCTION__, This);
    HRESULT ret = DDERR_INVALIDOBJECT;
    TRACE("NOT_IMPLEMENTED <- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__GetGDISurface(IDirectDrawImpl* This, LPDIRECTDRAWSURFACE7* lplpGDIDDSurface)
{
    TRACE("NOT_IMPLEMENTED -> %s(This=%p)\n", __FUNCTION__, This);
    HRESULT ret = DD_OK;
    *lplpGDIDDSurface = (LPDIRECTDRAWSURFACE7)g_ddraw->primary;
    TRACE("NOT_IMPLEMENTED <- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__GetMonitorFrequency(IDirectDrawImpl* This, LPDWORD lpdwFreq)
{
    TRACE("-> %s(This=%p)\n", __FUNCTION__, This);
    HRESULT ret = dd_GetMonitorFrequency(lpdwFreq);
    TRACE("<- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__GetScanLine(IDirectDrawImpl* This, LPDWORD lpdwScanLine)
{
    TRACE("NOT_IMPLEMENTED -> %s(This=%p)\n", __FUNCTION__, This);
    HRESULT ret = DDERR_UNSUPPORTED;
    TRACE("NOT_IMPLEMENTED <- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__GetVerticalBlankStatus(IDirectDrawImpl* This, LPBOOL lpbIsInVB)
{
    TRACE("-> %s(This=%p, lpbIsInVB=%p)\n", __FUNCTION__, This, lpbIsInVB);
    HRESULT ret = dd_GetVerticalBlankStatus(lpbIsInVB);
    TRACE("<- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__Initialize(IDirectDrawImpl* This, GUID* lpGUID)
{
    TRACE("-> %s(This=%p)\n", __FUNCTION__, This);
    HRESULT ret = DD_OK;
    TRACE("<- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__RestoreDisplayMode(IDirectDrawImpl* This)
{
    TRACE("-> %s(This=%p)\n", __FUNCTION__, This);
    HRESULT ret = dd_RestoreDisplayMode();
    TRACE("<- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__SetCooperativeLevel(IDirectDrawImpl* This, HWND hwnd, DWORD dwFlags)
{
    TRACE("-> %s(This=%p, hwnd=%p, dwFlags=0x%08X)\n", __FUNCTION__, This, hwnd, dwFlags);
    HRESULT ret = dd_SetCooperativeLevel(hwnd, dwFlags);
    TRACE("<- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__SetDisplayMode(IDirectDrawImpl* This, DWORD dwWidth, DWORD dwHeight, DWORD dwBPP)
{
    TRACE("-> %s(This=%p, dwWidth=%d, dwHeight=%d, dwBPP=%d)\n", __FUNCTION__, This, dwWidth, dwHeight, dwBPP);
    HRESULT ret = dd_SetDisplayMode(dwWidth, dwHeight, dwBPP, SDM_MODE_SET_BY_GAME);
    TRACE("<- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__SetDisplayModeX(
    IDirectDrawImpl* This,
    DWORD dwWidth,
    DWORD dwHeight,
    DWORD dwBPP,
    DWORD dwRefreshRate,
    DWORD dwFlags)
{
    TRACE(
        "-> %s(This=%p, dwWidth=%d, dwHeight=%d, dwBPP=%d, refreshRate=%d, dwFlags=%d)\n",
        __FUNCTION__,
        This,
        dwWidth,
        dwHeight,
        dwBPP,
        dwRefreshRate,
        dwFlags);

    HRESULT ret = dd_SetDisplayMode(dwWidth, dwHeight, dwBPP, SDM_MODE_SET_BY_GAME);

    TRACE("<- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__WaitForVerticalBlank(IDirectDrawImpl* This, DWORD dwFlags, HANDLE hEvent)
{
    TRACE_EXT("-> %s(This=%p, dwFlags=%08X, hEvent=%p)\n", __FUNCTION__, This, dwFlags, hEvent);
    HRESULT ret = dd_WaitForVerticalBlank(dwFlags, hEvent);
    TRACE_EXT("<- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__GetAvailableVidMem(
    IDirectDrawImpl* This,
    LPDDSCAPS2 lpDDCaps,
    LPDWORD lpdwTotal,
    LPDWORD lpdwFree)
{
    TRACE(
        "-> %s(This=%p, lpDDCaps=%p, lpdwTotal=%p, lpdwFree=%p)\n",
        __FUNCTION__,
        This,
        lpDDCaps,
        lpdwTotal,
        lpdwFree);

    HRESULT ret = dd_GetAvailableVidMem((LPDDSCAPS)lpDDCaps, lpdwTotal, lpdwFree);

    TRACE("<- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__GetSurfaceFromDC(IDirectDrawImpl* This, HDC hdc, LPDIRECTDRAWSURFACE7* lplpDDSurface)
{
    TRACE("NOT_IMPLEMENTED -> %s(This=%p)\n", __FUNCTION__, This);
    HRESULT ret = DDERR_NOTFOUND;
    TRACE("NOT_IMPLEMENTED <- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__RestoreAllSurfaces(IDirectDrawImpl* This)
{
    TRACE("-> %s(This=%p)\n", __FUNCTION__, This);
    HRESULT ret = DD_OK;
    TRACE("<- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__TestCooperativeLevel(IDirectDrawImpl* This)
{
    TRACE_EXT("-> %s(This=%p)\n", __FUNCTION__, This);
    HRESULT ret = DD_OK;
    TRACE_EXT("<- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__GetDeviceIdentifier(IDirectDrawImpl* This, LPDDDEVICEIDENTIFIER2 pDDDI, DWORD dwFlags)
{
    TRACE("-> %s(This=%p, pDDDI=%p, dwFlags=%08X)\n", __FUNCTION__, This, pDDDI, dwFlags);
    HRESULT ret = dd_GetDeviceIdentifier((LPDDDEVICEIDENTIFIER)pDDDI, dwFlags, &This->guid);
    TRACE("<- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__StartModeTest(IDirectDrawImpl* This, LPSIZE pModes, DWORD dwNumModes, DWORD dwFlags)
{
    TRACE("NOT_IMPLEMENTED -> %s(This=%p)\n", __FUNCTION__, This);
    HRESULT ret = DDERR_CURRENTLYNOTAVAIL;
    TRACE("NOT_IMPLEMENTED <- %s\n", __FUNCTION__);
    return ret;
}

HRESULT __stdcall IDirectDraw__EvaluateMode(IDirectDrawImpl* This, DWORD dwFlags, DWORD* pTimeout)
{
    TRACE("NOT_IMPLEMENTED -> %s(This=%p)\n", __FUNCTION__, This);
    HRESULT ret = DDERR_INVALIDOBJECT;
    TRACE("NOT_IMPLEMENTED <- %s\n", __FUNCTION__);
    return ret;
}

struct IDirectDrawImplVtbl g_dd_vtbl1 =
{
    /*** IUnknown methods ***/
    IDirectDraw__QueryInterface,
    IDirectDraw__AddRef,
    IDirectDraw__Release,
    /*** IDirectDraw methods ***/
    IDirectDraw__Compact,
    IDirectDraw__CreateClipper,
    IDirectDraw__CreatePalette,
    IDirectDraw__CreateSurface,
    IDirectDraw__DuplicateSurface,
    IDirectDraw__EnumDisplayModes,
    IDirectDraw__EnumSurfaces,
    IDirectDraw__FlipToGDISurface,
    IDirectDraw__GetCaps,
    IDirectDraw__GetDisplayMode,
    IDirectDraw__GetFourCCCodes,
    IDirectDraw__GetGDISurface,
    IDirectDraw__GetMonitorFrequency,
    IDirectDraw__GetScanLine,
    IDirectDraw__GetVerticalBlankStatus,
    IDirectDraw__Initialize,
    IDirectDraw__RestoreDisplayMode,
    IDirectDraw__SetCooperativeLevel,
    {IDirectDraw__SetDisplayMode},
    IDirectDraw__WaitForVerticalBlank,
    /*** Added in the v2 Interface ***/
    IDirectDraw__GetAvailableVidMem,
    /*** Added in the v4 Interface ***/
    IDirectDraw__GetSurfaceFromDC,
    IDirectDraw__RestoreAllSurfaces,
    IDirectDraw__TestCooperativeLevel,
    IDirectDraw__GetDeviceIdentifier,
    /*** Added in the v7 Interface ***/
    IDirectDraw__StartModeTest,
    IDirectDraw__EvaluateMode,
};

struct IDirectDrawImplVtbl g_dd_vtblx =
{
    /*** IUnknown methods ***/
    IDirectDraw__QueryInterface,
    IDirectDraw__AddRef,
    IDirectDraw__Release,
    /*** IDirectDraw methods ***/
    IDirectDraw__Compact,
    IDirectDraw__CreateClipper,
    IDirectDraw__CreatePalette,
    IDirectDraw__CreateSurface,
    IDirectDraw__DuplicateSurface,
    IDirectDraw__EnumDisplayModes,
    IDirectDraw__EnumSurfaces,
    IDirectDraw__FlipToGDISurface,
    IDirectDraw__GetCaps,
    IDirectDraw__GetDisplayMode,
    IDirectDraw__GetFourCCCodes,
    IDirectDraw__GetGDISurface,
    IDirectDraw__GetMonitorFrequency,
    IDirectDraw__GetScanLine,
    IDirectDraw__GetVerticalBlankStatus,
    IDirectDraw__Initialize,
    IDirectDraw__RestoreDisplayMode,
    IDirectDraw__SetCooperativeLevel,
    {IDirectDraw__SetDisplayModeX},
    IDirectDraw__WaitForVerticalBlank,
    /*** Added in the v2 interface ***/
    IDirectDraw__GetAvailableVidMem,
    /*** Added in the v4 Interface ***/
    IDirectDraw__GetSurfaceFromDC,
    IDirectDraw__RestoreAllSurfaces,
    IDirectDraw__TestCooperativeLevel,
    IDirectDraw__GetDeviceIdentifier,
    /*** Added in the v7 Interface ***/
    IDirectDraw__StartModeTest,
    IDirectDraw__EvaluateMode,
};
