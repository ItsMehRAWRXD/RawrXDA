import pathlib
text = pathlib.Path('src/core/command_registry.hpp').read_text(encoding='utf-8', errors='replace')
for key in ['TERM_PWSH_LEGACY','TERM_GITBASH_LEGACY','MODEL_UNLOAD_LEGACY']:
    i = text.find(key)
    print('\n---', key, '---')
    print(repr(text[i-40:i+220]))
