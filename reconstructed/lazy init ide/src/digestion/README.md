# Digestion Module Core

Stage 1 delivers the digestion core: configuration, orchestration, metrics, and SQLite persistence.

## Configuration
Supports JSON and YAML. Example:

```json
{
  "digestion": {
    "chunk_size": 65536,
    "threads": 0,
    "flags": ["DEFLATE", "CRC", "REVERSE_STREAM"],
    "apply_fixes": false,
    "create_backups": true,
    "incremental": true,
    "backup_dir": ".digest_backups"
  },
  "database": {
    "path": "digestion_results.db",
    "schema": "digestion_schema.sql",
    "enabled": true
  }
}
```

## Database
Schema is in `digestion_schema.sql`. If not found, the system falls back to the embedded schema.

## Build (subproject)
```powershell
cmake -S "d:\lazy init ide\src\digestion" -B "d:\lazy init ide\build\digestion"
cmake --build "d:\lazy init ide\build\digestion" --config Release
```
