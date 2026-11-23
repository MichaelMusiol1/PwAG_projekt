:; # Windows: dwuklik albo "build.cmd".   Linux: "sh build.cmd"
:; # Ten sam plik dziala w cmd.exe i w sh: cmd.exe traktuje linie zaczynajace sie od ":;"
:; # jak etykiety (pomija je), a sh wykonuje je jak zwykle polecenia. Wyglada dziwnie, ale tak musi byc.
:; # Konce linii musza zostac LF (pilnuje tego .gitattributes) - sh nie znosi CRLF.
:; cd "$(dirname "$0")" || exit 1
:; command -v cmake >/dev/null || { echo "[BLAD] Brak CMake. Instrukcja instalacji: README.md"; exit 1; }
:; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release || exit 1
:; cmake --build build --parallel "$(nproc 2>/dev/null || echo 4)" || exit 1
:; echo "Gotowe: bin/gra   (uruchom: sh run.cmd)"; exit 0
@echo off
rem Dalej juz tylko cmd.exe - sh skonczyl na exit powyzej.
setlocal
cd /d "%~dp0"
set "PAUSE_CMD=pause"
if /i "%~1"=="nopause" set "PAUSE_CMD="

where cmake >nul 2>nul
if errorlevel 1 (
    echo [BLAD] Nie znaleziono CMake. Zainstaluj z https://cmake.org/download/
    echo        i zaznacz "Add CMake to the system PATH". Szczegoly: README.md
    %PAUSE_CMD%
    exit /b 1
)

rem Wybor kompilatora: Visual Studio (jesli zainstalowane z C++), inaczej MinGW (gcc w PATH)
set "HAVE_VS="
set "GENERATOR="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "HAVE_VS=1"
if not defined HAVE_VS (
    where gcc >nul 2>nul
    if not errorlevel 1 set "GENERATOR=MinGW Makefiles"
)
if not defined HAVE_VS if not defined GENERATOR (
    echo [BLAD] Nie znaleziono kompilatora C++. Zainstaluj Visual Studio Community
    echo        z pakietem "Programowanie aplikacji klasycznych w jezyku C++"
    echo        albo MSYS2 + MinGW i dodaj C:\msys64\ucrt64\bin do PATH. Szczegoly: README.md
    %PAUSE_CMD%
    exit /b 1
)

if not exist build\CMakeCache.txt (
    if defined GENERATOR (
        cmake -S . -B build -G "%GENERATOR%" -DCMAKE_BUILD_TYPE=Release
    ) else (
        cmake -S . -B build
    )
    if errorlevel 1 (
        echo [BLAD] Konfiguracja CMake nie powiodla sie.
        %PAUSE_CMD%
        exit /b 1
    )
)

cmake --build build --config Release --parallel
if errorlevel 1 (
    echo [BLAD] Budowanie nie powiodlo sie.
    %PAUSE_CMD%
    exit /b 1
)
echo Gotowe: bin\gra.exe   (uruchom: run.cmd)
%PAUSE_CMD%
exit /b 0
