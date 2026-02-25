struct CommandEntry { const char* name; void(*handler)(); };
void handleBootTest() { MessageBoxA(NULL, "Boot test", "RawrXD", MB_OK); }
CommandEntry COMMAND_TABLE[] = { {"boot_test", handleBootTest} };
