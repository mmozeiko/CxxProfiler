#include "backend_debugger.h"
#include "backend_strings.h"
#include "backend_symbols.h"
#include "backend_threads.h"
#include "backend_commands.h"
#include "backend.h"

#define WIN32_LEAN_AND_MEAN
#define DBGHELP_TRANSLATE_TCHAR
#include <windows.h>
#include <mmsystem.h>
#include <dbghelp.h>

typedef struct backend_debugger {
    uint64_t process_base;
    BOOL is_wow64;

    backend_strings strings;
    backend_symbols symbols;
    backend_threads threads;

    backend_stack stack;
} backend_debugger;

static void backend_stack_init(backend_stack* stack)
{
    stack->count = 0;
    stack->next = NULL;
}

static void backend_stack_reset(backend_stack* stack)
{
    while (stack)
    {
        stack->count = 0;
        stack = stack->next;
    }
}

static backend_stack* backend_stack_append(backend_stack* stack, backend_stack_entry* entry)
{
    if (stack->count == ArrayCount(stack->entries))
    {
        stack = stack->next = backend_alloc(sizeof(*stack));
        backend_stack_init(stack);
    }
    stack->entries[stack->count++] = *entry;
    return stack;
}

static void backend_debugger_take_samples(HANDLE process, backend_debugger* debugger)
{
    for (backend_thread* thread = &debugger->threads.list; thread->next != NULL; thread = thread->next)
    {
        uint32_t thread_id = thread->next->id;
        HANDLE thread_handle = thread->next->handle;

        if (SuspendThread(thread_handle) == (DWORD)-1)
        {
            continue;
        }

        WOW64_CONTEXT ctx32;
        CONTEXT ctx64;
        PVOID ctx = NULL;
        DWORD machine;

        STACKFRAME64 frame;
        backend_zero(&frame, sizeof(frame));
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Mode = AddrModeFlat;

        if (debugger->is_wow64)
        {
            machine = IMAGE_FILE_MACHINE_I386;
            ctx32.ContextFlags = WOW64_CONTEXT_CONTROL | WOW64_CONTEXT_INTEGER;
            if (Wow64GetThreadContext(thread_handle, &ctx32))
            {
                frame.AddrPC.Offset = ctx32.Eip;
                frame.AddrFrame.Offset = ctx32.Ebp;
                frame.AddrStack.Offset = ctx32.Esp;
                ctx = &ctx32;
            }
        }
        else
        {
            machine = IMAGE_FILE_MACHINE_AMD64;
            ctx64.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
            if (GetThreadContext(thread_handle, &ctx64))
            {
                frame.AddrPC.Offset = ctx64.Rip;
                frame.AddrFrame.Offset = ctx64.Rbp;
                frame.AddrStack.Offset = ctx64.Rsp;
                ctx = &ctx64;
            }
        }

        if (ctx)
        {
            uint32_t ptr_size = debugger->is_wow64 ? sizeof(uint32_t) : sizeof(uint64_t);

            backend_stack* stack = &debugger->stack;
            backend_stack_reset(stack);

            uint64_t delta = 0;
            DWORD64 last_stack = 0;
            while (StackWalk64(machine, process, thread_handle, &frame, ctx, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL))
            {
                if (frame.AddrPC.Offset == 0)
                {
                    break;
                }
                if (frame.AddrStack.Offset <= last_stack)
                {
                    break;
                }
                if ((frame.AddrStack.Offset & (ptr_size - 1)) != 0)
                {
                    break;
                }
                last_stack = frame.AddrStack.Offset;

                uint64_t address = frame.AddrPC.Offset - delta;

                backend_symbol* symbol = backend_symbols_get(process, &debugger->symbols, &debugger->strings, address);
                if (symbol)
                {
                    backend_stack_entry entry =
                    {
                        .symbol = symbol->id,
                        .line = backend_symbols_lookup_line(process, address),
                        .offset = (uint32_t)(address - symbol->address),
                    };
                    stack = backend_stack_append(stack, &entry);
                }
                delta = 1;
            }

            backend_reply_stack_samples(thread_id, &debugger->stack);
        }
        else
        {
            backend_reply_error("GetThreadContext failed");
        }

        ResumeThread(thread_handle);
    }
}

static void backend_get_filename_from_handle(wchar_t* result, HANDLE handle)
{
    wchar_t path[MAX_PATH];
    DWORD length = GetFinalPathNameByHandleW(handle, path, ArrayCount(path), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (length == 0)
    {
        result[0] = 0;
        return;
    }

    int offset = 0;
    if (length >= 4 &&
        path[0] == L'\\' &&
        path[1] == L'\\' &&
        path[2] == L'?' &&
        path[3] == L'\\')
    {
        offset = 4;
    }

    length -= offset;
    backend_copy(result, path + offset, length * sizeof(wchar_t));
    result[length] = 0;
}

static void backend_get_filename_from_pointer(wchar_t* result, HANDLE process, LPVOID address, BOOL unicode, BOOL is_wow64)
{
    result[0] = 0;

    uint64_t ptr = 0;
    SIZE_T ptr_size = is_wow64 ? sizeof(uint32_t) : sizeof(uint64_t);
    SIZE_T read;
    if (ReadProcessMemory(process, address, &ptr, ptr_size, &read) && read == ptr_size)
    {
        if (ptr_size == sizeof(uint32_t) && (uint32_t)ptr != 0 ||
            ptr_size == sizeof(uint64_t) && (uint64_t)ptr != 0)
        {
            char buffer[MAX_PATH * sizeof(wchar_t)];
            if (ReadProcessMemory(process, (LPCVOID)ptr, buffer, MAX_PATH * (unicode ? sizeof(wchar_t) : sizeof(char)), &read) && read > 0)
            {
                if (unicode)
                {
                    wchar_t* wpath = (wchar_t*)buffer;
                    wpath[MAX_PATH - 1] = 0;

                    size_t wlength = wcslen(wpath);
                    backend_copy(result, wpath, wlength * sizeof(wchar_t));
                    result[wlength] = 0;
                }
                else
                {
                    char* path = (char*)buffer;
                    path[MAX_PATH - 1] = 0;

                    int length = (int)strlen(path);
                    int wlength = MultiByteToWideChar(CP_UTF8, 0, path, length, NULL, 0);
                    Assert(wlength > 0);

                    wlength = MultiByteToWideChar(CP_UTF8, 0, path, length, result, wlength + 1);
                    result[wlength] = 0;
                }
            }
        }
    }
}

static int backend_debugger_process_start(backend_globals* globals, backend_debugger* debugger, DWORD process_id, DWORD thread_id, const CREATE_PROCESS_DEBUG_INFO* info)
{
    globals->process = info->hProcess;
    debugger->process_base = (uint64_t)info->lpBaseOfImage;
    IsWow64Process(info->hProcess, &debugger->is_wow64);

    backend_reply_process_start(process_id, debugger->is_wow64 ? sizeof(uint32_t) : sizeof(uint64_t));

    if (!backend_symbols_init(globals->process, &debugger->symbols, globals->download_symbols, debugger->is_wow64))
    {
        return 0;
    }

    backend_threads_add(&debugger->threads, thread_id, (uint64_t)info->lpStartAddress, info->hThread);

    wchar_t name[MAX_PATH];
    backend_get_filename_from_handle(name, info->hFile);
    backend_symbols_load(globals->process, &debugger->symbols, &debugger->strings, info->hFile, name, (uint64_t)info->lpBaseOfImage);

    return 1;
}

static void backend_debugger_process_end(backend_globals* globals, backend_debugger* debugger, DWORD thread_id, const EXIT_PROCESS_DEBUG_INFO* info)
{
    backend_reply_process_end(info->dwExitCode);

    backend_symbols_unload(globals->process, &debugger->symbols, debugger->process_base);
    backend_threads_remove(&debugger->threads, thread_id);

    backend_symbols_done(globals->process);
    CloseHandle(globals->process);
}

static void backend_debugger_thread_start(backend_debugger* debugger, DWORD thread_id, const CREATE_THREAD_DEBUG_INFO* info)
{
    backend_threads_add(&debugger->threads, thread_id, (uint64_t)info->lpStartAddress, info->hThread);
}

static void backend_debugger_thread_end(backend_debugger* debugger, DWORD thread_id, const EXIT_THREAD_DEBUG_INFO* info)
{
    (void)info;
    backend_threads_remove(&debugger->threads, thread_id);
}

static void backend_debugger_module_load(HANDLE process, backend_debugger* debugger, const LOAD_DLL_DEBUG_INFO* info)
{
    wchar_t name[MAX_PATH];
    name[0] = 0;
    if (info->lpImageName != NULL)
    {
        backend_get_filename_from_pointer(name, process, info->lpImageName, info->fUnicode != 0, debugger->is_wow64);
    }
    if (name[0] == 0)
    {
        backend_get_filename_from_handle(name, info->hFile);
    }

    backend_symbols_load(process, &debugger->symbols, &debugger->strings, info->hFile, name, (uint64_t)info->lpBaseOfDll);
}

static void backend_debugger_module_unload(HANDLE process, backend_debugger* debugger, const UNLOAD_DLL_DEBUG_INFO* info)
{
    backend_symbols_unload(process, &debugger->symbols, (uint64_t)info->lpBaseOfDll);
}

static DWORD CALLBACK backend_debugger_thread(LPVOID arg)
{
    backend_globals* globals = arg;
    typedef BOOL WINAPI WaitForDebugEventExProc(LPDEBUG_EVENT, DWORD);

    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    Assert(kernel32);

    WaitForDebugEventExProc* WaitForDebugEventEx = (WaitForDebugEventExProc*)GetProcAddress(kernel32, "WaitForDebugEventEx");
    if (!WaitForDebugEventEx)
    {
        backend_reply_error("WaitForDebugEventEx not available, using WaitForDebugEvent");
        WaitForDebugEventEx = WaitForDebugEvent;
    }

    if (globals->attach_pid)
    {
        if (!DebugActiveProcess(globals->attach_pid))
        {
            backend_reply_error("DebugActiveProcess failed");
            return 0;
        }
    }
    else
    {
        STARTUPINFOW startup =
        {
            .cb = sizeof(startup),
        };
        PROCESS_INFORMATION info;
        if (!CreateProcessW(globals->launch_command, globals->launch_arguments, NULL, NULL, FALSE,
            DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS, NULL, globals->launch_folder, &startup, &info))
        {
            backend_reply_error("CreateProcess failed");
            return 0;
        }
        CloseHandle(info.hProcess);
        CloseHandle(info.hThread);
    }
    BOOL kill = DebugSetProcessKillOnExit(globals->attach_pid == 0);
    Assert(kill);

    backend_debugger debugger;
    backend_strings_init(&debugger.strings);
    backend_threads_init(&debugger.threads);
    backend_stack_init(&debugger.stack);

    if (timeBeginPeriod(1) != TIMERR_NOERROR)
    {
        backend_reply_error("timeBeginPeriod failed, sampling interval will not be very precise");
    }

    uint32_t sampling_msec = Clamp(globals->sampling_usec / 1000, 1, 1000);

    globals->running = 1;
    while (globals->running)
    {
        DEBUG_EVENT event;
        if (WaitForDebugEventEx(&event, sampling_msec))
        {
            DWORD continue_status = DBG_CONTINUE;

            switch (event.dwDebugEventCode)
            {
            case CREATE_PROCESS_DEBUG_EVENT:
                if (!backend_debugger_process_start(globals, &debugger, event.dwProcessId, event.dwThreadId, &event.u.CreateProcessInfo))
                {
                    globals->running = 0;
                }
                break;

            case EXIT_PROCESS_DEBUG_EVENT:
                backend_debugger_process_end(globals, &debugger, event.dwThreadId, &event.u.ExitProcess);
                globals->running = 0;
                break;

            case CREATE_THREAD_DEBUG_EVENT:
                backend_debugger_thread_start(&debugger, event.dwThreadId, &event.u.CreateThread);
                break;

            case EXIT_THREAD_DEBUG_EVENT:
                backend_debugger_thread_end(&debugger, event.dwThreadId, &event.u.ExitThread);
                break;

            case LOAD_DLL_DEBUG_EVENT:
                backend_debugger_module_load(globals->process, &debugger, &event.u.LoadDll);
                break;

            case UNLOAD_DLL_DEBUG_EVENT:
                backend_debugger_module_unload(globals->process, &debugger, &event.u.UnloadDll);
                break;

            case EXCEPTION_DEBUG_EVENT:
                if (!event.u.Exception.dwFirstChance)
                {
                    continue_status = DBG_EXCEPTION_NOT_HANDLED;
                }
                break;
            }

            if (globals->running)
            {
                BOOL ok = ContinueDebugEvent(event.dwProcessId, event.dwThreadId, continue_status);
                Assert(ok);
            }
        }
        else
        {
            DWORD err = GetLastError();
            if (err == ERROR_SEM_TIMEOUT)
            {
                if (globals->process)
                {
                    backend_debugger_take_samples(globals->process, &debugger);
                }
            }
            else
            {
                backend_reply_error("WaitForDebugEvnet failed");
            }
        }
    }

    timeEndPeriod(1);

    return 0;
}

void backend_debugger_start(backend_globals* globals)
{
    globals->thread = CreateThread(NULL, 0, backend_debugger_thread, globals, 0, NULL);
}

void backend_debugger_stop(backend_globals* globals)
{
    if (globals->attach_pid)
    {
        DebugActiveProcessStop(globals->attach_pid);
        globals->running = 0;
    }
    else if (globals->process)
    {
        TerminateProcess(globals->process, 0);
        CloseHandle(globals->process);
    }
    WaitForSingleObject(globals->thread, INFINITE);
    CloseHandle(globals->thread);
}
