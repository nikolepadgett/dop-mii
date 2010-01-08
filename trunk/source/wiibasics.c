/*-------------------------------------------------------------

wiibasics.c -- basic Wii initialization and functions

Copyright (C) 2008 tona
Unless other credit specified

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <wiiuse/wpad.h>

#include "wiibasics.h"
#include "controller.h"
#include "tools.h"
#include "gecko.h"
#include "video.h"

//#include "id.h"

#define MAX_WIIMOTES 4

u16 be16(const u8 *p) {
    return (p[0] << 8) | p[1];
}

u32 be32(const u8 *p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

u64 be64(const u8 *p) {
    return ((u64)be32(p) << 32) | be32(p + 4);
}

u32 getTitleIDUID(u64 titleID) {
    s32 ret, i;
    static u8 uid_buffer[0x1000] ATTRIBUTE_ALIGN(32);
    u32 size;
    ret = ISFS_ReadFileToArray("/sys/uid.sys", uid_buffer, 0x1000, &size);
    if (ret < 0)
        return 0;

    for (i = 0; i < size; i+=12)
        if (be64(&uid_buffer[i]) == titleID)
            return be32(&uid_buffer[i+8]);

    return 0;
}

u64 getUIDTitleID(u32 uid) {
    s32 ret, i;
    static u8 uid_buffer[0x1000] ATTRIBUTE_ALIGN(32);
    u32 size;
    ret = ISFS_ReadFileToArray("/sys/uid.sys", uid_buffer, 0x1000, &size);
    if (ret < 0)
        return 0;

    for (i = 8; i < size; i+=12)
        if (be32(&uid_buffer[i]) == uid)
            return be64(&uid_buffer[i-8]);
    return 0;
}

/* Basic init taken pretty directly from the libOGC examples */
void WiiInit() 
{
	InitGecko(); 
	gprintf("\n\nInitializing Wii\n");
	gprintf("- VideoInit\n"); VideoInit();  
	gprintf("- WPAD_Init\n"); WPAD_Init();
	gprintf("- PAD_Init\n"); PAD_Init();
}

void miscInit(void) {
    int ret;

    // This function initialises the attached controllers
    WPAD_Init();

    // snip!
    //Identify_SU();

    printf("Initializing Filesystem driver...");
    fflush(stdout);

    ret = ISFS_Initialize();
    if (ret < 0) 
	{
        printf("\nError! ISFS_Initialize (ret = %d)\n", ret);
		WaitAnyKey();
        ReturnToLoader();
    } 
	else printf("OK!\n");

    //IdentSysMenu();
}

void IdentSysMenu(void) {
    int ret;

    // snip!
    //Identify_SysMenu();

    ret = ES_SetUID(TITLE_ID(1, 2));
    if (ret < 0) 
	{
        printf("SetUID fail %d", ret);
		WaitAnyKey();
		ReturnToLoader();
    }

    printf("Initializing Filesystem driver...");
    fflush(stdout);

    ISFS_Deinitialize();
    ret = ISFS_Initialize();
    if (ret < 0) 
	{
        printf("\nError! ISFS_Initialize (ret = %d)\n", ret);
		WaitAnyKey();
		ReturnToLoader();
    } 
	else printf("OK!\n");
}

void miscDeInit() 
{
    fflush(stdout);
    ISFS_Deinitialize();
}

char charASCII(u8 c) 
{
    if (c < 0x20 || c > 0x7E) return '.';
    else return (char)c;
}

void hex_print_array16(const u8 *array, u32 size) 
{
    u32 offset = 0;
    u32 page_size = 0x100;
    char line[17];
    line[16] = 0;
    if (size > page_size) printf("Page 1 of %u", (size / page_size)+1);
	u32 button = 0;

    while (offset < size) 
	{
        if (!(offset % 16)) printf("\n0x%08X: ", offset);

        printf("%02X", array[offset]);

        line[offset % 16] = charASCII(array[offset]);

        if (!(++offset % 2)) printf(" ");

        if (!(offset % 16)) printf(line);

        if (!(offset % page_size) && offset < size) 
		{

            printf("\n\tPress A for next page or B for finish\n");
			for (button = 0;;ScanPads(&button))
			{
				if (button&WPAD_BUTTON_HOME) ReturnToLoader();
				if (button&WPAD_BUTTON_A) break;
				if (button&WPAD_BUTTON_B) return;
			}
        }
    }
}

bool PromptYesNo()
{
    printf("      [A] Yes        [B] NO    [HOME|START] Exit\n");

	u32 button;
	for (button = 0;;ScanPads(&button))
	{
		if (button&WPAD_BUTTON_A) return true;
		if (button&WPAD_BUTTON_B) return false;
		if (button&WPAD_BUTTON_HOME) ReturnToLoader();
	}
}

bool PromptContinue() 
{
    printf("Are you sure you want to continue?\n");
    printf("      [A] Yes        [B] NO    [HOME|START] Exit\n");	

	u32 button;
	for (button = 0;;ScanPads(&button))
	{
		if (button&WPAD_BUTTON_A) return true;
		if (button&WPAD_BUTTON_B) return false;
		if (button&WPAD_BUTTON_HOME) ReturnToLoader();
	}
}

/* Reads a file from ISFS to an array in memory */
s32 ISFS_ReadFileToArray (const char *filepath, u8 *filearray, u32 max_size, u32 *file_size) {
    s32 ret, fd;
    static fstats filestats ATTRIBUTE_ALIGN(32);

    *file_size = 0;
    ret = ISFS_Open(filepath, ISFS_OPEN_READ);
    if (ret <= 0) {
        printf("Error! ISFS_Open (ret = %d)\n", ret);
        return -1;
    }

    fd = ret;

    ret = ISFS_GetFileStats(fd, &filestats);
    if (ret < 0) {
        printf("Error! ISFS_GetFileStats (ret = %d)\n", ret);
        return -1;
    }

    *file_size = filestats.file_length;

    if (*file_size > max_size) {
        printf("File is too large! Size: %u Max: %u", *file_size, max_size);
        return -1;
    }

    ret = ISFS_Read(fd, filearray, *file_size);
    *file_size = ret;
    if (ret < 0) {
        printf("Error! ISFS_Read (ret = %d)\n", ret);
        return -1;
    } else if (ret != filestats.file_length) {
        printf("Error! ISFS_Read Only read: %d\n", ret);
        return -1;
    }

    ret = ISFS_Close(fd);
    if (ret < 0) {
        printf("Error! ISFS_Close (ret = %d)\n", ret);
        return -1;
    }
    return 0;
}

/* Writes from an array in memory to a file with ISFS */
s32 ISFS_WriteFileFromArray (const char *filepath, const u8 *filearray, u32 array_size, u32 ownerID, u16 groupID, u8 attr, u8 own_perm, u8 group_perm, u8 other_perm) {
    s32 ret, fd = 0, out;
    u64 currentTid;
    u32 realownid;
    u16 realgroupid;
    u8 realattr, realownperm, realgroupperm, realotherperm;
    static fstats filestats ATTRIBUTE_ALIGN(32);

    out = 0;

    ret = ISFS_Open(filepath, ISFS_OPEN_WRITE);
    if (ret == -102) {

        printf("\tMaking file writable...\n");
        ret = ISFS_SetAttr(filepath, ownerID, groupID, attr, 3, 3, 3);
        if (ret < 0) {
            printf("Error! ISFS_SetAttr (ret = %d)\n", ret);
            out = -1;
            goto cleanup;
        }

        return ISFS_WriteFileFromArray(filepath, filearray, array_size, ownerID, groupID, attr, own_perm, group_perm, other_perm);

    } else if (ret == -106) {

        printf("\tCreating file...\n");
        ret = ISFS_CreateFile(filepath, attr, 3, 3, 3);
        if (ret < 0) {
            printf("Error! ISFS_CreateFile (ret = %d)\n", ret);
            out = -1;
            goto cleanup;
        }

        return ISFS_WriteFileFromArray(filepath, filearray, array_size, ownerID, groupID, attr, own_perm, group_perm, other_perm);

    } else	if (ret <= 0) {
        printf("Error! ISFS_Open WRITE (ret = %d)\n", ret);
        out = -1;
        goto cleanup;
    }

    fd = ret;

    ret = ISFS_Seek(fd, 0, 0);
    if (ret < 0) {
        printf("Error! ISFS_Seek (ret = %d)\n", ret);
        out = -1;
        goto cleanup;
    }

    ret = ISFS_Write(fd, filearray, array_size);
    if (ret < 0) {
        printf("Error! ISFS_Write (ret = %d)\n", ret);
        out = -1;
        goto cleanup;
    }

    if (ret != array_size) {
        printf("Filesize is wrong! Wrote:%u Expect:%u", filestats.file_length, array_size);
        out = -1;
    }

    ret = ISFS_Close(fd);
    if (ret < 0) {
        printf("Error! ISFS_Close (ret = %d)\n", ret);
        return -1;
    }
    fd = 0;

    /*
    ret = ISFS_Open(filepath, ISFS_OPEN_READ);
    if (ret <= 0) {
    	printf("Error! ISFS_Open READ (ret = %d)\n", ret);
    	out = -1;
    	goto cleanup;
    }
    fd = ret;

    ret = ISFS_GetFileStats(fd, &filestats);
    if (ret < 0)
    {
    	printf("Error! ISFS_GetFileStats (ret = %d)\n", ret);
    	out = -1;
    	goto cleanup;
    }

    ret = ISFS_Close(fd);
    if (ret < 0)
    {
    	printf("Error! ISFS_Close (ret = %d)\n", ret);
    	return -1;
    }
    fd = 0;
    */
    ret = ISFS_GetAttr(filepath, &realownid, &realgroupid, &realattr, &realownperm, &realgroupperm, &realotherperm);
    if (ret < 0) {
        printf("Error! ISFS_GetAttr (ret = %d)\n", ret);
        out = -1;
    }

    if (realownid != ownerID) {
        ret = ES_GetTitleID(&currentTid);
        if (ret) {
            printf("Fail GetTitleID %d", ret);
            if (WaitKey(WPAD_BUTTON_A | WPAD_BUTTON_B) & WPAD_BUTTON_B) goto cleanup;
        }
        ret = ES_SetUID(getUIDTitleID(ownerID));
        if (ret) {
            printf("Couldn't set OwnerID, using current owner ID\n");
            if (WaitKey(WPAD_BUTTON_A | WPAD_BUTTON_B) & WPAD_BUTTON_B) goto cleanup;
            ownerID = realownid;
        }
    }

    ret = ISFS_SetAttr(filepath, ownerID, groupID, attr, own_perm, group_perm, other_perm);
    if (ret < 0) {
        printf("Error! ISFS_SetAttr (ret = %d)\n", ret);
        out = -1;
        goto cleanup;
    }

    if (realownid != ownerID) 
	{
        ret = ES_SetUID(currentTid);
        if (ret) 
		{
            printf("Fail SetUID %d", ret);
            if (WaitKey(WPAD_BUTTON_A | WPAD_BUTTON_B) & WPAD_BUTTON_B) goto cleanup;
        }
    }

cleanup:
    if (fd) {
        ret = ISFS_Close(fd);
        if (ret < 0) {
            printf("Error! ISFS_Close (ret = %d)\n", ret);
            return -1;
        }
    }
    return out;

}