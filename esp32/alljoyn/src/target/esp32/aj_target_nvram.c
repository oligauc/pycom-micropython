/**
 * @file
 */
/******************************************************************************
 * Copyright AllSeen Alliance. All rights reserved.
 *
 *    Permission to use, copy, modify, and/or distribute this software for any
 *    purpose with or without fee is hereby granted, provided that the above
 *    copyright notice and this permission notice appear in all copies.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 ******************************************************************************/

/**
 * Per-module definition of the current module for debug logging.  Must be defined
 * prior to first inclusion of aj_debug.h
 */
#define AJ_MODULE TARGET_NVRAM

#include "ff.h"
#include <aj_nvram.h>
#include <aj_debug.h>
#include "../../aj_target_nvram.h"

/**
 * Turn on per-module debug printing by setting this variable to non-zero value
 * (usually in debugger).
 */
#ifndef NDEBUG
uint8_t dbgTARGET_NVRAM = 0;
#endif

uint8_t AJ_EMULATED_NVRAM[AJ_NVRAM_SIZE];
uint8_t* AJ_NVRAM_BASE_ADDRESS;

extern void AJ_NVRAM_Layout_Print();

#define NV_FILE "/flash/ajtcl.nvram"

const char* nvFile = NV_FILE;

void AJ_SetNVRAM_FilePath(const char* path)
{
    if (path) {
        nvFile = path;
    }
}

void AJ_NVRAM_Init()
{
    AJ_NVRAM_BASE_ADDRESS = AJ_EMULATED_NVRAM;
    _AJ_LoadNVFromFile();
    if (*((uint32_t*)AJ_NVRAM_BASE_ADDRESS) != AJ_NV_SENTINEL) {
        AJ_NVRAM_Clear();
        _AJ_StoreNVToFile();
    }
}

void _AJ_NV_Write(void* dest, const void* buf, uint16_t size)
{
    memcpy(dest, buf, size);
    _AJ_StoreNVToFile();
}

void _AJ_NV_Move(void* dest, const void* buf, uint16_t size)
{
    memmove(dest, buf, size);
    _AJ_StoreNVToFile();
}

void _AJ_NV_Read(void* src, void* buf, uint16_t size)
{
    memcpy(buf, src, size);
}

void _AJ_NVRAM_Clear()
{
    memset((uint8_t*)AJ_NVRAM_BASE_ADDRESS, INVALID_DATA_BYTE, AJ_NVRAM_SIZE);
    *((uint32_t*)AJ_NVRAM_BASE_ADDRESS) = AJ_NV_SENTINEL;
    _AJ_StoreNVToFile();
}

AJ_Status _AJ_LoadNVFromFile()
{
    FIL fp;		
    FRESULT res = FR_OK;

    res = f_open(&fp, nvFile, FA_OPEN_EXISTING | FA_READ);
    printf("+++ _AJ_LoadNVFromFile f_open return status %d\n",res);
    if (res != FR_OK){
	printf("_AJ_LoadNVFromFile(): LoadNVFromFile() failed. status=AJ_ERR_FAILURE\n");
        return AJ_ERR_FAILURE;
    }
     
    memset(AJ_NVRAM_BASE_ADDRESS, INVALID_DATA_BYTE, AJ_NVRAM_SIZE);

    UINT sz_out = 0;
    res = f_read (&fp, AJ_NVRAM_BASE_ADDRESS, AJ_NVRAM_SIZE, &sz_out);
    printf("_AJ_StoreNVToFile - f_read res: %d, read: %d\n", res, sz_out);
     
    f_close(&fp);
    return AJ_OK;

    /*FILE* f = fopen(nvFile, "r");
    if (f == NULL) {
        printf("_AJ_LoadNVFromFile(): LoadNVFromFile() failed. status=AJ_ERR_FAILURE\n");
        return AJ_ERR_FAILURE;
    }

    memset(AJ_NVRAM_BASE_ADDRESS, INVALID_DATA_BYTE, AJ_NVRAM_SIZE);
    fread(AJ_NVRAM_BASE_ADDRESS, AJ_NVRAM_SIZE, 1, f);
    fclose(f);
    return AJ_OK;*/
}

AJ_Status _AJ_StoreNVToFile()
{
    FIL fp;		
    FRESULT res = FR_OK;

    res = f_open(&fp, nvFile, FA_CREATE_ALWAYS | FA_WRITE);
    printf("+++ _AJ_StoreNVToFile f_open return status %d\n",res);
    if (res != FR_OK){
	printf("_AJ_StoreNVToFile(): StoreNVToFile() failed. status=AJ_ERR_FAILURE\n");
        return AJ_ERR_FAILURE;
    }

    UINT n;
    res = f_write(&fp, AJ_NVRAM_BASE_ADDRESS, AJ_NVRAM_SIZE, &n);
    printf("_AJ_StoreNVToFile - f_write res: %d, written: %d\n", res,n);
    f_close(&fp);
    return AJ_OK;

    /*FILE* f = fopen(nvFile, "w");
    if (!f) {
        printf("_AJ_StoreNVToFile(): StoreNVToFile() failed. status=AJ_ERR_FAILURE\n");
        return AJ_ERR_FAILURE;
    }

    fwrite(AJ_NVRAM_BASE_ADDRESS, AJ_NVRAM_SIZE, 1, f);
    fclose(f);
    return AJ_OK;*/
}

// Compact the storage by removing invalid entries
AJ_Status _AJ_CompactNVStorage()
{
    uint16_t capacity = 0;
    uint16_t id = 0;
    uint16_t* data = (uint16_t*)(AJ_NVRAM_BASE_ADDRESS + SENTINEL_OFFSET);
    uint8_t* writePtr = (uint8_t*)data;
    uint16_t entrySize = 0;
    uint16_t garbage = 0;
    //AJ_NVRAM_Layout_Print();
    while ((uint8_t*)data < (uint8_t*)AJ_NVRAM_END_ADDRESS && *data != INVALID_DATA) {
        id = *data;
        capacity = *(data + 1);
        entrySize = ENTRY_HEADER_SIZE + capacity;
        if (id != INVALID_ID) {
            _AJ_NV_Move(writePtr, data, entrySize);
            writePtr += entrySize;
        } else {
            garbage += entrySize;
        }
        data += entrySize >> 1;
    }

    memset(writePtr, INVALID_DATA_BYTE, garbage);
    _AJ_StoreNVToFile();
    //AJ_NVRAM_Layout_Print();
    return AJ_OK;
}
