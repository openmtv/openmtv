@echo off
rem Play Mad TV on Windows -- what ./play.sh does on Linux and macOS.
rem   play.cmd               the launcher: the page "Play" in its own window (packs,
rem                          settings, start, multiplayer; the content editor in it)
rem   play.cmd editor        the launcher, at the content editor
rem   play.cmd --direct      the game at once: the decompiled build (build\mtv-dec.exe,
rem                          make win) with the settings in %APPDATA%\madtv\settings.ini
rem   play.cmd d / e, --classic, --fixes=a,b, --mod NAME, --fullscreen, ...
rem                          the game at once, as ./play.sh takes them
rem   play.cmd -h / --help   the options
rem Everything goes through build\openmtv.exe (launcher\, in Go: make launcher-win), without
rem Python -- a program of the window subsystem (no console window of its own: with
rem arguments it writes to this one's, and the game plays in it).  SOURCE1.RSC and
rem SOURCE2.RSC in run\ or one directory up (or set GAME=C:\path\to\the\game\files).
setlocal
if not exist "%~dp0build\openmtv.exe" (
  echo OpenMTV: build\openmtv.exe is not built -- make launcher-win 1>&2
  exit /b 2
)
if "%~1"=="" (
  start "" "%~dp0build\openmtv.exe"
  exit /b 0
)
"%~dp0build\openmtv.exe" %*
