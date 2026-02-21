@echo off
set root=%cd%
pushd build
cl %root%\src\main.cpp /Feminesweeper.exe -nologo -Zi /D_DEBUG
