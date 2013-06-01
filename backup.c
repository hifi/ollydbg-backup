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
#include <stdio.h>
#include <time.h>
#include "libcsv/csv.h"

#define _MSC_VER
#ifdef __CHAR_UNSIGNED__
    #define _CHAR_UNSIGNED
#endif

#include "plugin.h"

#ifdef _DEBUG
    #define dprintf(...) fprintf(stderr, __VA_ARGS__)
#else
    #define dprintf(...)
#endif

#define lprintf(...) \
    Infoline(__VA_ARGS__); Addtolist(0, 0, __VA_ARGS__)

void LoadFromFile(t_module *module, const char *filename);
void SaveToFile(t_module *module, const char *filename);

int _export cdecl ODBG_Plugininit(int ollydbgversion, HWND hw, ulong *features)
{
    if (ollydbgversion < 110) {
        Addtolist(0, 0, "OllyDbg too old, backup manager not initialized.");
        return -1;
    }

    Addtolist(0, 0, "Backup manager loaded.");
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
            "1 S&ave to MODULE_YYYY-MM-DD_HH:MM:SS.csv,"
            "2 &Load MODULE.csv,"
            "3 L&oad...,"
        );
        return 1;
    }

    return 0;
}

void _export cdecl ODBG_Pluginaction(int origin, int action, void *item)
{
    if (origin == PM_MAIN) {

        t_module *module = Findmodule(Plugingetvalue(VAL_MAINBASE));

        if (!module) {
            Flash("No program loaded");
            return;
        }

        switch (action) {
            case 0:
            {
                char buf[PATH_MAX];
                strcpy(buf, module->path);

                char *last_stop = strchr(buf, '.');
                if (last_stop) {
                    *last_stop= '\0';
                    strcat(buf, ".csv");

                    SaveToFile(module, buf);
                }

                break;
            }

            case 1:
            {
                char buf[PATH_MAX];
                strcpy(buf, module->path);

                char *last_stop = strchr(buf, '.');
                if (last_stop) {
                    *last_stop= '\0';

                    time_t now = time(NULL);
                    char tbuf[32];
                    struct tm *loctime = localtime(&now);

                    strftime(tbuf, sizeof tbuf, "-%Y%m%d_%H%M%S.csv", loctime);
                    strcat(buf, tbuf);

                    SaveToFile(module, buf);
                }

                break;
            }

            case 2:
            {
                char buf[PATH_MAX];
                strcpy(buf, module->path);

                char *last_stop = strchr(buf, '.');
                if (last_stop) {
                    *last_stop= '\0';
                    strcat(buf, ".csv");

                    LoadFromFile(module, buf);
                }

                break;
            }

            case 3:
            {
                char buf[PATH_MAX] = { '\0' };

                if (Browsefilename("Select a CSV file...", buf, ".csv;*.txt", 0) == TRUE) {
                    LoadFromFile(module, buf);
                }

                break;
            }
        }

    }
}

typedef void (*cb1)(void *, size_t, void *);
typedef void (*cb2)(int, void *);

struct csv_input {
    int labels;
    int comments;
    int index;
    char rva[9];
    char label[TEXTLEN];
    char comment[TEXTLEN];
    t_module *module;
};

void csv_value(void *buf, size_t len, struct csv_input *data)
{
    char str[1024];

    if (len > sizeof(str) - 1)
        len = sizeof(str) - 1;

    memcpy(str, buf, len);
    str[len] = '\0';

    if (data->index == 0)
        strncpy(data->rva, str, sizeof(data->rva) - 1);

    if (data->index == 1)
        strncpy(data->label, str, sizeof(data->label) - 1);

    if (data->index == 2)
        strncpy(data->comment, str, sizeof(data->comment) - 1);

    data->index++;
}

void csv_eol(int x, struct csv_input *data)
{
    long int rva = strtol(data->rva, NULL, 16);

    if (data->label[0]) {
        Quickinsertname(data->module->base + rva, NM_LABEL, data->label);
        data->labels++;
    }

    if (data->comment[0]) {
        Quickinsertname(data->module->base + rva, NM_COMMENT, data->comment);
        data->comments++;
    }

    data->index = 0;
    memset(data->rva, 0, sizeof data->rva);
    memset(data->label, 0, sizeof data->label);
    memset(data->comment, 0, sizeof data->comment);
}

void LoadFromFile(t_module *module, const char *filename)
{
    char row[2048];
    struct csv_input input;
    struct csv_parser p;
    csv_init(&p, 0);

    memset(&input, 0, sizeof input);
    input.module = module;

    FILE *fh = fopen(filename, "rb");
    if (!fh) {
        lprintf("Failed to open %s for reading", filename);
        return;
    }

    while (fgets(row, sizeof row, fh)) {
        csv_parse(&p, row, strlen(row), (cb1)csv_value, (cb2)csv_eol, &input);
    }

    csv_fini(&p, (cb1)csv_value, (cb2)csv_eol, NULL);
    csv_free(&p);

    fclose(fh);

    Mergequicknames();

    lprintf("Loaded %d labels and %d comments from %s", input.labels, input.comments, filename);
}

void SaveToFile(t_module *module, const char *filename)
{
    FILE *fh = fopen(filename, "wb");

    if (!fh) {
        Flash("File %s could not be opened for writing", filename);
        return;
    }

    fprintf(fh, "RVA,label,comment\r\n");

    char label[TEXTLEN];
    char comment[TEXTLEN];
    unsigned int end = module->base + module->size;

    int labels = 0;
    int comments = 0;

    for (unsigned int addr = module->base; addr < end; addr++) {

        label[0] = '\0';
        comment[0] = '\0';

        if (Findname(addr, NM_LABEL, label)) {
            labels++;
        }

        if (Findname(addr, NM_COMMENT, comment)) {
            comments++;
        }

        if (label[0] || comment[0]) {
            fprintf(fh, "%08X,", (unsigned int)(addr - module->base));

            if (strchr(label, ',') || strchr(label, '"'))
                csv_fwrite(fh, label, strlen(label));
            else
                fwrite(label, strlen(label), 1, fh);

            fwrite(",", 1, 1, fh);

            if (strchr(comment, ',') || strchr(comment, '"'))
                csv_fwrite(fh, comment, strlen(comment));
            else
                fwrite(comment, strlen(comment), 1, fh);

            fwrite("\r\n", 2, 1, fh);
        }
    }

    fclose(fh);

    lprintf("Saved %d labels and %d comments to %s", labels, comments, filename);
}
