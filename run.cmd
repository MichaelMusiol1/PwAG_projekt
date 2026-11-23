:; # ============================================================================
:; # run.cmd - buduje (tylko zmienione pliki) i uruchamia gre.
:; # Windows: dwuklik albo "run.cmd".   Linux: "sh run.cmd"
:; # Gra startuje z katalogu projektu, bo czyta pliki z data/ i shaders/.
:; # ============================================================================
:; cd "$(dirname "$0")" || exit 1
:; sh ./build.cmd || exit 1
:; exec ./bin/gra "$@"
@echo off
rem ---------------------------- Windows ----------------------------------------
setlocal
cd /d "%~dp0"
call build.cmd nopause
if errorlevel 1 (
    pause
    exit /b 1
)
bin\gra.exe %*
if errorlevel 1 pause
