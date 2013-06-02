/*
 * Copyright (c) 2013 Toni Spets <toni.spets@iki.fi>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <windows.h>
#include "backup.h"
#include "list.h"

#define _MSC_VER
#ifdef __CHAR_UNSIGNED__
    #define _CHAR_UNSIGNED
#endif

// you dare ask
#define extern
#define Addtolist (*Addtolist)
#define Plugingetvalue (*Plugingetvalue)
#define Flash (*Flash)
#define Browsefilename (*Browsefilename)
#define Quickinsertname (*Quickinsertname)
#define Mergequicknames (*Mergequicknames)
#define Infoline (*Infoline)
#define Findname (*Findname)
#define Findmodule (*Findmodule)

#include "v110.h"

#undef Addtolist
#undef Plugingetvalue
#undef Flash
#undef Browsefilename
#undef Quickinsertname
#undef Mergequicknames
#undef Infoline
#undef Findname
#undef Findmodule

static void LoadFromFile(t_module *module, const char *filename);
static void SaveToFile(t_module *module, const char *filename);

static bool initialized = false;

int _export cdecl ODBG_Plugininit(int ollydbgversion, HWND hw, ulong *features)
{
    if (ollydbgversion < 110)
        return -1;

    HANDLE handle = GetModuleHandle(NULL);

    Addtolist       = (void *)GetProcAddress(handle, "_Addtolist");
    Plugingetvalue  = (void *)GetProcAddress(handle, "_Plugingetvalue");
    Findmodule      = (void *)GetProcAddress(handle, "_Findmodule");
    Flash           = (void *)GetProcAddress(handle, "_Flash");
    Browsefilename  = (void *)GetProcAddress(handle, "_Browsefilename");
    Quickinsertname = (void *)GetProcAddress(handle, "_Quickinsertname");
    Mergequicknames = (void *)GetProcAddress(handle, "_Mergequicknames");
    Infoline        = (void *)GetProcAddress(handle, "_Infoline");
    Findname        = (void *)GetProcAddress(handle, "_Findname");

    if (!Addtolist || !Plugingetvalue || !Findmodule || !Flash
            || !Browsefilename  || !Quickinsertname || !Mergequicknames
            || !Infoline || !Findname)
        return -1;

    initialized = true;

    return 0;
}

int _export cdecl ODBG_Plugindata(char *shortname)
{
    strcpy(shortname, "Backup");
    return PLUGIN_VERSION;
}

int _export cdecl ODBG_Pluginmenu(int origin, char data[4096], void *item)
{
    if (origin == PM_MAIN) {
        strcpy(data,
            "0 &Save to MODULE.csv,"
            "1 S&ave to MODULE-YYYYMMDD_HHMMSS.csv,"
            "2 &Load MODULE.csv,"
            "3 L&oad...,"
        );
        return 1;
    }

    return 0;
}

void _export cdecl ODBG_Pluginaction(int origin, int action, void *item)
{
    if (!initialized)
        return;

    if (origin == PM_MAIN) {

        t_module *module = Findmodule(Plugingetvalue(VAL_MAINBASE));

        if (!module) {
            Flash("No program loaded");
            return;
        }

        char buf[MAX_PATH];
        strcpy_s(buf, sizeof buf, module->path);

        char *last_stop = strrchr(buf, '.');
        if (!last_stop) {
            return;
        }

        *last_stop= '\0';

        switch (action) {
            case 0:
            {
                strcat_s(buf, sizeof buf, ".csv");
                SaveToFile(module, buf);
                break;
            }

            case 1:
            {
                char tbuf[32];

                GetDateFormat(LOCALE_USER_DEFAULT, 0, NULL, "-yyyy''MM''dd", tbuf, sizeof tbuf);
                strcat_s(buf, sizeof buf, tbuf);

                GetTimeFormat(LOCALE_USER_DEFAULT, TIME_FORCE24HOURFORMAT, NULL, "_hh''mm''ss", tbuf, sizeof tbuf);
                strcat_s(buf, sizeof buf, tbuf);

                strcat_s(buf, sizeof buf, ".csv");
                SaveToFile(module, buf);
                break;
            }

            case 2:
            {
                strcat_s(buf, sizeof buf, ".csv");
                LoadFromFile(module, buf);
                break;
            }

            case 3:
            {
                strcat_s(buf, sizeof buf, ".csv");
                if (Browsefilename("Select a CSV file...", buf, ".csv;*.txt", 0) == TRUE) {
                    LoadFromFile(module, buf);
                }
                break;
            }
        }

    }
}

static void LoadFromFile(t_module *module, const char *filename)
{
    char message[1024];
    rva_t *rvas = backup_load(filename, message);

    if (rvas == NULL) {
        Flash(message);
        return;
    }

    LIST_FOREACH (rvas, rva_t, rva) {
        if (rva->label[0]) {
            Quickinsertname(module->base + rva->address, NM_LABEL, rva->label);
        }
        if (rva->comment[0]) {
            Quickinsertname(module->base + rva->address, NM_COMMENT, rva->comment);
        }
    }

    LIST_FREE(rvas);

    Mergequicknames();

    Infoline(message);
}

static void SaveToFile(t_module *module, const char *filename)
{
    unsigned int end = module->base + module->size;

    char label[TEXTLEN];
    char comment[TEXTLEN];

    rva_t *rvas = NULL;

    for (unsigned int address = end; address > module->base; address--) {

        label[0] = '\0';
        comment[0] = '\0';

        Findname(address, NM_LABEL, label);
        Findname(address, NM_COMMENT, comment);

        if (label[0] || comment[0]) {
            rva_t *rva = malloc(sizeof(rva_t));
            rva->address = address - module->base;
            strcpy_s(rva->label, sizeof(rva->label), label);
            strcpy_s(rva->comment, sizeof(rva->label), comment);
            LIST_INSERT(rvas, rva);
        }
    }

    char message[1024];
    if (backup_save(filename, rvas, message)) {
        Infoline(message);
    } else {
        Flash(message);
    }
}
