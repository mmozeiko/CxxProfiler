set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(UI
  MainWindow.ui
  NewDialog.ui
  RunningDialog.ui
  SymbolWidget.ui
)

set(MOC
  Profiler.h
  SyntaxHighlighter.h
  MainWindow.h
  NewDialog.h
  RunningDialog.h
  SymbolWidget.h
  SourceWidget.h
  SourceLoader.h
)

set(SOURCE
  Resources.rc
  Precompiled.h
  Precompiled.cpp
  Utils.h
  Utils.cpp
  Main.cpp
  Profiler.cpp
  SyntaxHighlighter.cpp
  MainWindow.cpp
  NewDialog.cpp
  RunningDialog.cpp
  SymbolWidget.cpp
  SourceWidget.cpp
  SourceLoader.cpp
  Symbols.cpp
  Symbols.h
  Version.h
)

set(QRC Resources.qrc)

qt5_wrap_ui(UI_OUT ${UI})
qt5_wrap_cpp(MOC_OUT ${MOC})
qt5_add_resources(QRC_OUT ${QRC} OPTIONS "-no-compress")
                                  
source_group("" FILES ${SOURCE} ${MOC} ${UI} ${QRC})
source_group("Generated" FILES ${MOC_OUT} ${UI_OUT} ${MOC_OUT} ${QRC_OUT})

add_executable(CxxProfiler WIN32 ${SOURCE} ${MOC} ${MOC_OUT} ${UI_OUT} ${MOC_OUT} ${QRC_OUT})
qt5_use_modules(CxxProfiler Widgets Concurrent)
use_pch(CxxProfiler Precompiled.h Precompiled.cpp)
