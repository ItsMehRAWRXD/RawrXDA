@echo off
echo 🔐 API Key Setup Script
echo ========================

echo.
echo ⚠️  IMPORTANT: You need to rotate your compromised keys first!
echo.
echo 1. Go to OpenAI dashboard and create a NEW API key
echo 2. Go to Google Cloud Console and create a NEW API key  
echo 3. Delete the old keys you shared in chat
echo.

set /p openai_key="Enter your NEW OpenAI API key: "
set /p gemini_key="Enter your NEW Gemini API key: "

echo.
echo Setting environment variables for this session...

set OPENAI_API_KEY=%openai_key%
set GEMINI_API_KEY=%gemini_key%

echo ✅ Environment variables set!
echo.
echo 🔑 OPENAI_API_KEY = %OPENAI_API_KEY:~0,10%...
echo 🔑 GEMINI_API_KEY = %GEMINI_API_KEY:~0,10%...
echo.

echo 🚀 Now you can run the scanners:
echo    java AIEnhancedKeyscan
echo    java PracticalKeyscanHunt
echo    java RealWebKeyscan
echo.

echo 💡 To make these permanent, add them to your system environment variables
echo    or create a .env file in your project directory.
echo.

pause
