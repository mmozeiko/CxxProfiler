#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct backend_string backend_string;

typedef struct backend_strings {
    uint32_t count;
    uint32_t used;
    backend_string** table;
} backend_strings;

void backend_strings_init(backend_strings* strings);
uint32_t backend_strings_get(backend_strings* strings, const wchar_t* wstring, int wlength);
