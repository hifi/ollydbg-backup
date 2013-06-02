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
#include "backup.h"
#include "list.h"
#include "libcsv/csv.h"

BOOL WINAPI DllMainCRTStartup(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) { return TRUE; }

typedef void (*cb1)(void *, size_t, void *);
typedef void (*cb2)(int, void *);

struct csv_data {
    int labels;
    int comments;
    int index;
    char address[9];
    char label[256];
    char comment[256];
    rva_t *rvas;
};

static void csv_value(void *rbuf, size_t len, struct csv_data *data)
{
    char buf[256];

    if (len > sizeof(buf) - 1)
        len = sizeof(buf) - 1;
    memcpy(buf, rbuf, len);
    buf[len] = '\0';

    if (data->index == 0)
        strcpy_s(data->address, sizeof(data->address), buf);

    if (data->index == 1)
        strcpy_s(data->label, sizeof(data->label), buf);

    if (data->index == 2)
        strcpy_s(data->comment, sizeof(data->comment), buf);

    data->index++;
}

static void csv_eol(int x, struct csv_data *data)
{
    long int address = strtol(data->address, NULL, 16);

    if (strcmp(data->address, "RVA") != 0 && (data->label[0] || data->comment[0])) {
        rva_t *rva = malloc(sizeof(rva_t));

        rva->address = address;
        strcpy_s(rva->label, sizeof(rva->label), data->label);
        strcpy_s(rva->comment, sizeof(rva->label), data->comment);

        LIST_INSERT(data->rvas, rva);

        if (data->label[0]) {
            data->labels++;
        }

        if (data->comment[0]) {
            data->comments++;
        }
    }

    data->index = 0;
    data->address[0] = '\0';
    data->label[0] = '\0';
    data->comment[0] = '\0';
}

rva_t *backup_load(const char *filename, char *message)
{
    char row[2048];
    struct csv_data data;
    struct csv_parser p;
    csv_init(&p, 0);

    memset(&data, 0, sizeof data);

    FILE *fh = fopen(filename, "rb");
    if (!fh) {
        sprintf(message, "Failed to open %s for reading", filename);
        return NULL;
    }

    while (fgets(row, sizeof row, fh)) {
        csv_parse(&p, row, strlen(row), (cb1)csv_value, (cb2)csv_eol, &data);
    }

    csv_fini(&p, (cb1)csv_value, (cb2)csv_eol, NULL);
    csv_free(&p);

    fclose(fh);

    if (data.rvas == NULL)
    {
        sprintf(message, "File %s didn't have any labels or comments", filename);
        return NULL;
    }

    sprintf(message, "Loaded %d labels and %d comments from %s", data.labels, data.comments, filename);
    return data.rvas;
}

bool backup_save(const char *filename, rva_t *rvas, char *message)
{
    FILE *fh = fopen(filename, "wb");

    if (!fh) {
        sprintf(message, "File %s could not be opened for writing", filename);
        return false;
    }

    if (!rvas) {
        strcpy(message, "Nothing to save");
        return false;
    }

    fprintf(fh, "RVA,label,comment\r\n");

    int labels = 0;
    int comments = 0;

    LIST_FOREACH (rvas, rva_t, rva) {
        if (rva->label[0]) {
            labels++;
        }

        if (rva->comment[0]) {
            comments++;
        }

        fprintf(fh, "%08X,", rva->address);

        if (strchr(rva->label, ',') || strchr(rva->label, '"'))
            csv_fwrite(fh, rva->label, strlen(rva->label));
        else
            fwrite(rva->label, strlen(rva->label), 1, fh);

        fwrite(",", 1, 1, fh);

        if (strchr(rva->comment, ',') || strchr(rva->comment, '"'))
            csv_fwrite(fh, rva->comment, strlen(rva->comment));
        else
            fwrite(rva->comment, strlen(rva->comment), 1, fh);

        fwrite("\r\n", 2, 1, fh);
    }

    fclose(fh);

    sprintf(message, "Saved %d labels and %d comments to %s", labels, comments, filename);
    return true;
}
