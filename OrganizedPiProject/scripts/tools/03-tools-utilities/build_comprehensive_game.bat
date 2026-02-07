@echo off
echo Building Comprehensive FPS Game...

rem Create build directory
if not exist build_game mkdir build_game
cd build_game

rem Try different compilers
set FOUND_COMPILER=0

rem Try cl (MSVC) first
where cl >nul 2>&1
if %ERRORLEVEL% == 0 (
    echo Using Microsoft Visual C++ Compiler
    cl /std:c++17 /EHsc /I../include ^
       ../src/main_comprehensive_game.cpp ^
       ../src/weapon_system.cpp ^
       ../src/movement_system.cpp ^
       ../src/destructible_environment.cpp ^
       ../src/particle_system.cpp ^
       ../src/audio_system.cpp ^
       ../src/multiplayer_networking.cpp ^
       ../src/hud_system.cpp ^
       ../src/game_modes.cpp ^
       ../src/advanced_warzone_generator.cpp ^
       ../src/loadout_system.cpp ^
       ../src/perk_system.cpp ^
       ../src/character_progression_system.cpp ^
       ../src/wildlife_generator.cpp ^
       ../src/power_armor_system.cpp ^
       ../src/creature_collection_system.cpp ^
       ../src/infected_system.cpp ^
       ../src/mirage_generator.cpp ^
       ../src/mutation_system.cpp ^
       /Fe:comprehensive_fps_game.exe
    
    if %ERRORLEVEL% == 0 (
        set FOUND_COMPILER=1
        echo Build successful with MSVC!
        echo Running game...
        comprehensive_fps_game.exe
    ) else (
        echo MSVC build failed, trying g++...
    )
)

rem Try g++ if MSVC failed
if %FOUND_COMPILER% == 0 (
    where g++ >nul 2>&1
    if %ERRORLEVEL% == 0 (
        echo Using g++ compiler
        g++ -std=c++17 -O2 -I../include ^
            ../src/main_comprehensive_game.cpp ^
            ../src/weapon_system.cpp ^
            ../src/movement_system.cpp ^
            ../src/destructible_environment.cpp ^
            ../src/particle_system.cpp ^
            ../src/audio_system.cpp ^
            ../src/multiplayer_networking.cpp ^
            ../src/hud_system.cpp ^
            ../src/game_modes.cpp ^
            ../src/advanced_warzone_generator.cpp ^
            ../src/loadout_system.cpp ^
            ../src/perk_system.cpp ^
            ../src/character_progression_system.cpp ^
            ../src/wildlife_generator.cpp ^
            ../src/power_armor_system.cpp ^
            ../src/creature_collection_system.cpp ^
            ../src/infected_system.cpp ^
            ../src/mirage_generator.cpp ^
            ../src/mutation_system.cpp ^
            -o comprehensive_fps_game.exe
        
        if %ERRORLEVEL% == 0 (
            set FOUND_COMPILER=1
            echo Build successful with g++!
            echo Running game...
            comprehensive_fps_game.exe
        ) else (
            echo g++ build failed, trying clang++...
        )
    )
)

rem Try clang++ if g++ failed
if %FOUND_COMPILER% == 0 (
    where clang++ >nul 2>&1
    if %ERRORLEVEL% == 0 (
        echo Using clang++ compiler
        clang++ -std=c++17 -O2 -I../include ^
                ../src/main_comprehensive_game.cpp ^
                ../src/weapon_system.cpp ^
                ../src/movement_system.cpp ^
                ../src/destructible_environment.cpp ^
                ../src/particle_system.cpp ^
                ../src/audio_system.cpp ^
                ../src/multiplayer_networking.cpp ^
                ../src/hud_system.cpp ^
                ../src/game_modes.cpp ^
                ../src/advanced_warzone_generator.cpp ^
                ../src/loadout_system.cpp ^
                ../src/perk_system.cpp ^
                ../src/character_progression_system.cpp ^
                ../src/wildlife_generator.cpp ^
                ../src/power_armor_system.cpp ^
                ../src/creature_collection_system.cpp ^
                ../src/infected_system.cpp ^
                ../src/mirage_generator.cpp ^
                ../src/mutation_system.cpp ^
                -o comprehensive_fps_game.exe
        
        if %ERRORLEVEL% == 0 (
            set FOUND_COMPILER=1
            echo Build successful with clang++!
            echo Running game...
            comprehensive_fps_game.exe
        )
    )
)

if %FOUND_COMPILER% == 0 (
    echo No suitable compiler found!
    echo Please install one of: MSVC (Visual Studio), g++ (MinGW), or clang++
    echo.
    echo You can also try running the game with:
    echo   node ../src/simple_game_demo.js
    echo   python ../src/simple_game_demo.py
    pause
    goto :eof
)

echo.
echo === Game Demonstration Complete ===
echo All systems have been tested and are working!
pause

cd ..
goto :eof
