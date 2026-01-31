#!/usr/bin/env bash
set -Eeuo pipefail

# Inputs:
#   $LANGUAGE  : c | cpp | java | node | python
#   $ENTRY     : entry filename (e.g., main.c, Main.java, app.js)
#   $OUTDIR    : output dir inside /work/out (created)
#   $TIMEOUT   : seconds (default 10)
# Behavior:
#   - compiles deterministically where possible
#   - writes artifacts & logs to /work/out
#   - exits nonzero on failure

LANGUAGE="${LANGUAGE:-}"
ENTRY="${ENTRY:-}"
OUTDIR="/work/out"
TIMEOUT="${TIMEOUT:-10}"

mkdir -p "$OUTDIR"
exec 3>&1 4>&2
LOG="$OUTDIR/build.log"
exec 1>>"$LOG" 2>&1

echo "[info] language=$LANGUAGE entry=$ENTRY timeout=${TIMEOUT}s"

# Safety: clamp resource usage inside the container
ulimit -t "$TIMEOUT"     # CPU seconds
ulimit -v $((256*1024))  # ~256MB virtual memory
ulimit -f $((64*1024))   # max file size ~64MB
ulimit -n 256
ulimit -m $((256*1024))  # resident set (best-effort)

# Ensure entry exists in /work/src
if [[ ! -f "/work/src/$ENTRY" ]]; then
  echo "[error] missing /work/src/$ENTRY"
  exit 2
fi

# Deterministic flags
export LC_ALL=C.UTF-8
CFLAGS="-O2 -pipe -static -s -fno-asynchronous-unwind-tables -fno-ident"
CXXFLAGS="$CFLAGS"
LDFLAGS="-Wl,--build-id=none -Wl,--strip-all"

compile_c() {
  echo "[info] gcc compile"
  gcc $CFLAGS -std=c11 "/work/src/$ENTRY" -o "$OUTDIR/a.out" $LDFLAGS
  sha256sum "$OUTDIR/a.out" | tee "$OUTDIR/a.out.sha256"
}

compile_cpp() {
  echo "[info] g++ compile"
  g++ $CXXFLAGS -std=c++20 "/work/src/$ENTRY" -o "$OUTDIR/a.out" $LDFLAGS
  sha256sum "$OUTDIR/a.out" | tee "$OUTDIR/a.out.sha256"
}

compile_java() {
  echo "[info] javac compile"
  mkdir -p /work/build
  javac -g:none -encoding UTF-8 -d /work/build "/work/src/$ENTRY"
  (cd /work/build && jar --create --file "$OUTDIR/app.jar" .)
  sha256sum "$OUTDIR/app.jar" | tee "$OUTDIR/app.jar.sha256"
}

compile_node() {
  echo "[info] node bundle (no network, no install)"
  # For pure JS, "compile" = copy + hash; optional terser step if included
  cp "/work/src/$ENTRY" "$OUTDIR/app.js"
  sha256sum "$OUTDIR/app.js" | tee "$OUTDIR/app.js.sha256"
}

compile_python() {
  echo "[info] python byte-compile"
  mkdir -p /work/build
  python3 -m py_compile "/work/src/$ENTRY" || true
  cp -r /work/src "$OUTDIR/src"
  sha256sum "$OUTDIR/src/$ENTRY" | tee "$OUTDIR/src.sha256"
}

# NEW: lint function (no network, read-only src -> JSON-ish outputs)
lint_all() {
  echo "[info] linting src -> out/lint.*"

  mkdir -p "$OUTDIR"
  # ESLint JSON (only if JS/TS files exist)
  if compgen -G "/work/src/**/*.{js,jsx,ts,tsx}" > /dev/null || compgen -G "/work/src/*.{js,jsx,ts,tsx}" > /dev/null; then
    echo "[info] eslint..."
    eslint -c /etc/ai-lint/.eslintrc.json \
      -f json "/work/src" --ext .js,.jsx,.ts,.tsx \
      > "$OUTDIR/eslint.json" 2> "$OUTDIR/eslint.stderr" || true
  fi

  # Flake8 text
  if compgen -G "/work/src/**/*.py" > /dev/null || compgen -G "/work/src/*.py" > /dev/null; then
    echo "[info] flake8..."
    flake8 --config=/etc/ai-lint/.flake8 /work/src \
      > "$OUTDIR/flake8.txt" 2> "$OUTDIR/flake8.stderr" || true
  fi

  # PHPCS full report (checkstyle XML is nice, but text is ok)
  if compgen -G "/work/src/**/*.php" > /dev/null || compgen -G "/work/src/*.php" > /dev/null; then
    echo "[info] phpcs..."
    (cd /work/src && phpcs --report-full --runtime-set ignore_warnings_on_exit 1 --standard=/etc/ai-lint/phpcs.xml .) \
      > "$OUTDIR/phpcs.txt" 2> "$OUTDIR/phpcs.stderr" || true
  fi

  # Simple summary JSON
  python3 - <<'PY' > "$OUTDIR/lint.json"
import json, os, re, sys
out = {}
base = "/work/out"
# ESLint
p = os.path.join(base, "eslint.json")
if os.path.exists(p):
    try:
        data = json.load(open(p, "r", encoding="utf8"))
    except Exception:
        data = []
    issues = []
    for f in data:
        file = f.get("filePath","")
        for m in f.get("messages",[]):
            issues.append({
                "tool": "eslint",
                "file": file.replace("/work/src/",""),
                "line": m.get("line",1),
                "col": m.get("column",1),
                "rule": m.get("ruleId"),
                "severity": "error" if m.get("severity",1)==2 else "warn",
                "message": m.get("message","")
            })
    out["eslint"] = issues
# Flake8
p = os.path.join(base, "flake8.txt")
if os.path.exists(p):
    issues = []
    for line in open(p, "r", encoding="utf8"):
        line=line.strip()
        if not line: continue
        # path:row:col:CODE:message
        m = re.match(r"([^:]+):(\d+):(\d+):([A-Z]\d+):(.*)$", line)
        if not m: continue
        issues.append({
            "tool":"flake8",
            "file": m.group(1).replace("/work/src/",""),
            "line": int(m.group(2)),
            "col": int(m.group(3)),
            "rule": m.group(4),
            "severity": "warn" if m.group(4).startswith("W") else "error",
            "message": m.group(5).strip()
        })
    out["flake8"] = issues
# PHPCS
p = os.path.join(base, "phpcs.txt")
if os.path.exists(p):
    issues = []
    cur_file = None
    for line in open(p, "r", encoding="utf8"):
        line=line.rstrip()
        if line.startswith("FILE:"):
            cur_file = line.split("FILE:",1)[1].strip().replace("/work/src/","")
        elif re.match(r"^\s*\d+\s+\|\s+\d+\s+\|\s+.*", line) and cur_file:
            # best-effort parse for: line | col | message
            parts = [x.strip() for x in line.split("|",2)]
            try:
                issues.append({
                    "tool":"phpcs",
                    "file": cur_file,
                    "line": int(parts[0]),
                    "col": int(parts[1]),
                    "rule": "phpcs",
                    "severity": "warn",
                    "message": parts[2]
                })
            except Exception:
                pass
    out["phpcs"] = issues
print(json.dumps(out))
PY
  echo "[ok] lint done"
}

# No network safety net: even though container is started with --network=none, also unset proxies
unset http_proxy https_proxy HTTP_PROXY HTTPS_PROXY

# Run compilation with wall clock timeout
case "$LANGUAGE" in
  c)      timeout --preserve-status "$TIMEOUT" bash -lc compile_c ;;
  cpp)    timeout --preserve-status "$TIMEOUT" bash -lc compile_cpp ;;
  java)   timeout --preserve-status "$TIMEOUT" bash -lc compile_java ;;
  node)   timeout --preserve-status "$TIMEOUT" bash -lc compile_node ;;
  python) timeout --preserve-status "$TIMEOUT" bash -lc compile_python ;;
  lint)   timeout --preserve-status "$TIMEOUT" bash -lc lint_all ;;
  *) echo "[error] unsupported language: $LANGUAGE"; exit 3 ;;
esac

echo "[ok] done"
# also dump a mini SBOM with file(1) + sha256
( cd "$OUTDIR" && find . -type f -maxdepth 2 -print0 \
  | xargs -0 -I{} sh -c 'printf "%s :: " "{}"; file -b "{}"; sha256sum "{}"' \
) > "$OUTDIR/sbom.txt" 2>/dev/null || true

# emit result path to fd 3 (host reads)
echo "$OUTDIR" >&3
