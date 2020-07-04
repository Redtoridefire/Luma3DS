/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2020 Aurora Wright, TuxSH
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

#include <3ds.h>
#include <3ds/os.h>
#include "menus.h"
#include "menu.h"
#include "draw.h"
#include "menus/process_list.h"
#include "menus/n3ds.h"
#include "menus/debugger.h"
#include "menus/miscellaneous.h"
#include "menus/sysconfig.h"
#include "menus/screen_filters.h"
#include "plgloader.h"
#include "ifile.h"
#include "memory.h"
#include "fmt.h"
#include "process_patches.h"

Menu rosalinaMenu = {
    "Rosalina menu",
    {
        { "Take screenshot", METHOD, .method = &RosalinaMenu_TakeScreenshot },
        { "Change screen brightness", METHOD, .method = &RosalinaMenu_ChangeScreenBrightness },
        { "", METHOD, .method = PluginLoader__MenuCallback},
        { "Cheats...", METHOD, .method = &RosalinaMenu_Cheats },
        { "Process list", METHOD, .method = &RosalinaMenu_ProcessList },
        { "Debugger options...", MENU, .menu = &debuggerMenu },
        { "System configuration...", MENU, .menu = &sysconfigMenu },
        { "Screen filters...", MENU, .menu = &screenFiltersMenu },
        { "New 3DS menu...", MENU, .menu = &N3DSMenu, .visibility = &menuCheckN3ds },
        { "Miscellaneous options...", MENU, .menu = &miscellaneousMenu },
        { "Power off", METHOD, .method = &RosalinaMenu_PowerOff },
        { "Reboot", METHOD, .method = &RosalinaMenu_Reboot },
        { "Credits", METHOD, .method = &RosalinaMenu_ShowCredits },
    }
};

bool rosalinaMenuShouldShowDebugInfo(void)
{
    return true;
}

void RosalinaMenu_ShowDebugInfo(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    char memoryMap[512];
    formatMemoryMapOfProcess(memoryMap, 511, CUR_PROCESS_HANDLE);

    s64 kextAddrSize;
    svcGetSystemInfo(&kextAddrSize, 0x10000, 0x300);
    u32 kextPa = (u32)((u64)kextAddrSize >> 32);
    u32 kextSize = (u32)kextAddrSize;

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Rosalina -- Debug info");

        u32 posY = Draw_DrawString(10, 30, COLOR_WHITE, memoryMap);
        Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Kernel ext PA: %08lx - %08lx\n", kextPa, kextPa + kextSize);
        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & KEY_B) && !menuShouldExit);
}

void RosalinaMenu_ShowCredits(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Rosalina -- Luma3DS credits");

        u32 posY = Draw_DrawString(10, 30, COLOR_WHITE, "Luma3DS (c) 2016-2020 AuroraWright, TuxSH") + SPACING_Y;

        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "3DSX loading code by fincs");
        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "Networking code & basic GDB functionality by Stary");
        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "InputRedirection by Stary (PoC by ShinyQuagsire)");

        posY += 2 * SPACING_Y;

        Draw_DrawString(10, posY, COLOR_WHITE,
            (
                "Special thanks to:\n"
                "  Bond697, WinterMute, piepie62, yifanlu\n"
                "  Luma3DS contributors, ctrulib contributors,\n"
                "  other people"
            ));

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & KEY_B) && !menuShouldExit);
}

void RosalinaMenu_Reboot(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Rosalina menu");
        Draw_DrawString(10, 30, COLOR_WHITE, "Press A to reboot, press B to go back.");
        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & KEY_A)
        {
            menuLeave();
            APT_HardwareResetAsync();
            return;
        } else if(pressed & KEY_B)
            return;
    }
    while(!menuShouldExit);
}

static u32 gspPatchAddrN3ds, gspPatchValuesN3ds[2];
static bool gspPatchDoneN3ds;

static Result RosalinaMenu_PatchN3dsGspForBrightness(u32 size)
{
    u32 *off = (u32 *)0x00100000;
    u32 *end = (u32 *)(0x00100000 + size);

    for (; off < end && (off[0] != 0xE92D4030 || off[1] != 0xE1A04000 || off[2] != 0xE2805C01 || off[3] != 0xE5D0018C); off++);

    if (off >= end) {
        return -1;
    }

    gspPatchAddrN3ds = (u32)off;
    gspPatchValuesN3ds[0] = off[26];
    gspPatchValuesN3ds[1] = off[50];

    // NOP brightness changing in GSP
    off[26] = 0xE1A00000;
    off[50] = 0xE1A00000;

    return 0;
}
static Result RosalinaMenu_RevertN3dsGspPatch(u32 size)
{
    (void)size;

    u32 *off = (u32 *)gspPatchAddrN3ds;
    off[26] = gspPatchValuesN3ds[0];
    off[50] = gspPatchValuesN3ds[1];

    return 0;
}

void RosalinaMenu_ChangeScreenBrightness(void)
{
    Result patchResult = 0;
    if (isN3DS && !gspPatchDoneN3ds)
    {
        patchResult = PatchProcessByName("gsp", RosalinaMenu_PatchN3dsGspForBrightness);
        gspPatchDoneN3ds = R_SUCCEEDED(patchResult);
    }

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        // Assume the current brightness for both screens are the same.
        s32 brightness = (s32)(LCD_TOP_BRIGHTNESS & 0xFF);

        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Rosalina menu");
        u32 posY = 30;
        posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Current brightness (0..255): %3lu\n\n", brightness);
        if (R_SUCCEEDED(patchResult))
        {
            posY = Draw_DrawString(10, posY, COLOR_WHITE, "Press Up/Down for +-1, Right/Left for +-10.\n");
            posY = Draw_DrawString(10, posY, COLOR_WHITE, "Press Y to revert the GSP patch and exit.\n\n");

            posY = Draw_DrawString(10, posY, COLOR_RED, "WARNING: \n");
            posY = Draw_DrawString(10, posY, COLOR_WHITE, "  * avoid using values far higher than the presets.\n");
            posY = Draw_DrawString(10, posY, COLOR_WHITE, "  * normal brightness mngmt. is now broken on N3DS.\nYou'll need to press Y to revert");
        }
        else
            Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Failed to patch GSP (0x%08lx).", (u32)patchResult);

        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if ((pressed & DIRECTIONAL_KEYS) && R_SUCCEEDED(patchResult))
        {
            if (pressed & KEY_UP)
                brightness += 1;
            else if (pressed & KEY_DOWN)
                brightness -= 1;
            else if (pressed & KEY_RIGHT)
                brightness += 10;
            else if (pressed & KEY_LEFT)
                brightness -= 10;

            brightness = brightness < 0 ? 0 : brightness;
            brightness = brightness > 255 ? 255 : brightness;
            LCD_TOP_BRIGHTNESS = (u32)brightness;
            LCD_BOT_BRIGHTNESS = (u32)brightness;
        }
        else if ((pressed & KEY_Y) && gspPatchDoneN3ds)
        {
            patchResult = PatchProcessByName("gsp", RosalinaMenu_RevertN3dsGspPatch);
            gspPatchDoneN3ds = !R_SUCCEEDED(patchResult);
            return;
        }
        else if (pressed & KEY_B)
            return;
    }
    while (!menuShouldExit);
}

void RosalinaMenu_PowerOff(void) // Soft shutdown.
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Rosalina menu");
        Draw_DrawString(10, 30, COLOR_WHITE, "Press A to power off, press B to go back.");
        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & KEY_A)
        {
            menuLeave();
            srvPublishToSubscriber(0x203, 0);
            return;
        }
        else if(pressed & KEY_B)
            return;
    }
    while(!menuShouldExit);
}


#define TRY(expr) if(R_FAILED(res = (expr))) goto end;

static s64 timeSpentConvertingScreenshot = 0;
static s64 timeSpentWritingScreenshot = 0;

static Result RosalinaMenu_WriteScreenshot(IFile *file, bool top, bool left)
{
    u64 total;
    Result res = 0;
    u32 dimX = top ? 400 : 320;
    u32 lineSize = 3 * dimX;
    u32 remaining = lineSize * 240;
    u8 *framebufferCache = (u8 *)Draw_GetFramebufferCache();
    u8 *framebufferCacheEnd = framebufferCache + Draw_GetFramebufferCacheSize();

    u8 *buf = framebufferCache;
    Draw_CreateBitmapHeader(framebufferCache, dimX, 240);
    buf += 54;

    u32 y = 0;
    // Our buffer might be smaller than the size of the screenshot...
    while (remaining != 0)
    {
        s64 t0 = svcGetSystemTick();
        u32 available = (u32)(framebufferCacheEnd - buf);
        u32 size = available < remaining ? available : remaining;
        u32 nlines = size / lineSize;
        Draw_ConvertFrameBufferLines(buf, y, nlines, top, left);

        s64 t1 = svcGetSystemTick();
        timeSpentConvertingScreenshot += t1 - t0;
        TRY(IFile_Write(file, &total, framebufferCache, (y == 0 ? 54 : 0) + lineSize * nlines, 0)); // don't forget to write the header
        timeSpentWritingScreenshot += svcGetSystemTick() - t1;

        y += nlines;
        remaining -= lineSize * nlines;
        buf = framebufferCache;
    }
    end: return res;
}

void RosalinaMenu_TakeScreenshot(void)
{
    IFile file;
    Result res = 0;

    char filename[64];

    FS_Archive archive;
    FS_ArchiveID archiveId;
    s64 out;
    bool isSdMode;

    timeSpentConvertingScreenshot = 0;
    timeSpentWritingScreenshot = 0;

    if(R_FAILED(svcGetSystemInfo(&out, 0x10000, 0x203))) svcBreak(USERBREAK_ASSERT);
    isSdMode = (bool)out;

    archiveId = isSdMode ? ARCHIVE_SDMC : ARCHIVE_NAND_RW;
    Draw_Lock();
    Draw_RestoreFramebuffer();

    svcFlushEntireDataCache();

    res = FSUSER_OpenArchive(&archive, archiveId, fsMakePath(PATH_EMPTY, ""));
    if(R_SUCCEEDED(res))
    {
        res = FSUSER_CreateDirectory(archive, fsMakePath(PATH_ASCII, "/luma/screenshots"), 0);
        if((u32)res == 0xC82044BE) // directory already exists
            res = 0;
        FSUSER_CloseArchive(archive);
    }

    u32 seconds, minutes, hours, days, year, month;
    u64 milliseconds = osGetTime();
    seconds = milliseconds/1000;
    milliseconds %= 1000;
    minutes = seconds / 60;
    seconds %= 60;
    hours = minutes / 60;
    minutes %= 60;
    days = hours / 24;
    hours %= 24;

    year = 1900; // osGetTime starts in 1900

    while(true)
    {
        bool leapYear = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        u16 daysInYear = leapYear ? 366 : 365;
        if(days >= daysInYear)
        {
            days -= daysInYear;
            ++year;
        }
        else
        {
            static const u8 daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            for(month = 0; month < 12; ++month)
            {
                u8 dim = daysInMonth[month];

                if (month == 1 && leapYear)
                    ++dim;

                if (days >= dim)
                    days -= dim;
                else
                    break;
            }
            break;
        }
    }
    days++;
    month++;

    sprintf(filename, "/luma/screenshots/%04lu-%02lu-%02lu_%02lu-%02lu-%02lu.%03llu_top.bmp", year, month, days, hours, minutes, seconds, milliseconds);
    TRY(IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, filename), FS_OPEN_CREATE | FS_OPEN_WRITE));
    TRY(RosalinaMenu_WriteScreenshot(&file, true, true));
    TRY(IFile_Close(&file));

    sprintf(filename, "/luma/screenshots/%04lu-%02lu-%02lu_%02lu-%02lu-%02lu.%03llu_bot.bmp", year, month, days, hours, minutes, seconds, milliseconds);
    TRY(IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, filename), FS_OPEN_CREATE | FS_OPEN_WRITE));
    TRY(RosalinaMenu_WriteScreenshot(&file, false, true));
    TRY(IFile_Close(&file));

    if((GPU_FB_TOP_FMT & 0x20) && (Draw_GetCurrentFramebufferAddress(true, true) != Draw_GetCurrentFramebufferAddress(true, false)))
    {
        sprintf(filename, "/luma/screenshots/%04lu-%02lu-%02lu_%02lu-%02lu-%02lu.%03llu_top_right.bmp", year, month, days, hours, minutes, seconds, milliseconds);
        TRY(IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, filename), FS_OPEN_CREATE | FS_OPEN_WRITE));
        TRY(RosalinaMenu_WriteScreenshot(&file, true, false));
        TRY(IFile_Close(&file));
    }

end:
    IFile_Close(&file);
    svcFlushEntireDataCache();
    Draw_SetupFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Screenshot");
        if(R_FAILED(res))
            Draw_DrawFormattedString(10, 30, COLOR_WHITE, "Operation failed (0x%08lx).", (u32)res);
        else
        {
            u32 t1 = (u32)(1000 * timeSpentConvertingScreenshot / SYSCLOCK_ARM11);
            u32 t2 = (u32)(1000 * timeSpentWritingScreenshot / SYSCLOCK_ARM11);
            u32 posY = 30;
            posY = Draw_DrawString(10, posY, COLOR_WHITE, "Operation succeeded.\n\n");
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Time spent converting:    %5lums\n", t1);
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Time spent writing files: %5lums\n", t2);
        }

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & KEY_B) && !menuShouldExit);

#undef TRY
}
