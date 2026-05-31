import re, pathlib
text = pathlib.Path('src/core/command_registry.hpp').read_text(encoding='utf-8', errors='replace')
pat = re.compile(r'X\(\s*(\d+)\s*,\s*(\w+)\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*(\w+)\s*,\s*"([^"]*)"\s*,\s*(\w+)\s*,\s*([^)]+)\)', re.MULTILINE)
entries = [m.group(2) for m in pat.finditer(text)]
for sym in ['TERM_NEW_LEGACY','TERM_PWSH_LEGACY','TERM_GITBASH_LEGACY','TERM_RUN_TASK_LEGACY','TERM_RUN_FILE_LEGACY','TERM_CLEAR_LEGACY','TERM_KILL_LEGACY','TERMINAL_POWERSHELL_LEGACY','TERMINAL_CMD_LEGACY2','TERMINAL_STOP_LEGACY2','TERMINAL_NEW_USER_LEGACY','TERMINAL_NEW_AGENT_LEGACY','TERMINAL_FOCUS_INT_LEGACY','TERMINAL_CLEAR_ALL_LEGACY','MODEL_LOAD_LEGACY','MODEL_UNLOAD_LEGACY','MODEL_INFO_LEGACY','MODEL_RELOAD_LEGACY','MODEL_SETTINGS_LEGACY']:
    print(sym, sym in entries)
