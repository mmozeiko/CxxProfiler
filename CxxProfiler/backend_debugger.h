#pragma once

typedef struct backend_globals backend_globals;

void backend_debugger_start(backend_globals* globals);
void backend_debugger_stop(backend_globals* globals);
