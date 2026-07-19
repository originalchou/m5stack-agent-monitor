#!/usr/bin/env sh
# forward-hook.sh — forwards a Codex lifecycle-hook payload to the agent-monitor
# backend, then prints an empty JSON object so Codex behaviour is never altered.
#
# Codex passes the hook payload as JSON on stdin. We POST it verbatim to the
# backend, discard the HTTP response (so it can't leak onto our stdout), and
# always exit 0 — a backend that's down or slow must never block the agent.
#
# Install: copy this to ~/.codex/hooks/agent-monitor-forward.sh and `chmod +x`.
# Override the target with AGENT_MONITOR_URL if the backend isn't on :3050.

URL="${AGENT_MONITOR_URL:-http://localhost:3050}/api/hooks/codex"

curl -s -m 2 -o /dev/null \
  -X POST \
  -H "Content-Type: application/json" \
  --data-binary @- \
  "$URL" 2>/dev/null || true

# Emit a no-op result. Valid JSON satisfies events (Stop/SessionEnd) that require
# JSON on stdout, and an empty object expresses "no decision" for every event.
printf '{}'
