@echo off

rmdir build /S/Q
mkdir build

xcopy src\exportnotes\bin\Release\exportnotes.exe build\
xcopy src\exportnotes\exportnotes_default.cmd build\
xcopy src\exportnotes\bin\Release\Interop.Domino.dll build\
xcopy src\gui\dx11\Release\notes_attic.exe build\
xcopy src\gui\dx11\attic.ico build\
xcopy src\gui\dx11\Release\freetype271.dll build\
xcopy dependencies\* build\
xcopy /Y/E fonts build\fonts\
xcopy README.txt build\

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