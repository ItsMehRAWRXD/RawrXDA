// ============================================================================
// Win32IDE_SlashRouter.cpp
//
// Minimal slash router thunk used by CLI entry points (e.g. --slash).
// This intentionally stays decoupled from UI-only Win32IDE internals.
// ============================================================================

#include <string>

bool RawrXD_SlashRouter_TryRoute(const std::string& commandText, std::string& outputText)
{
	outputText.clear();

	if (commandText.empty())
	{
		outputText = "empty slash command";
		return false;
	}

	if (commandText == "/ping")
	{
		outputText = "pong";
		return true;
	}

	if (commandText == "/help")
	{
		outputText = "supported slash commands: /help, /ping, /echo <text>";
		return true;
	}

	if (commandText.rfind("/echo ", 0) == 0)
	{
		outputText = commandText.substr(6);
		return true;
	}

	outputText = "unknown slash command: " + commandText;
	return false;
}
