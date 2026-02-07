@echo off
:: ============================================================================
:: Auto-Pilot Protocol for GitHub Copilot
:: ============================================================================
:: This script represents a "mode" for interacting with your Copilot assistant
:: to enable autonomous, non-stop work on a complex task. To activate this
:: mode, structure your requests using the following protocol.
::
:: USAGE:
:: Start your prompt with "Activate Auto-Pilot Mode" or "Run AutoPilotMode.bat".
:: ============================================================================

:: 1. DEFINE A CLEAR OBJECTIVE
:: State the high-level goal clearly and concisely.
:: Example: "My objective is to implement a complete user authentication system."

:: 2. PROVIDE A STEP-BY-STEP PLAN
:: Break down the objective into a clear, numbered list of tasks. This will
:: become my internal TODO list.
:: Example:
:: "1. Create the database schema for the 'users' table.
::  2. Implement the backend registration endpoint.
::  3. Build the frontend login form.
::  4. Add session management with JWT."

:: 3. GRANT AUTONOMY
:: Explicitly tell me to proceed without interruption.
:: Example: "You have full autonomy to complete this plan. Do not stop to ask for
:: confirmation between steps. Only report back when the entire plan is complete
:: or if you encounter a critical error you cannot solve."

:: 4. SET THE CONTEXT
:: Tell me which files are relevant or where to start looking.
:: Example: "The main files are `AuthService.java` and `LoginPanel.java`."

:: ============================================================================
:: By following this protocol, you enable your Copilot to function as a true
:: autonomous agent, executing complex tasks from start to finish.
:: ============================================================================
echo Auto-Pilot Mode protocol has been defined.
echo To use, start your prompt with "Activate Auto-Pilot Mode" and follow the protocol.
