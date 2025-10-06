@echo off
set PROJECTNAME=%1
REM Copy the built library from the project's Release folder to the parent Simple directory
xcopy "Release\%PROJECTNAME%.lib" ".." /Y