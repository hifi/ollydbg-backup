#include <stdbool.h>

typedef struct rva_t {
    unsigned int address;
    char label[256];
    char comment[256];
    struct rva_t *next;
} rva_t;

rva_t *backup_load(const char *filename, char *message);
bool backup_save(const char *filename, rva_t *rvas, char *message);
