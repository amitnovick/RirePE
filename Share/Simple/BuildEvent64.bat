@echo off
set PROJECTNAME=%1
REM Simple64 outputs to $(SolutionDir)Release\ which is one level up from this project
xcopy "..\Release\%PROJECTNAME%.lib" ".." /Y