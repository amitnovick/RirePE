@echo off
set PROJECTNAME=%1
REM Copy the built library from the solution's Release folder to the parent Simple directory
REM The lib is at $(SolutionDir)Release\Simple.lib, which is ../Release/ relative to the project
xcopy "..\Release\%PROJECTNAME%.lib" ".." /Y