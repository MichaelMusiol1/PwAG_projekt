:; # Buduje (tylko zmienione pliki) i uruchamia gre. Windows: dwuklik albo "run.cmd". Linux: "sh run.cmd"
:; # Ta sama sztuczka z ":;" co w build.cmd. Gra startuje z katalogu projektu, bo czyta data/ i shaders/.
:; cd "$(dirname "$0")" || exit 1
:; sh ./build.cmd || exit 1
:; exec ./bin/gra "$@"
@echo off
setlocal
cd /d "%~dp0"
call build.cmd nopause
if errorlevel 1 (
    pause
    exit /b 1
)
rem pause przy bledzie, zeby okno nie zniknelo, zanim ktos przeczyta komunikat
bin\gra.exe %*
if errorlevel 1 pause
