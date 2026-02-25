#ifndef DDSURFACE_INTERNAL_H
#define DDSURFACE_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "IDirectDrawSurface.h"

HRESULT dds_GetDCInternal(IDirectDrawSurfaceImpl* This, HDC FAR* lpHDC);

#endif
