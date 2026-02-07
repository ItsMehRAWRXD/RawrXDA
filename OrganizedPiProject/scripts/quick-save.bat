@echo off
cd /d "C:\Users\Garre\Desktop\Desktop\RawrZApp"
mkdir backup-%date:~-4,4%%date:~-10,2%%date:~-7,2% 2>nul
xcopy /s /y *.js backup-%date:~-4,4%%date:~-10,2%%date:~-7,2%\ >nul
xcopy /s /y *.json backup-%date:~-4,4%%date:~-10,2%%date:~-7,2%\ >nul
xcopy /s /y *.html backup-%date:~-4,4%%date:~-10,2%%date:~-7,2%\ >nul
echo Sources saved to backup-%date:~-4,4%%date:~-10,2%%date:~-7,2%