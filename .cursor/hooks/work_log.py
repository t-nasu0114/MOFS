#!/usr/bin/env python3
"""MOFS work-log hook: append session events under scratch/work_logs/."""

from __future__ import annotations

import json
import os
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

PROMPT_MAX = 2000
RESPONSE_MAX = 2000
THOUGHT_SUMMARY_MAX = 800
JJ_DIFF_STAT_MAX = 1500
JJ_DIFF_MAX = 4000


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def work_logs_root() -> Path:
    return repo_root() / "scratch" / "work_logs"


def now_iso() -> str:
    return datetime.now(timezone.utc).astimezone().isoformat(timespec="seconds")


def today_stamp() -> str:
    return datetime.now().strftime("%Y-%m-%d")


def read_stdin() -> dict[str, Any]:
    raw = sys.stdin.read()
    if not raw.strip():
        return {}
    try:
        data = json.loads(raw)
    except json.JSONDecodeError:
        return {}
    return data if isinstance(data, dict) else {}


def emit(obj: dict[str, Any] | None = None) -> None:
    sys.stdout.write(json.dumps(obj if obj is not None else {}, ensure_ascii=False))
    sys.stdout.flush()


def truncate_text(text: str, limit: int) -> tuple[str, bool]:
    text = text or ""
    if len(text) <= limit:
        return text, False
    return text[:limit] + "…(truncated)", True


def summarize_thought(text: str) -> dict[str, Any]:
    original = text or ""
    lines = original.count("\n") + (1 if original else 0)
    chars = len(original)
    collapsed = re.sub(r"\s+", " ", original).strip()
    summary, truncated = truncate_text(collapsed, THOUGHT_SUMMARY_MAX)
    return {
        "summary": summary,
        "chars": chars,
        "lines": lines,
        "truncated": truncated or chars > THOUGHT_SUMMARY_MAX,
    }


def resolve_session_id(payload: dict[str, Any]) -> str:
    for key in ("conversation_id", "session_id"):
        value = payload.get(key)
        if isinstance(value, str) and value.strip():
            return value.strip()
    env = os.environ.get("MOFS_WORK_LOG_SESSION", "").strip()
    if env:
        return env
    return "unknown"


def session_dir(session_id: str) -> Path:
    safe = re.sub(r"[^A-Za-z0-9._-]+", "_", session_id)[:120] or "unknown"
    path = work_logs_root() / "sessions" / safe
    path.mkdir(parents=True, exist_ok=True)
    return path


def append_event(session_id: str, event: dict[str, Any]) -> None:
    path = session_dir(session_id) / "events.jsonl"
    event = {"ts": now_iso(), **event}
    with path.open("a", encoding="utf-8") as fh:
        fh.write(json.dumps(event, ensure_ascii=False) + "\n")


def load_meta(session_id: str) -> dict[str, Any]:
    path = session_dir(session_id) / "meta.json"
    if not path.exists():
        return {}
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        return {}
    return data if isinstance(data, dict) else {}


def save_meta(session_id: str, meta: dict[str, Any]) -> None:
    path = session_dir(session_id) / "meta.json"
    path.write_text(
        json.dumps(meta, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def has_edit_events(session_id: str) -> bool:
    path = session_dir(session_id) / "events.jsonl"
    if not path.exists():
        return False
    try:
        with path.open(encoding="utf-8") as fh:
            for line in fh:
                line = line.strip()
                if not line:
                    continue
                try:
                    ev = json.loads(line)
                except json.JSONDecodeError:
                    continue
                if ev.get("type") == "file_edit":
                    return True
    except OSError:
        return False
    return False


def rel_path(file_path: str) -> str:
    try:
        return str(Path(file_path).resolve().relative_to(repo_root()))
    except Exception:
        return file_path


def daily_log_rel() -> str:
    return f"scratch/work_logs/{today_stamp()}.md"


def run_jj(args: list[str]) -> tuple[int, str]:
    try:
        result = subprocess.run(
            ["jj", *args],
            cwd=repo_root(),
            capture_output=True,
            text=True,
            timeout=30,
            check=False,
        )
        output = (result.stdout or "") + (result.stderr or "")
        return result.returncode, output.strip()
    except (OSError, subprocess.TimeoutExpired) as exc:
        return -1, str(exc)


def collect_jj_diff_summary() -> dict[str, Any]:
    rc_stat, stat = run_jj(["diff", "--stat"])
    rc_diff, diff = run_jj(["diff"])
    jj_available = rc_stat == 0 and rc_diff == 0

    has_changes = bool(stat and "0 files changed" not in stat)
    if not has_changes and diff:
        has_changes = True

    stat_text, stat_truncated = truncate_text(stat, JJ_DIFF_STAT_MAX)
    diff_text, diff_truncated = truncate_text(diff, JJ_DIFF_MAX)
    return {
        "has_changes": has_changes,
        "stat": stat_text,
        "diff": diff_text,
        "stat_truncated": stat_truncated,
        "diff_truncated": diff_truncated,
        "jj_available": jj_available,
    }


def build_stop_followup(jj_summary: dict[str, Any]) -> str:
    parts = [
        "作業記録を更新してください。",
        f"`{daily_log_rel()}` に今回の作業エントリを追記し、",
        "目的・決定・実施・検討要約・残課題を短く書いてください。",
    ]
    if jj_summary.get("has_changes"):
        parts.extend(
            [
                "",
                "以下は `jj diff` の要約です。Agent 編集だけでなく手動変更も含めて説明に反映してください。",
            ]
        )
        if jj_summary.get("stat"):
            parts.extend(["", "```", jj_summary["stat"], "```"])
        if jj_summary.get("diff"):
            parts.extend(["", "```diff", jj_summary["diff"], "```"])
    parts.extend(
        [
            "",
            "MOFS リポジトリには `scratch/work_logs/` をコミットしないでください。",
            "チャット返答は短くて構いません。",
        ]
    )
    return "\n".join(parts)


def handle_session_start(payload: dict[str, Any]) -> dict[str, Any]:
    session_id = resolve_session_id(payload)
    meta = {
        "session_id": session_id,
        "started_at": now_iso(),
        "composer_mode": payload.get("composer_mode"),
        "is_background_agent": payload.get("is_background_agent"),
        "model": payload.get("model"),
    }
    save_meta(session_id, meta)
    append_event(session_id, {"type": "session_start", **meta})

    context = (
        "MOFS work logging is active.\n"
        f"- Append structured notes to `{daily_log_rel()}` at meaningful milestones.\n"
        "- Never commit `scratch/work_logs/` into the MOFS repository.\n"
        "- Archive pushes use `scripts/work_logs_push.sh` only (separate repo)."
    )
    return {
        "env": {"MOFS_WORK_LOG_SESSION": session_id},
        "additional_context": context,
    }


def handle_before_submit_prompt(payload: dict[str, Any]) -> dict[str, Any]:
    session_id = resolve_session_id(payload)
    prompt = payload.get("prompt") or ""
    text, truncated = truncate_text(str(prompt), PROMPT_MAX)
    append_event(
        session_id,
        {
            "type": "prompt",
            "text": text,
            "truncated": truncated,
            "chars": len(str(prompt)),
        },
    )
    return {"continue": True}


def handle_after_agent_thought(payload: dict[str, Any]) -> dict[str, Any]:
    session_id = resolve_session_id(payload)
    summary = summarize_thought(str(payload.get("text") or ""))
    append_event(
        session_id,
        {
            "type": "thought_summary",
            "summary": summary["summary"],
            "chars": summary["chars"],
            "lines": summary["lines"],
            "truncated": summary["truncated"],
            "duration_ms": payload.get("duration_ms"),
        },
    )
    return {}


def handle_after_file_edit(payload: dict[str, Any]) -> dict[str, Any]:
    session_id = resolve_session_id(payload)
    file_path = str(payload.get("file_path") or "")
    edits = payload.get("edits") or []
    edit_count = len(edits) if isinstance(edits, list) else 0
    append_event(
        session_id,
        {
            "type": "file_edit",
            "path": rel_path(file_path),
            "edit_count": edit_count,
        },
    )
    meta = load_meta(session_id)
    meta["has_file_edits"] = True
    save_meta(session_id, meta)
    return {}


def handle_after_agent_response(payload: dict[str, Any]) -> dict[str, Any]:
    session_id = resolve_session_id(payload)
    text = str(payload.get("text") or "")
    clipped, truncated = truncate_text(text, RESPONSE_MAX)
    append_event(
        session_id,
        {
            "type": "response",
            "text": clipped,
            "truncated": truncated,
            "chars": len(text),
        },
    )
    return {}


def handle_stop(payload: dict[str, Any]) -> dict[str, Any]:
    session_id = resolve_session_id(payload)
    status = payload.get("status") or ""
    loop_count = payload.get("loop_count") or 0
    try:
        loop_count = int(loop_count)
    except (TypeError, ValueError):
        loop_count = 0

    append_event(
        session_id,
        {
            "type": "stop",
            "status": status,
            "loop_count": loop_count,
        },
    )

    marker = session_dir(session_id) / ".summary_requested"
    meta = load_meta(session_id)
    edited = bool(meta.get("has_file_edits")) or has_edit_events(session_id)
    jj_summary = collect_jj_diff_summary()
    append_event(
        session_id,
        {
            "type": "jj_diff_summary",
            "has_changes": jj_summary.get("has_changes"),
            "stat": jj_summary.get("stat"),
            "diff": jj_summary.get("diff"),
            "stat_truncated": jj_summary.get("stat_truncated"),
            "diff_truncated": jj_summary.get("diff_truncated"),
            "jj_available": jj_summary.get("jj_available"),
        },
    )

    has_work = edited or bool(jj_summary.get("has_changes"))
    if (
        status == "completed"
        and loop_count == 0
        and has_work
        and not marker.exists()
    ):
        marker.write_text(now_iso() + "\n", encoding="utf-8")
        return {"followup_message": build_stop_followup(jj_summary)}
    return {}


def handle_session_end(payload: dict[str, Any]) -> dict[str, Any]:
    session_id = resolve_session_id(payload)
    meta = load_meta(session_id)
    meta.update(
        {
            "ended_at": now_iso(),
            "reason": payload.get("reason"),
            "duration_ms": payload.get("duration_ms"),
            "final_status": payload.get("final_status"),
            "error_message": payload.get("error_message"),
            "is_background_agent": payload.get("is_background_agent"),
        }
    )
    save_meta(session_id, meta)
    append_event(
        session_id,
        {
            "type": "session_end",
            "reason": payload.get("reason"),
            "duration_ms": payload.get("duration_ms"),
            "final_status": payload.get("final_status"),
        },
    )
    return {}


HANDLERS = {
    "sessionStart": handle_session_start,
    "beforeSubmitPrompt": handle_before_submit_prompt,
    "afterAgentThought": handle_after_agent_thought,
    "afterFileEdit": handle_after_file_edit,
    "afterAgentResponse": handle_after_agent_response,
    "stop": handle_stop,
    "sessionEnd": handle_session_end,
}


def main() -> int:
    if len(sys.argv) < 2:
        emit({})
        return 0
    event = sys.argv[1]
    payload = read_stdin()
    handler = HANDLERS.get(event)
    try:
        if handler is None:
            emit({})
            return 0
        result = handler(payload)
        emit(result if isinstance(result, dict) else {})
        return 0
    except Exception as exc:  # noqa: BLE001 - fail open for hooks
        sys.stderr.write(f"[work_log] {event} failed: {exc}\n")
        emit({})
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
