C/C++ Profiler
==============

[![Build Status](https://ci.appveyor.com/api/projects/status/mus8ve0g0g2sc9qb/branch/master?svg=true)](https://ci.appveyor.com/project/mmozeiko/cxxprofiler/branch/master)
[![Downloads](https://img.shields.io/github/downloads/mmozeiko/CxxProfiler/total.svg?maxAge=86400)](https://github.com/mmozeiko/CxxProfiler/releases)
[![Release](https://img.shields.io/github/release/mmozeiko/CxxProfiler.svg?maxAge=86400)](https://github.com/mmozeiko/CxxProfiler/releases/latest)
[![License](https://img.shields.io/github/license/mmozeiko/CxxProfiler.svg?maxAge=2592000)](https://github.com/mmozeiko/CxxProfiler/blob/master/LICENSE)

Simple sampling C/C++ profiler. Requires 64-bit Windows.
Profiler attaches to process as a debugger, so make sure you disable any code that does something different when running under debugger.

For more accurate results do following steps when building executable (and dll files it depends)

1. enable optimizations (C/C++ -> Optimizations -> Optimization, /O2)
2. generate debug information in compiler (C/C++ -> General -> Debug Information Format, /Zi)
3. generate debug information in linker (Linker -> Debugging -> Generate Debug Info, /DEBUG)
4. for 32-bit build, don't omit frame pointers (C/C++ -> Command Line -> put in Additional Options '/Oy-')
5. for VS2013 and newer add /Zo in additional compiler options for enhanced debug information

Features
--------

* Allows to profile 32-bit or 64-bit executables
* Run new process or attach existing one
* Automatic download of pdb files for system dll files
* Shows flat view - who was function taking most time
* Shows call graph - which function calls which one
* Search or filter by function name
* View source code with profiling stats per line
* Navigate profile information in source code view (click on red percent numbers)
* Open files in explorer, in default editor or in Visual Studio
* Saving collected data to file for analyzing later

Download
--------

Get 64-bit Windows binary on [releases](https://github.com/mmozeiko/CxxProfiler/releases) page.

Screenshots
-----------

![screenshot1.png](https://raw.githubusercontent.com/wiki/mmozeiko/CxxProfiler/screenshot1.png)
![screenshot2.png](https://raw.githubusercontent.com/wiki/mmozeiko/CxxProfiler/screenshot2.png)
![screenshot3.png](https://raw.githubusercontent.com/wiki/mmozeiko/CxxProfiler/screenshot3.png)


Build instructions
------------------

1. Get Visual Studio 2013 - https://www.visualstudio.com/en-us/products/visual-studio-community-vs.aspx
2. Install CMake - http://www.cmake.org/
3. Install or build from source Qt v5 (64-bit) - https://www.qt.io/download-open-source/
4. Set QTDIR environment variable to Qt installation, or adjust path to Qt in bootstrap.cmd
5. Run bootstrap.cmd it will generate Visual Studio 2013 solution in Build folder

Future plans, TODO
------------------

* Capturing/showing assembly output for functions
* Dwarf debug info support for gcc/clang compilers
* Port to other platforms (GNU/Linux, OSX, ...)
* Visualizations (charts, flame graph)
* Displaying results in real time
* Remove dependency on Qt?

License
-------

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.
