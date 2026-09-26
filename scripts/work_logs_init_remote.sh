#!/usr/bin/env bash
# Initialize scratch/work_logs as a nested git repo pointing at a separate remote.
set -euo pipefail

usage() {
  echo "Usage: $0 <git-remote-url>" >&2
  echo "Example: $0 git@github.com:USER/mofs-work-logs.git" >&2
  exit 2
}

if [[ $# -ne 1 ]]; then
  usage
fi

REMOTE_URL="$1"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LOG_DIR="$ROOT/scratch/work_logs"

mkdir -p "$LOG_DIR/sessions"

if [[ ! -f "$LOG_DIR/README.md" ]]; then
  cat > "$LOG_DIR/README.md" <<'EOF'
# MOFS work logs

Private archive of MOFS agent work logs.

- Daily summaries: `YYYY-MM-DD.md`
- Session event streams: `sessions/<session_id>/`

This repository is separate from MOFS. Do not merge these files into the MOFS tree.
EOF
fi

if [[ ! -f "$LOG_DIR/.gitignore" ]]; then
  cat > "$LOG_DIR/.gitignore" <<'EOF'
# Session markers (local hook state)
**/.summary_requested
EOF
fi

if [[ -d "$LOG_DIR/.git" ]]; then
  echo "Already a git repo: $LOG_DIR" >&2
else
  git -C "$LOG_DIR" init -b main
fi

if git -C "$LOG_DIR" remote get-url origin >/dev/null 2>&1; then
  git -C "$LOG_DIR" remote set-url origin "$REMOTE_URL"
else
  git -C "$LOG_DIR" remote add origin "$REMOTE_URL"
fi

git -C "$LOG_DIR" add -A
if git -C "$LOG_DIR" diff --cached --quiet; then
  echo "Nothing to commit for initial snapshot." >&2
else
  git -C "$LOG_DIR" commit -m "work logs: initial"
fi

git -C "$LOG_DIR" push -u origin HEAD
echo "Initialized work logs remote: $REMOTE_URL"
