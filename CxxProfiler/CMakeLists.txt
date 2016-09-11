set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(UI
  MainWindow.ui
  NewDialog.ui
  Preferences.ui
  RunningDialog.ui
  SymbolWidget.ui
)

set(MOC
  Profiler.h
  SyntaxHighlighter.h
  MainWindow.h
  NewDialog.h
  Preferences.h
  RunningDialog.h
  SymbolWidget.h
  SourceWidget.h
  SourceLoader.h
)

set(OTHER
  Resources.rc
  Precompiled.h
  Utils.h
  Symbols.h
  Version.h
)
set(SOURCE
  Precompiled.cpp
  Utils.cpp
  Main.cpp
  Profiler.cpp
  SyntaxHighlighter.cpp
  MainWindow.cpp
  NewDialog.cpp
  Preferences.cpp
  RunningDialog.cpp
  SymbolWidget.cpp
  SourceWidget.cpp
  SourceLoader.cpp
  Symbols.cpp
)

set(BACKEND
  backend.h
  backend.c
  backend_commands.h
  backend_debugger.h
  backend_debugger.c
  backend_strings.h
  backend_strings.c
  backend_symbols.h
  backend_symbols.c
  backend_threads.h
  backend_threads.c
)

set(QRC Resources.qrc)

qt5_wrap_ui(UI_OUT ${UI})
qt5_wrap_cpp(MOC_OUT ${MOC})
qt5_add_resources(QRC_OUT ${QRC} OPTIONS "-no-compress")
                                  
source_group("" FILES ${SOURCE} ${MOC} ${UI} ${QRC} ${BACKEND} ${OTHER})
source_group("Generated" FILES ${MOC_OUT} ${UI_OUT} ${MOC_OUT} ${QRC_OUT})

add_executable(CxxProfiler WIN32 ${SOURCE} ${BACKEND} ${OTHER} ${MOC} ${MOC_OUT} ${UI_OUT} ${MOC_OUT} ${QRC_OUT})
qt5_use_modules(CxxProfiler Widgets Concurrent)
use_pch(Precompiled.h Precompiled.cpp "${SOURCE}")
use_pch(Precompiled.h Precompiled.cpp "${MOC_OUT}")
