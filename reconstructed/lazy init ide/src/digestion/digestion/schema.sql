CREATE TABLE IF NOT EXISTS digestion_runs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    root_dir TEXT NOT NULL,
    start_ms INTEGER,
    end_ms INTEGER,
    elapsed_ms INTEGER,
    total_files INTEGER,
    scanned_files INTEGER,
    stubs_found INTEGER,
    fixes_applied INTEGER,
    bytes_processed INTEGER,
    created_at TEXT DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS digestion_reports (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id INTEGER NOT NULL,
    report_json TEXT,
    FOREIGN KEY(run_id) REFERENCES digestion_runs(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS digestion_files (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id INTEGER NOT NULL,
    path TEXT NOT NULL,
    language TEXT,
    size_bytes INTEGER,
    stubs_found INTEGER,
    hash TEXT,
    FOREIGN KEY(run_id) REFERENCES digestion_runs(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS digestion_tasks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    file_id INTEGER NOT NULL,
    line_number INTEGER,
    stub_type TEXT,
    context TEXT,
    suggested_fix TEXT,
    confidence TEXT,
    applied INTEGER,
    backup_id TEXT,
    FOREIGN KEY(file_id) REFERENCES digestion_files(id) ON DELETE CASCADE
);
