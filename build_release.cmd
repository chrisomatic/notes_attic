@echo off

rmdir build /S/Q
mkdir build

xcopy src\exportnotes\bin\Release\exportnotes.exe build\
xcopy src\exportnotes\exportnotes_default.cmd build\
xcopy src\exportnotes\bin\Release\Interop.Domino.dll build\
xcopy src\gui\Release\notes_attic.exe build\
xcopy src\gui\attic.png build\
xcopy redist\msvcr120.dll build\
xcopy src\gui\Release\freetype271.dll build\
xcopy /Y/E src\gui\fonts build\fonts\
::xcopy src\README.txt build\

rmdir release /S/Q
mkdir release

:: Contingent on having 7zip installed here
cd build
del ..\release\notes_attic.zip
"C:\Program Files\7-Zip\7z.exe" a -tzip ..\release\notes_attic.zip *
cd ..

:: Clean up build
rmdir build /S/Q
pause