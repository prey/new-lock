:: Copyright (c) 2015 Jacob Lifshay, Fork Ltd.
:: This file is part of Prey.
::
:: Prey is free software: you can redistribute it and/or modify
:: it under the terms of the GNU General Public License as published by
:: the Free Software Foundation, either version 3 of the License, or
:: (at your option) any later version.
::
:: Prey is distributed in the hope that it will be useful,
:: but WITHOUT ANY WARRANTY; without even the implied warranty of
:: MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
:: GNU General Public License for more details.
::
:: You should have received a copy of the GNU General Public License
:: along with Prey.  If not, see <http://www.gnu.org/licenses/>.
::
@if "%1"=="" goto help
:parseargs
@if "%1"=="Debug64" goto Debug64
@if "%1"=="Release64" goto Release64
@if "%1"=="Debug32" goto Debug32
@if "%1"=="Release32" goto Release32
@if "%1"=="ZipSource" goto ZipSource
@if "%1"=="ZipAll" goto ZipAll
@if "%1"=="" exit /b
@echo invalid build target
@exit /b

:help
@echo Usage: build.cmd ^<target^> [^<target^> [...]]
@echo Targets: Debug32 Release32 Debug64 Release64 ZipSource ZipAll
@exit /b

:ZipSource
@shift
move /Y lock-screen-src.zip lock-screen-src.zip.bak 2>NUL
zip lock-screen-src.zip -r external_libs res build.cmd *.cpp *.c *.h *.cbp *.rc
@goto parseargs

:ZipAll
@shift
move /Y lock-screen.zip lock-screen.zip.bak 2>NUL
zip lock-screen.zip -r external_libs res build.cmd *.cpp *.c *.h *.cbp *.rc bin
@goto parseargs

:Debug64
@shift
@md obj\Debug 2>NUL
windres -DDEBUG -J rc -O coff -i lock-screen.rc -o obj\Debug\lock-screen.res
g++ -m64 -Wall -std=c++11 -municode -g -DDEBUG -c main.cpp -o obj\Debug\main.o
g++ -m64 -Wall -std=c++11 -municode -g -DDEBUG -c persistent.cpp -o obj\Debug\persistent.o
gcc -m64 -Wall -municode -g -DDEBUG -c md5.c -o obj\Debug\md5.o
@md bin\Debug 2>NUL
g++ -m64 -Lexternal_libs -o bin\Debug\new-prey-lock.exe obj\Debug\main.o obj\Debug\persistent.o obj\Debug\md5.o obj\Debug\lock-screen.res -municode -static -lgdi32 -luser32 -lkernel32 -lcomctl32 -lole32 -lpropsys_x64 -lWtsApi32
@goto parseargs

:Release64
@shift
@md obj\Release 2>NUL
windres -J rc -O coff -i lock-screen.rc -o obj\Release\lock-screen.res
g++ -m64 -Wall -std=c++11 -municode -O2 -c main.cpp -o obj\Release\main.o
g++ -m64 -Wall -std=c++11 -municode -O2 -c persistent.cpp -o obj\Release\persistent.o
gcc -m64 -Wall -municode -O2 -c md5.c -o obj\Release\md5.o
@md bin\Release 2>NUL
g++ -m64 -Lexternal_libs -o bin\Release\new-prey-lock.exe obj\Release\main.o obj\Release\persistent.o obj\Release\md5.o obj\Release\lock-screen.res -municode -static -s -lgdi32 -luser32 -lkernel32 -lcomctl32 -lole32 -lpropsys_x64 -lWtsApi32
@goto parseargs

:Debug32
@shift
@md obj\Debug-x86 2>NUL
windres -DDEBUG -J rc -O coff -i lock-screen.rc -o obj\Debug-x86\lock-screen.res
g++ -m32 -Wall -std=c++11 -municode -g -DDEBUG -c main.cpp -o obj\Debug-x86\main.o
g++ -m32 -Wall -std=c++11 -municode -g -DDEBUG -c persistent.cpp -o obj\Debug-x86\persistent.o
gcc -m32 -Wall -municode -g -DDEBUG -c md5.c -o obj\Debug-x86\md5.o
@md bin\Debug-x86 2>NUL
g++ -m32 -Lexternal_libs -o bin\Debug-x86\new-prey-lock.exe obj\Debug-x86\main.o obj\Debug-x86\persistent.o obj\Debug-x86\md5.o obj\Debug-x86\lock-screen.res -municode -static -lgdi32 -luser32 -lkernel32 -lcomctl32 -lole32 -lpropsys_x86 -lWtsApi32
@goto parseargs

:Release32
@shift
@md obj\Release-x86 2>NUL
windres -J rc -O coff -i lock-screen.rc -o obj\Release-x86\lock-screen.res
g++ -m32 -Wall -std=c++11 -municode -O2 -c persistent.cpp -o obj\Release-x86\persistent.o
g++ -m32 -Wall -std=c++11 -municode -O2 -c main.cpp -o obj\Release-x86\main.o
gcc -m32 -Wall -municode -O2 -c md5.c -o obj\Release-x86\md5.o
@md bin\Release-x86 2>NUL
g++ -m32 -Lexternal_libs -o bin\Release-x86\new-prey-lock.exe obj\Release-x86\main.o obj\Release-x86\persistent.o obj\Release-x86\md5.o obj\Release-x86\lock-screen.res -municode -static -s -lgdi32 -luser32 -lkernel32 -lcomctl32 -lole32 -lpropsys_x86 -lWtsApi32 -lwtsapi32 -luserenv
@goto parseargs
