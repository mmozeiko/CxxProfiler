#include "backend_symbols.h"
#include "backend_strings.h"
#include "backend_commands.h"
#include "backend.h"

#define WIN32_LEAN_AND_MEAN
#define DBGHELP_TRANSLATE_TCHAR
#include <windows.h>
#include <dbghelp.h>
#include <shlwapi.h>
#include <wchar.h>

#define CXX_SYMBOL_MEMORY_BLOCK_SIZE (64 * 1024)

struct backend_symbol_memory {
    backend_symbol_memory* next;
    uint32_t used;
    uint8_t data[0];
};

static backend_symbol_memory* backend_symbol_memory_create(void)
{
    backend_symbol_memory* mem = backend_os_alloc(CXX_SYMBOL_MEMORY_BLOCK_SIZE);
    mem->next = NULL;
    mem->used = 0;
    return mem;
}

static void backend_symbol_memory_free(backend_symbol_memory* mem)
{
    while (mem)
    {
        backend_symbol_memory* next = mem->next;
        backend_os_free(mem);
        mem = next;
    }
}

static backend_symbol* backend_symbol_memory_alloc(backend_symbol_memory* mem)
{
    uint32_t size = sizeof(backend_symbol);
    uint32_t max_size = CXX_SYMBOL_MEMORY_BLOCK_SIZE - sizeof(*mem);
    while (max_size - mem->used < size)
    {
        if (mem->next == NULL)
        {
            mem->next = backend_symbol_memory_create();
        }
        mem = mem->next;
    }
    void* result = mem->data + mem->used;
    mem->used += size;
    return result;
}

// http://algs4.cs.princeton.edu/32bst/
static backend_symbol* backend_symbol_find(backend_symbol* node, uint64_t address)
{
    if (!node)
    {
        return NULL;
    }

    Assert(node->left == NULL || node->left->address < node->address);
    Assert(node->right == NULL || node->right->address > node->address);

    if (address < node->address)
    {
        return backend_symbol_find(node->left, address);
    }
    else if (address > node->address)
    {
        backend_symbol* result = backend_symbol_find(node->right, address);
        return result ? result : node;
    }
    else
    {
        return node;
    }
}

static uint32_t backend_symbol_height(backend_symbol* node)
{
    return node ? node->height : 0;
}

static void backend_symbol_adjust(backend_symbol* node)
{
    node->height = 1 + Max(backend_symbol_height(node->left), backend_symbol_height(node->right));
}

static backend_symbol* backend_symbol_rotate_right(backend_symbol* node)
{
    backend_symbol* temp = node->left;
    node->left = temp->right;
    temp->right = node;

    backend_symbol_adjust(node);
    backend_symbol_adjust(temp);

    return temp;
}

static backend_symbol* backend_symbol_rotate_left(backend_symbol* node)
{
    backend_symbol* temp = node->right;
    node->right = temp->left;
    temp->left = node;

    backend_symbol_adjust(node);
    backend_symbol_adjust(temp);

    return temp;
}

static backend_symbol* backend_symbol_balance(backend_symbol* node)
{
    backend_symbol_adjust(node);

    if (backend_symbol_height(node->left) - backend_symbol_height(node->right) == 2)
    {
        if (backend_symbol_height(node->left->right) > backend_symbol_height(node->left->left))
        {
            node->left = backend_symbol_rotate_left(node->left);
        }
        return backend_symbol_rotate_right(node);
    }
    else if (backend_symbol_height(node->right) - backend_symbol_height(node->left) == 2)
    {
        if (backend_symbol_height(node->right->left) > backend_symbol_height(node->right->right))
        {
            node->right = backend_symbol_rotate_right(node->right);
        }
        return backend_symbol_rotate_left(node);
    }
    else
    {
        return node;
    }
}

static backend_symbol* backend_symbol_insert(backend_symbol* node, backend_symbol* symbol)
{
    if (symbol->address < node->address)
    {
        node->left = node->left ? backend_symbol_insert(node->left, symbol) : symbol;
    }
    else if (symbol->address > node->address)
    {
        node->right = node->right ? backend_symbol_insert(node->right, symbol) : symbol;
    }
    else
    {
        Assert(0);
    }

    Assert(node->left == NULL || node->left->address < node->address);
    Assert(node->right == NULL || node->right->address > node->address);
    backend_symbol* root = backend_symbol_balance(node);
    Assert(root->left == NULL || root->left->address < root->address);
    Assert(root->right == NULL || root->right->address > root->address);

    return root;
}

static backend_symbol* backend_symbol_lookup(HANDLE process, backend_module* module, backend_strings* strings, uint32_t* symbol_id, uint64_t address)
{
    SYMBOL_INFO_PACKAGEW info =
    {
        .si =
        {
            .SizeOfStruct = sizeof(info.si),
            .MaxNameLen = MAX_SYM_NAME,
        },
    };

    DWORD64 displacement;
    if (!SymFromAddrW(process, address, &displacement, &info.si))
    {
        return NULL;
    }

    if (info.si.Size == 0)
    {
        backend_symbol* symbol = backend_symbol_find(module->symbols, info.si.Address);
        if (symbol && symbol->address == info.si.Address)
        {
            return symbol;
        }
    }
    else if (address < info.si.Address || address >= info.si.Address + info.si.Size)
    {
        return NULL;
    }

    backend_symbol* symbol = backend_symbol_memory_alloc(module->memory);
    symbol->address = info.si.Address;
    symbol->size = info.si.Size;
    symbol->id = ++(*symbol_id);
    symbol->height = 1;
    symbol->left = NULL;
    symbol->right = NULL;

    backend_reply_symbol reply =
    {
        .id = symbol->id,
        .name = backend_strings_get(strings, info.si.Name, info.si.NameLen + 1),
        .size = symbol->size,
        .address = symbol->address,
        .module = module->name,
    };
    
    IMAGEHLP_LINEW64 line =
    {
        .SizeOfStruct = sizeof(line),
    };
    
    DWORD offset;
    if (SymGetLineFromAddrW64(process, symbol->address, &offset, &line))
    {
        reply.file = backend_strings_get(strings, line.FileName, -1);
        reply.line = line.LineNumber;
        reply.line_last = line.LineNumber;
    }

    if (symbol->size != 0)
    {
        if (SymGetLineFromAddrW64(process, symbol->address + symbol->size - 1, &offset, &line))
        {
            reply.line_last = line.LineNumber;
        }
    }

    backend_reply_new_symbol(&reply);

    module->symbols = module->symbols ? backend_symbol_insert(module->symbols, symbol) : symbol;
    return symbol;
}

static BOOL CALLBACK backend_symbols_callback(HANDLE process, ULONG action, ULONG64 data, ULONG64 user)
{
    (void)process;
    (void)user;

    if (action == CBA_DEBUG_INFO)
    {
        const wchar_t* str = (wchar_t*)data;

        const wchar_t* prefix = L"SYMSRV: ";
        if (wcsncmp(str, prefix, wcslen(prefix)) == 0 &&
            wcsstr(str, L" from https://msdl.microsoft.com/download/symbols: ") != NULL)
        {
            // SYMSRV:  ntmodule.pdb from https://msdl.microsoft.com/download/symbols: 367326 bytes -
            backend_reply_symbols(CXX_SYMBOL_STATUS_DOWNLOADING);
        }

        prefix = L"DBGHELP: ";
        if (wcsncmp(str, prefix, wcslen(prefix)) == 0)
        {
            if (wcsstr(str, L" - private symbols & lines") != NULL)
            {
                //DBGHELP: module - private symbols & lines
                backend_reply_symbols(CXX_SYMBOL_STATUS_LOADED_PRIVATE);
            }
            else if (wcsstr(str, L" - public symbol") != NULL)
            {
                //DBGHELP: module - public symbols
                backend_reply_symbols(CXX_SYMBOL_STATUS_LOADED_PUBLIC);
            }
            else if (wcsstr(str, L" - export symbols") != NULL)
            {
                //DBGHELP: module - export symbols
                backend_reply_symbols(CXX_SYMBOL_STATUS_LOADED_EXPORT);
            }
        }
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

int backend_symbols_init(HANDLE process, backend_symbols* symbols, int download_symbols, int is_wow64)
{
    symbols->list.next = NULL;
    symbols->free.next = NULL;
    symbols->symbol_id = 0;

    DWORD options = SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_DEBUG;
    if (download_symbols)
    {
        options |= SYMOPT_FAVOR_COMPRESSED | SYMOPT_IGNORE_NT_SYMPATH;
    }
    if (is_wow64)
    {
        options |= SYMOPT_INCLUDE_32BIT_MODULES;
    }
    SymSetOptions(options);

    wchar_t app[MAX_PATH];
    GetModuleFileNameW(NULL, app, ArrayCount(app));
    PathRemoveFileSpec(app);

    wchar_t path[MAX_PATH];
    if (download_symbols)
    {
        wsprintfW(path, L"SRV*%s\\symbols*https://msdl.microsoft.com/download/symbols", app);
    }
    else
    {
        wsprintfW(path, L"CACHE*%s\\symbols", app);
    }

    if (!SymInitializeW(process, path, FALSE))
    {
        backend_reply_error("SymInitialize failed");
        return 0;
    }

    SymRegisterCallbackW64(process, backend_symbols_callback, 0);

    return 1;
}

void backend_symbols_done(HANDLE process)
{
    SymCleanup(process);
}

void backend_symbols_load(HANDLE process, backend_symbols* symbols, backend_strings* strings, HANDLE handle, const wchar_t* name, uint64_t base)
{
    backend_reply_module_load(base, name);

    if (!SymLoadModuleExW(process, handle, name, NULL, base, 0, NULL, 0))
    {
        backend_reply_error("SymLoadModuleEx failed");
        return;
    }

    IMAGEHLP_MODULEW64 info =
    {
        .SizeOfStruct = sizeof(info),
    };
    if (!SymGetModuleInfoW64(process, base, &info))
    {
        backend_reply_error("SymGetModuleInfo64 failed");
        return;
    }

    backend_module* module;
    CXX_FREELIST_GET(module, symbols->free);
    module->handle = handle;
    module->base = info.BaseOfImage;
    module->size = info.ImageSize;
    module->name = backend_strings_get(strings, info.ModuleName, -1);
    module->symbols = NULL;
    module->memory = backend_symbol_memory_create();
    CXX_LIST_ADD(module, symbols->list);
}

#if 0
#include <stdio.h>
static void dump(FILE* f, int level, backend_symbol* symbol)
{
    if (!symbol) return;
    dump(f, level + 1, symbol->left);
    dump(f, level + 1, symbol->right);

    fprintf(f, "n%llx[label=\"%llx\\nh=%u\"]\n", symbol->address, symbol->address, symbol->height);
    if (symbol->left)
    {
        fprintf(f, "n%llx -> n%llx [label=\"left\"];\n", symbol->address, symbol->left->address);
    }
    if (symbol->right)
    {
        fprintf(f, "n%llx -> n%llx [label=\"right\"];\n", symbol->address, symbol->right->address);
    }
}
#endif

void backend_symbols_unload(HANDLE process, backend_symbols* symbols, uint64_t base)
{
    backend_reply_module_unload(base);

    backend_module* list = &symbols->list;
    while (list->next && list->next->base != base)
    {
        list = list->next;
    }
    backend_module* module = list->next;
    Assert(module);
    list->next = module->next;

    if (!SymUnloadModule64(process, base))
    {
        backend_reply_error("SymUnloadModule64 failed");
    }

#if 0
    static int c = 0;
    char name[100];
    sprintf(name, "symbols_%i.dot", ++c);
    FILE* f = fopen(name, "w");
    fprintf(f, "digraph {\n");
    dump(f, 0, module->symbols);
    fprintf(f, "}\n");
    fclose(f);
#endif

    backend_symbol_memory_free(module->memory);
    if (module->handle)
    {
        CloseHandle(module->handle);
    }

    CXX_LIST_ADD(module, symbols->free);
}

backend_symbol* backend_symbols_get(HANDLE process, backend_symbols* symbols, backend_strings* strings, uint64_t address)
{
    backend_module* module = NULL;
    {
        backend_module* prev = &symbols->list;
        while (prev->next)
        {
            module = prev->next;
            if (module->base <= address && address < module->base + module->size)
            {
                break;
            }
            prev = module;
        }
        if (module == NULL)
        {
            return NULL;
        }
    }
    if (module->base > address || address >= module->base + module->size)
    {
        return NULL;
    }

    backend_symbol* symbol = backend_symbol_find(module->symbols, address);
    if (symbol && symbol->address <= address && address < symbol->address + symbol->size)
    {
        return symbol;
    }

    return backend_symbol_lookup(process, module, strings, &symbols->symbol_id, address);
}

uint32_t backend_symbols_lookup_line(HANDLE process, uint64_t address)
{
    DWORD offset;
    IMAGEHLP_LINEW64 info =
    {
        .SizeOfStruct = sizeof(info),
    };
    if (SymGetLineFromAddrW64(process, address, &offset, &info))
    {
        return info.LineNumber;
    }

    return ~(uint32_t)0;
}
