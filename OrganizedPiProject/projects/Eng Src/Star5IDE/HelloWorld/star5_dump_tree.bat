@echo off
setlocal
echo Dumping repo tree to _tree.txt ...
tree /F /A > _tree_raw.txt

REM filter out noisy folders
type _tree_raw.txt | findstr /V /I "\\node_modules\\" | findstr /V /I "\\.git\\" | findstr /V /I "\\dist\\" | findstr /V /I "\\out\\" | findstr /V /I "\\bin\\" | findstr /V /I "\\obj\\" > _tree.txt
del _tree_raw.txt

echo Done. See _tree.txt
endlocal
