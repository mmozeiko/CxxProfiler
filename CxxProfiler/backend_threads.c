#include "backend_threads.h"
#include "backend.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void backend_threads_init(backend_threads* threads)
{
    threads->list.next = NULL;
    threads->free.next = NULL;
}

void backend_threads_add(backend_threads* threads, uint32_t id, uint64_t entry, HANDLE handle)
{
    backend_reply_thread_start(id, entry);

    backend_thread* thread;
    CXX_FREELIST_GET(thread, threads->free);
    thread->id = id;
    thread->handle = handle;
    CXX_LIST_ADD(thread, threads->list);
}

void backend_threads_remove(backend_threads* threads, uint32_t id)
{
    backend_reply_thread_end(id);

    backend_thread* list = &threads->list;
    while (list->next && list->next->id != id)
    {
        list = list->next;
    }
    backend_thread* thread = list->next;
    Assert(thread);
    list->next = thread->next;

    if (thread->handle)
    {
        CloseHandle(thread->handle);
    }

    CXX_LIST_ADD(thread, threads->free);
}
