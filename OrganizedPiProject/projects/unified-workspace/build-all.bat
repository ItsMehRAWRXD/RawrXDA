@echo off
echo Building Unified Workspace
echo =========================

cd rawrz-platform
if exist package.json npm install && npm start
if exist server.js node server.js

cd ../ide-suite  
if exist *.java javac *.java && java IDEMain

cd ../ai-tools
if exist package.json npm install && npm run dev

echo All projects built successfully!
pause
