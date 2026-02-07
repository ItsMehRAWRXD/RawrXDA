@echo off
echo Removing all .git directories to eliminate compromised tokens...

REM Remove file attributes and delete .git directories
for /d /r . %%d in (.git) do (
    if exist "%%d" (
        echo Removing %%d
        attrib -r -h -s "%%d\*.*" /s /d 2>nul
        rd /s /q "%%d" 2>nul
    )
)

echo Done! All .git directories have been removed.
echo Your source code is safe - only Git metadata was removed.
pause