#include "backend_strings.h"
#include "backend.h"

#define CXX_STRING_HASH_TABLE_SIZE (1024 * 1024)

struct backend_string {
    uint32_t hash;
    uint32_t id;
    uint32_t length;
    char string[0];
};

// http://www.isthe.com/chongo/tech/comp/fnv/#FNV-1a
static uint32_t fnv1a_hash(const void* data, uint32_t size)
{
    const uint8_t* bytes = data;
    uint32_t hash = 2166136261;
    for (uint32_t i = 0; i < size; i++)
    {
        hash ^= bytes[i];
        hash *= 16777619;
    }
    return hash;
}

static void backend_strings_expand(backend_strings* strings)
{
    uint32_t new_count = strings->count * 2;
    backend_string** new_table = backend_os_alloc(new_count * sizeof(void*));

    for (size_t i = 0; i < strings->count; i++)
    {
        if (strings->table[i])
        {
            uint32_t index = strings->table[i]->hash % new_count;

            while (new_table[index] != NULL)
            {
                if (++index == new_count)
                {
                    index = 0;
                }
            }
            new_table[index] = strings->table[i];
        }
    }

    backend_os_free(strings->table);
    strings->count = new_count;
    strings->table = new_table;
}

void backend_strings_init(backend_strings* strings)
{
    strings->count = CXX_STRING_HASH_TABLE_SIZE / sizeof(void*);
    strings->used = 0;
    strings->table = backend_os_alloc(strings->count * sizeof(void*));
}

uint32_t backend_strings_get(backend_strings* strings, const wchar_t* wstring, int wlength)
{
    uint32_t length = backend_utf8_length(wstring, wlength);
    char* string = _alloca(length);
    length = backend_utf8_convert(string, length, wstring, wlength);

    backend_string* result;

    uint32_t hash = fnv1a_hash(string, length);
    uint32_t index = hash % strings->count;

    for (;;)
    {
        result = strings->table[index];
        if (result == NULL)
        {
            // 75%
            if (strings->used * 100 >= strings->count * 75)
            {
                backend_strings_expand(strings);
                index = hash % strings->count;
                continue;
            }

            backend_reply_new_string(string, (uint32_t)length);

            result = backend_alloc(sizeof(backend_string) + length);
            result->hash = hash;
            result->id = ++strings->used;
            result->length = length;
            backend_copy(result->string, string, length);
            strings->table[index] = result;
            break;
        }

        if (result->hash == hash && result->length == length && backend_equal(result->string, string, length))
        {
            break;
        }

        if (++index == strings->count)
        {
            index = 0;
        }
    }

    return result->id;
}
