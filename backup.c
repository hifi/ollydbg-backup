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

typedef void (*cb1)(void *, size_t, void *);
typedef void (*cb2)(int, void *);

struct csv_input {
    int labels;
    int comments;
    int index;
    char address[9];
    char label[256];
    char comment[256];
    rva_t *rvas;
};

static void csv_value(void *buf, size_t len, struct csv_input *data)
{
    char str[1024];

    if (len > sizeof(str) - 1)
        len = sizeof(str) - 1;

    memcpy(str, buf, len);
    str[len] = '\0';

    if (data->index == 0)
        strncpy(data->address, str, sizeof(data->address) - 1);

    if (data->index == 1)
        strncpy(data->label, str, sizeof(data->label) - 1);

    if (data->index == 2)
        strncpy(data->comment, str, sizeof(data->comment) - 1);

    data->index++;
}

static void csv_eol(int x, struct csv_input *data)
{
    long int address = strtol(data->address, NULL, 16);

    if (strcmp(data->address, "RVA") != 0 && (data->label[0] || data->comment[0])) {
        rva_t *rva = malloc(sizeof(rva_t));
        rva->address = address;
        strcpy(rva->label, data->label);
        strcpy(rva->comment, data->comment);

        LIST_INSERT(data->rvas, rva);

        if (data->label[0]) {
            data->labels++;
        }

        if (data->comment[0]) {
            data->comments++;
        }
    }

    data->index = 0;
    memset(data->address, 0, sizeof data->address);
    memset(data->label, 0, sizeof data->label);
    memset(data->comment, 0, sizeof data->comment);
}

rva_t *backup_load(const char *filename, char *message)
{
    char row[2048];
    struct csv_input input;
    struct csv_parser p;
    csv_init(&p, 0);

    memset(&input, 0, sizeof input);

    FILE *fh = fopen(filename, "rb");
    if (!fh) {
        sprintf(message, "Failed to open %s for reading", filename);
        return NULL;
    }

    while (fgets(row, sizeof row, fh)) {
        csv_parse(&p, row, strlen(row), (cb1)csv_value, (cb2)csv_eol, &input);
    }

    csv_fini(&p, (cb1)csv_value, (cb2)csv_eol, NULL);
    csv_free(&p);

    fclose(fh);

    if (input.rvas == NULL)
    {
        sprintf(message, "File %s didn't have any labels or comments", filename);
        return NULL;
    }

    sprintf(message, "Loaded %d labels and %d comments from %s", input.labels, input.comments, filename);
    return input.rvas;
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
