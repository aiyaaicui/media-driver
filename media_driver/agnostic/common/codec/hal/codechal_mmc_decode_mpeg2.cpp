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
//!
//! \file     codechal_mmc_decode_mpeg2.cpp
//! \brief    Impelements the public interface for CodecHal Media Memory Compression
//!

#include "codechal_mmc_decode_mpeg2.h"

CodechalMmcDecodeMpeg2::CodechalMmcDecodeMpeg2(
    CodechalHwInterface    *hwInterface, 
    void *standardState):
    CodecHalMmcState(hwInterface)
{
    CODECHAL_DECODE_FUNCTION_ENTER;
    
    m_mpeg2State = (CodechalDecodeMpeg2 *)standardState;
    CODECHAL_HW_ASSERT(m_mpeg2State);

    CODECHAL_HW_ASSERT(hwInterface);
    CODECHAL_HW_ASSERT(hwInterface->GetSkuTable());
    if (hwInterface &&  hwInterface->GetSkuTable() && MEDIA_IS_SKU(hwInterface->GetSkuTable(), FtrMemoryCompression))
    {
        MOS_USER_FEATURE_VALUE_DATA userFeatureData;
        MOS_ZeroMemory(&userFeatureData, sizeof(userFeatureData));
        userFeatureData.i32Data = m_mmcEnabled;
        userFeatureData.i32DataFlag = MOS_USER_FEATURE_VALUE_DATA_FLAG_CUSTOM_DEFAULT_VALUE_TYPE;

        MOS_UserFeature_ReadValue_ID(
            nullptr,
            __MEDIA_USER_FEATURE_VALUE_DECODE_MMC_ENABLE_ID,
            &userFeatureData);
        m_mmcEnabled = (userFeatureData.i32Data) ? true : false;

        MOS_USER_FEATURE_VALUE_WRITE_DATA userFeatureWriteData;
        MOS_ZeroMemory(&userFeatureWriteData, sizeof(userFeatureWriteData));
        userFeatureWriteData.Value.i32Data = m_mmcEnabled;
        userFeatureWriteData.ValueID = __MEDIA_USER_FEATURE_VALUE_DECODE_MMC_IN_USE_ID;
        MOS_UserFeature_WriteValues_ID(nullptr, &userFeatureWriteData, 1);
    }

#if (_DEBUG || _RELEASE_INTERNAL)
    m_compressibleId  = __MEDIA_USER_FEATURE_VALUE_MMC_DEC_RT_COMPRESSIBLE_ID;
    m_compressModeId  = __MEDIA_USER_FEATURE_VALUE_MMC_DEC_RT_COMPRESSMODE_ID;
#endif
}

MOS_STATUS CodechalMmcDecodeMpeg2::SetPipeBufAddr(
    PMHW_VDBOX_PIPE_BUF_ADDR_PARAMS pipeBufAddrParams,
    PMOS_COMMAND_BUFFER cmdBuffer)
{
    MOS_STATUS eStatus = MOS_STATUS_SUCCESS;
    
    CODECHAL_DECODE_FUNCTION_ENTER;

    CODECHAL_DECODE_CHK_NULL_RETURN(m_mpeg2State->picParams);

    if (m_mmcEnabled &&
        !m_mpeg2State->bDeblockingEnabled &&
        m_mpeg2State->sDestSurface.bCompressible &&
        (m_mpeg2State->picParams->m_currPic.PicFlags == PICTURE_FRAME))
    {
        pipeBufAddrParams->PreDeblockSurfMmcState = MOS_MEMCOMP_VERTICAL;
    }

    CODECHAL_DEBUG_TOOL(
        m_mpeg2State->sDestSurface.MmcState = m_mpeg2State->bDeblockingEnabled ? 
            pipeBufAddrParams->PostDeblockSurfMmcState : pipeBufAddrParams->PreDeblockSurfMmcState;
    )

    return eStatus;
}

MOS_STATUS CodechalMmcDecodeMpeg2::SetRefrenceSync(
    bool disableDecodeSyncLock,
    bool disableLockForTranscode)
{
    MOS_STATUS eStatus = MOS_STATUS_SUCCESS;
    
    CODECHAL_DECODE_FUNCTION_ENTER;

    CODECHAL_DECODE_CHK_NULL_RETURN(m_mpeg2State->picParams);

    // Check if reference surface needs to be synchronized in MMC case
    if (m_mmcEnabled&&
        (!CodecHal_PictureIsField(m_mpeg2State->picParams->m_currPic) ||
         !m_mpeg2State->picParams->m_secondField))
    {
        MOS_SYNC_PARAMS syncParams          = g_cInitSyncParams;
        syncParams.GpuContext               = m_mpeg2State->GetVideoContext();
        syncParams.bDisableDecodeSyncLock   = disableDecodeSyncLock;
        syncParams.bDisableLockForTranscode = disableLockForTranscode;

        for (uint32_t i = 0; i < CODEC_MAX_NUM_REF_FRAME_NON_AVC; i++)
        {
            if (m_mpeg2State->presReferences[i])
            {
                syncParams.presSyncResource = m_mpeg2State->presReferences[i];
                syncParams.bReadOnly = true;

                CODECHAL_DECODE_CHK_STATUS_RETURN(m_osInterface->pfnPerformOverlaySync(
                    m_osInterface,
                    &syncParams));
                CODECHAL_DECODE_CHK_STATUS_RETURN(m_osInterface->pfnResourceWait(
                    m_osInterface,
                    &syncParams));
                m_osInterface->pfnSetResourceSyncTag(m_osInterface, &syncParams);
            }
        }
    }

    return eStatus;
}

MOS_STATUS CodechalMmcDecodeMpeg2::CheckReferenceList(
        PMHW_VDBOX_PIPE_BUF_ADDR_PARAMS pipeBufAddrParams)
{
    MOS_STATUS eStatus = MOS_STATUS_SUCCESS;

    CODECHAL_DECODE_FUNCTION_ENTER;

    CODECHAL_DECODE_CHK_NULL_RETURN(pipeBufAddrParams);
    CODECHAL_DECODE_CHK_NULL_RETURN(m_mpeg2State->picParams);

    // Disable MMC if self-reference is dectected for P/B frames (mainly for error concealment)
    if (((pipeBufAddrParams->PostDeblockSurfMmcState != MOS_MEMCOMP_DISABLED) ||
         (pipeBufAddrParams->PreDeblockSurfMmcState != MOS_MEMCOMP_DISABLED)) &&
        (m_mpeg2State->picParams->m_pictureCodingType != I_TYPE))
    {
        bool selfReference = false;
        if ((m_mpeg2State->picParams->m_currPic.FrameIdx == m_mpeg2State->picParams->m_forwardRefIdx) ||
            (m_mpeg2State->picParams->m_currPic.FrameIdx == m_mpeg2State->picParams->m_backwardRefIdx))
        {
            selfReference = true;
        }

        if (selfReference)
        {
            pipeBufAddrParams->PostDeblockSurfMmcState = MOS_MEMCOMP_DISABLED;
            pipeBufAddrParams->PreDeblockSurfMmcState = MOS_MEMCOMP_DISABLED;
            CODECHAL_DECODE_ASSERTMESSAGE("Self-reference is detected for P/B frames!");

            // Decompress current frame to avoid green corruptions in this error handling case
            MOS_MEMCOMP_STATE mmcMode;
            CODECHAL_DECODE_CHK_STATUS_RETURN(m_osInterface->pfnGetMemoryCompressionMode(
                m_osInterface,
                &m_mpeg2State->sDestSurface.OsResource,
                &mmcMode));
            if (mmcMode != MOS_MEMCOMP_DISABLED)
            {
                CODECHAL_DECODE_CHK_STATUS_RETURN(m_osInterface->pfnDecompResource(
                    m_osInterface,
                    &m_mpeg2State->sDestSurface.OsResource));
            }
        }
    }

    return eStatus;
}
