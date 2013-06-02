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

#define UNICODE
#define _UNICODE

#include <windows.h>
#include "backup.h"
#include "list.h"

// you dare ask
#define extern
#define Findmainmodule (*Findmainmodule)
#define Browsefilename (*Browsefilename)
#define Flash (*Flash)
#define Unicodetoutf (*Unicodetoutf)
#define Utftounicode (*Utftounicode)
#define FindnameW (*FindnameW)
#define Info (*Info)
#define QuickinsertnameW (*QuickinsertnameW)
#define Mergequickdata (*Mergequickdata)

#include "v201.h"

#undef Findmainmodule
#undef Browsefilename
#undef Flash
#undef Unicodetoutf
#undef Utftounicode
#undef FindnameW
#undef Info
#undef QuickinsertnameW
#undef Mergequickdata

#define PLUGINNAME      L"Backup"
#define PLUGINDESC      L"Backup and restore current labels and comments."

static void LoadFromFile(t_module *module, const wchar_t *filename);
static void SaveToFile(t_module *module, const wchar_t *filename);

static bool initialized = false;

extc int _export cdecl ODBG2_Pluginquery(int ollydbgversion, ulong *features, wchar_t pluginname[SHORTNAME], wchar_t pluginversion[SHORTNAME])
{
    if (ollydbgversion < 201)
        return 0;

    HANDLE handle = GetModuleHandle(NULL);

    Findmainmodule      = (void *)GetProcAddress(handle, "Findmainmodule");
    Browsefilename      = (void *)GetProcAddress(handle, "Browsefilename");
    Flash               = (void *)GetProcAddress(handle, "Flash");
    Unicodetoutf        = (void *)GetProcAddress(handle, "Unicodetoutf");
    Utftounicode        = (void *)GetProcAddress(handle, "Utftounicode");
    FindnameW           = (void *)GetProcAddress(handle, "FindnameW");
    Info                = (void *)GetProcAddress(handle, "Info");
    QuickinsertnameW    = (void *)GetProcAddress(handle, "QuickinsertnameW");
    Mergequickdata      = (void *)GetProcAddress(handle, "Mergequickdata");

    if (!Findmainmodule || !Browsefilename || !Flash || !Unicodetoutf
            || !Utftounicode || !FindnameW || !Info || !QuickinsertnameW
            || !Mergequickdata)
        return 0;

    initialized = true;

    wcscpy_s(pluginname, SHORTNAME, PLUGINNAME);
    wcscpy_s(pluginversion, SHORTNAME, REV);
    return PLUGIN_VERSION;
}

static int menucb(t_table *pt, wchar_t *name, ulong index, int mode)
{
    if (!initialized)
        return MENU_GRAYED;

    t_module *module = Findmainmodule();

    if (module == NULL)
        return MENU_GRAYED;

    if (mode == MENU_VERIFY)
        return MENU_NORMAL;

    if (mode == MENU_EXECUTE) {

        wchar_t buf[MAXPATH];
        wcscpy_s(buf, _countof(buf), module->path);

        wchar_t *last_stop = wcsrchr(buf, L'.');

        if (!last_stop) {
            return MENU_ABSENT;
        }

        *last_stop= L'\0';

        switch (index) {
            case 0:
            {
                wcscat_s(buf, _countof(buf), L".csv");
                SaveToFile(module, buf);
                break;
            }

            case 1:
            {
                wchar_t tbuf[32];

                GetDateFormatW(LOCALE_USER_DEFAULT, 0, NULL, L"-yyyy''MM''dd", tbuf, _countof(tbuf));
                wcscat_s(buf, _countof(buf), tbuf);

                GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_FORCE24HOURFORMAT, NULL, L"_hh''mm''ss", tbuf, sizeof tbuf);
                GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_FORCE24HOURFORMAT, NULL, L"_hh''mm''ss", tbuf, _countof(tbuf));
                wcscat_s(buf, _countof(buf), tbuf);

                wcscat_s(buf, _countof(buf), L".csv");
                SaveToFile(module, buf);
                break;
            }

            case 2:
            {
                wcscat_s(buf, _countof(buf), L".csv");
                LoadFromFile(module, buf);
                break;
            }

            case 3:
            {
                wcscat_s(buf, _countof(buf), L".csv");
                if (Browsefilename(L"Select a CSV file...", buf, NULL, NULL, L".csv", NULL, 0)) {
                    LoadFromFile(module, buf);
                }
                break;
            }
        }
    }

    return MENU_ABSENT;
}

t_menu pluginmenu[] = {
    {
        L"&Save to MODULE.csv",
        NULL,
        K_NONE,
        menucb,
        NULL,
        { 0 }
    },
    {
        L"S&ave to MODULE-YYYYMMDD_HHMMSS.csv",
        NULL,
        K_NONE,
        menucb,
        NULL,
        { 1 }
    },
    {
        L"&Load MODULE.csv",
        NULL,
        K_NONE,
        menucb,
        NULL,
        { 2 }
    },
    {
        L"L&oad...",
        NULL,
        K_NONE,
        menucb,
        NULL,
        { 3 }
    },
    {
        NULL,
        NULL,
        K_NONE,
        NULL,
        NULL,
        { 0 }
    }
};

t_menu mainmenu[] = {
    {
        L"&" PLUGINNAME,
        PLUGINDESC,
        K_NONE,
        NULL,
        pluginmenu,
        { 0 }
    },
    {
        NULL,
        NULL,
        K_NONE,
        NULL,
        NULL,
        { 0 }
    }
};

extc t_menu _export cdecl *ODBG2_Pluginmenu(wchar_t *type)
{
    if (lstrcmp(type, PWM_MAIN) == 0) {
        return mainmenu;
    }

    return NULL;
}

static void LoadFromFile(t_module *module, const wchar_t *filename)
{
    wchar_t unicode[TEXTLEN];
    char utf[TEXTLEN];

    char message[1024];

    Unicodetoutf(filename, wcslen(filename), utf, _countof(utf));

    rva_t *rvas = backup_load(utf, message);

    if (rvas == NULL) {
        Utftounicode(message, strlen(message), unicode, _countof(unicode));
        Flash(unicode);
        return;
    }

    LIST_FOREACH (rvas, rva_t, rva) {
        if (rva->label[0]) {
            Utftounicode(rva->label, strlen(rva->label), unicode, _countof(unicode));
            QuickinsertnameW(module->base + rva->address, NM_LABEL, unicode);
        }
        if (rva->comment[0]) {
            Utftounicode(rva->comment, strlen(rva->comment), unicode, _countof(unicode));
            QuickinsertnameW(module->base + rva->address, NM_COMMENT, unicode);
        }
    }

    LIST_FREE(rvas);

    Mergequickdata();

    Utftounicode(message, strlen(message), unicode, _countof(unicode));
    Info(unicode);
}

static void SaveToFile(t_module *module, const wchar_t *filename)
{
    unsigned int end = module->base + module->size;

    wchar_t label[TEXTLEN];
    wchar_t comment[TEXTLEN];
    wchar_t unicode[TEXTLEN];
    char utf[TEXTLEN];

    rva_t *rvas = NULL;

    for (unsigned int address = end; address > module->base; address--) {

        label[0] = L'\0';
        comment[0] = L'\0';

        FindnameW(address, NM_LABEL, label, _countof(label));
        FindnameW(address, NM_COMMENT, comment, _countof(label));

        if (label[0] || comment[0]) {
            rva_t *rva = malloc(sizeof(rva_t));
            rva->address = address - module->base;
            Unicodetoutf(label, _countof(label), rva->label, sizeof(rva->label));
            Unicodetoutf(comment, _countof(comment), rva->comment, sizeof(rva->comment));
            LIST_INSERT(rvas, rva);
        }
    }

    char message[1024];
    Unicodetoutf(filename, wcslen(filename), utf, _countof(utf));

    if (backup_save(utf, rvas, message)) {
        Utftounicode(message, strlen(message), unicode, _countof(unicode));
        Info(unicode);
    } else {
        Utftounicode(message, strlen(message), unicode, _countof(unicode));
        Flash(unicode);
    }
}
