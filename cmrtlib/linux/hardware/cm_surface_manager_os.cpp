/*
* Copyright (c) 2017, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

/**-----------------------------------------------------------------------------
***
*** Copyright  (C) 1985-2016 Intel Corporation. All rights reserved.
***
*** The information and source code contained herein is the exclusive
*** property of Intel Corporation. and may not be disclosed, examined
*** or reproduced in whole or in part without explicit written authorization
*** from the company.
***
*** ----------------------------------------------------------------------------
*/
#include "cm_surface_manager.h"
#include "cm_debug.h"
#include "cm_def.h"
#include "cm_device.h"
#include "cm_mem.h"

int32_t CmSurfaceManager::Surface2DSanityCheck(uint32_t width,
                                           uint32_t height,
                                           CM_SURFACE_FORMAT format)
{
    if ((width < CM_MIN_SURF_WIDTH) || (width > CM_MAX_2D_SURF_WIDTH))
    {
        CmAssert(0);
        return CM_INVALID_WIDTH;
    }
    if ((height < CM_MIN_SURF_HEIGHT) || (height > CM_MAX_2D_SURF_HEIGHT))
    {
        CmAssert(0);
        return CM_INVALID_HEIGHT;
    }

    switch (format)
    {
    case CM_SURFACE_FORMAT_X8R8G8B8:
    case CM_SURFACE_FORMAT_A8R8G8B8:
    case CM_SURFACE_FORMAT_A8B8G8R8:
    case CM_SURFACE_FORMAT_R32F:
    case CM_SURFACE_FORMAT_A16B16G16R16:
    case CM_SURFACE_FORMAT_R10G10B10A2:
    case CM_SURFACE_FORMAT_A16B16G16R16F:
    case CM_SURFACE_FORMAT_L16:
    case CM_SURFACE_FORMAT_D16:
    case CM_SURFACE_FORMAT_A8:
    case CM_SURFACE_FORMAT_P8:
    case CM_SURFACE_FORMAT_V8U8:
    case CM_SURFACE_FORMAT_R16_UINT:
    case CM_SURFACE_FORMAT_R8_UINT:
        break;

    case CM_SURFACE_FORMAT_UYVY:
    case CM_SURFACE_FORMAT_YUY2:
        if (width & 0x1)
        {
            CmAssert(0);
            return CM_INVALID_WIDTH;
        }
        break;

    case CM_SURFACE_FORMAT_P016:
    case CM_SURFACE_FORMAT_P010:
    case CM_SURFACE_FORMAT_YV12:
    case CM_SURFACE_FORMAT_422H:
    case CM_SURFACE_FORMAT_444P:
    case CM_SURFACE_FORMAT_422V:
    case CM_SURFACE_FORMAT_411P:
    case CM_SURFACE_FORMAT_IMC3:
        if (width & 0x1)
        {
            CmAssert(0);
            return CM_INVALID_WIDTH;
        }
        if (height & 0x1)
        {
            CmAssert(0);
            return CM_INVALID_HEIGHT;
        }
        break;

    case CM_SURFACE_FORMAT_NV12:
        if (width & 0x1)
        {
            CmAssert(0);
            return CM_INVALID_WIDTH;
        }
        if (height & 0x1)
        {
            CmAssert(0);
            return CM_INVALID_HEIGHT;
        }
        break;

    default:
        CmAssert(0);
        return CM_SURFACE_FORMAT_NOT_SUPPORTED;
    }

    return CM_SUCCESS;
}

int32_t CmSurfaceManager::GetBytesPerPixel(CM_SURFACE_FORMAT format)
{
    uint32_t sizePerPixel = 0;
    switch (format)
    {
    case CM_SURFACE_FORMAT_A16B16G16R16:
    case CM_SURFACE_FORMAT_A16B16G16R16F:
        sizePerPixel = 8;
        break;

    case CM_SURFACE_FORMAT_X8R8G8B8:
    case CM_SURFACE_FORMAT_A8R8G8B8:
    case CM_SURFACE_FORMAT_R32F:
    case CM_SURFACE_FORMAT_R10G10B10A2:
    case CM_SURFACE_FORMAT_A8B8G8R8:
        sizePerPixel = 4;
        break;

    case CM_SURFACE_FORMAT_444P:
    case CM_SURFACE_FORMAT_422H:
    case CM_SURFACE_FORMAT_411P:
        sizePerPixel = 3;
        break;

    case CM_SURFACE_FORMAT_YUY2:
    case CM_SURFACE_FORMAT_UYVY:
    case CM_SURFACE_FORMAT_V8U8:
    case CM_SURFACE_FORMAT_IMC3:
    case CM_SURFACE_FORMAT_422V:
    case CM_SURFACE_FORMAT_L16:
    case CM_SURFACE_FORMAT_D16:
        sizePerPixel = 2;
        break;

    case CM_SURFACE_FORMAT_A8:
    case CM_SURFACE_FORMAT_P8:
        sizePerPixel = 1;
        break;

    case CM_SURFACE_FORMAT_NV12:
        sizePerPixel = 1;
        break;

    default:
        CmAssert(0);
        CmDebugMessage((" Fail to get surface description! "));
        return CM_SURFACE_FORMAT_NOT_SUPPORTED;
    }

    return sizePerPixel;
}

int32_t CmSurfaceManager::CreateSurface2D(uint32_t width,
                                      uint32_t height,
                                      CM_SURFACE_FORMAT format,
                                      CmSurface2D *&surface)
{
    int32_t result = CM_SUCCESS;

    //Sanity check
    CHK_RET(Surface2DSanityCheck(width, height, format));

    // Passes the reference to CMRT@UMD to create a CmSurface.
    CHK_RET(AllocateSurface2DInUmd(width, height, format, true, false, 0, surface));
    CHK_NULL(surface);

finish:
    if (FAILED(result))
    {
        surface = nullptr;
    }

    return result;
}

int32_t CmSurfaceManager::CreateSurface2D(VASurfaceID vaSurface,
                                      CmSurface2D *&surface)
{  // Not created by Cm , but it is created through Libva
    return CreateSurface2D(vaSurface, false, true, surface);
}

int32_t CmSurfaceManager::CreateSurface2D(VASurfaceID *vaSurfaceArray,
                                      const uint32_t surfaceCount,
                                      CmSurface2D **surfaceArray)
{
    int32_t result = CM_FAILURE;
    uint32_t SurfIndex = 0;
    for (SurfIndex = 0; SurfIndex < surfaceCount; SurfIndex++)
    {
        CHK_RET(CreateSurface2D(vaSurfaceArray[SurfIndex], surfaceArray[SurfIndex]));
        CHK_NULL(surfaceArray[SurfIndex]);
    }

finish:
    if (result != CM_SUCCESS)
    {
        for (uint32_t j = 0; j < SurfIndex; j++)
        {
            DestroySurface(surfaceArray[j]);
        }
    }
    return result;
}

int32_t CmSurfaceManager::CreateVaSurface2D(uint32_t width,
                                        uint32_t height,
                                        CM_SURFACE_FORMAT format,
                                        VASurfaceID &vaSurface,
                                        CmSurface2D *&surface)
{
    int32_t hr = CM_SUCCESS;
    VAStatus va_status = VA_STATUS_SUCCESS;
    VASurfaceID va_surface_id = 0;
    VADisplay *pdpy = nullptr;
    uint32_t VaFormat = 0;

    VaFormat = ConvertToLibvaFormat(format);

    //Create Va Surface
    m_device->GetVaDpy(pdpy);
    if(pdpy == nullptr)
    {
        CmAssert(0);
        return CM_INVALID_LIBVA_INITIALIZE;
    }

    // Set attribute for vaCreateSurfaces
    VASurfaceAttrib surfaceAttrib;
    surfaceAttrib.type = VASurfaceAttribPixelFormat;
    surfaceAttrib.value.type = VAGenericValueTypeInteger;
    surfaceAttrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
    surfaceAttrib.value.value.i = VaFormat;

    // since no 10-bit format is supported in MDF, 
    // the VA_RT_FORMAT_YUV420 will be overwritten
    // by the format in attribute
    va_status = vaCreateSurfaces(*pdpy, VA_RT_FORMAT_YUV420, width, height, &vaSurface, 1,
                                 &surfaceAttrib, 1);
    if (va_status != VA_STATUS_SUCCESS)
    {
        CmAssert(0);
        return CM_VA_SURFACE_NOT_SUPPORTED;
    }

    //Create Cm Surface
    hr = CreateSurface2D(vaSurface, true, true, surface);
    if (hr != CM_SUCCESS)
    {
        CmAssert(0);
        va_status = vaDestroySurfaces(*pdpy, &vaSurface, 1);
        return hr;
    }

    return CM_SUCCESS;
}

int32_t CmSurfaceManager::CreateSurface2D(VASurfaceID &vaSurface,
                                      bool cmCreated,
                                      bool createdbyLibva,
                                      CmSurface2D *&surface)
{
    int32_t result = CM_SUCCESS;
    //Pass it to CMRT@UMD to create CmSurface
    CHK_RET(AllocateSurface2DInUmd(0, 0, (CM_SURFACE_FORMAT)0, cmCreated, createdbyLibva,
                                 vaSurface, surface));
    CHK_NULL(surface);

finish:
    return result;
}

int32_t CmSurfaceManager::ConvertToLibvaFormat(int32_t format)
{
    int32_t VAFmt = format;
    switch (format)
    {
    case VA_CM_FMT_A8R8G8B8:
        VAFmt = VA_FOURCC_ARGB;
        break;

    default:
        VAFmt = format;
        break;
    }
    return VAFmt;
}

int32_t CmSurfaceManager::AllocateSurface2DInUmd(uint32_t width,
                                           uint32_t height,
                                           CM_SURFACE_FORMAT format,
                                           bool cmCreated,
                                           bool createdbyLibva,
                                           VASurfaceID vaSurface,
                                           CmSurface2D *&surface)
{
    VADisplay *pDisplay = nullptr;
    m_device->GetVaDpy(pDisplay);

    CM_CREATESURFACE2D_PARAM inParam;
    CmSafeMemSet(&inParam, 0, sizeof(CM_CREATESURFACE2D_PARAM));
    inParam.iWidth = width;
    inParam.iHeight = height;
    inParam.Format = format;
    inParam.bIsCmCreated = cmCreated;
    inParam.bIsLibvaCreated = createdbyLibva;
    inParam.uiVASurfaceID = vaSurface;
    inParam.pVaDpy = pDisplay;

    int32_t hr = m_device->OSALExtensionExecute(CM_FN_CMDEVICE_CREATESURFACE2D,
                                                &inParam, sizeof(inParam),
                                                nullptr, 0);
    CHK_FAILURE_RETURN(hr);
    CHK_FAILURE_RETURN(inParam.iReturnValue);
    surface = (CmSurface2D *)inParam.pCmSurface2DHandle;

    return hr;
}