// transcript.ts — pull current context-window + rate-limit metrics out of a Codex
// session transcript (the JSONL at `transcript_path`).
//
// The transcript contains `event_msg` records whose payload.type === "token_count".
// The LAST such record reflects the current state:
//   payload.info.model_context_window        - the model's context size
//   payload.info.last_token_usage.input_tokens - tokens in the most recent request
//   payload.rate_limits.primary / .secondary - { used_percent, window_minutes, resets_at }
//
// Reads are cached by (size + mtime) so refreshing on every hook event is cheap.

import { promises as fs } from 'node:fs';

export interface RateLimitWindow {
  usedPercent: number;
  windowMinutes: number;
  resetsAt: number; // epoch seconds
}

export interface TranscriptMetrics {
  contextWindow: number | null;
  contextUsedTokens: number | null;
  contextLeftPercent: number | null;
  rateLimits: {
    primary: RateLimitWindow | null;
    secondary: RateLimitWindow | null;
  };
  planType: string | null;
}

interface CacheEntry {
  size: number;
  mtimeMs: number;
  metrics: TranscriptMetrics | null;
}

const cache = new Map<string, CacheEntry>();

function clampPercent(n: number): number {
  return Math.max(0, Math.min(100, Math.round(n)));
}

function normalizeWindow(w: unknown): RateLimitWindow | null {
  if (!w || typeof w !== 'object') return null;
  const o = w as Record<string, unknown>;
  if (typeof o.used_percent !== 'number') return null;
  return {
    usedPercent: o.used_percent,
    windowMinutes: typeof o.window_minutes === 'number' ? o.window_minutes : 0,
    resetsAt: typeof o.resets_at === 'number' ? o.resets_at : 0,
  };
}

function parseMetrics(rec: any): TranscriptMetrics {
  const info = rec?.payload?.info ?? {};
  const rl = rec?.payload?.rate_limits ?? {};
  const contextWindow = typeof info.model_context_window === 'number' ? info.model_context_window : null;
  const used =
    typeof info?.last_token_usage?.input_tokens === 'number'
      ? info.last_token_usage.input_tokens
      : typeof info?.last_token_usage?.total_tokens === 'number'
        ? info.last_token_usage.total_tokens
        : null;
  const contextLeftPercent =
    contextWindow && used != null ? clampPercent((1 - used / contextWindow) * 100) : null;
  return {
    contextWindow,
    contextUsedTokens: used,
    contextLeftPercent,
    rateLimits: {
      primary: normalizeWindow(rl.primary),
      secondary: normalizeWindow(rl.secondary),
    },
    planType: typeof rl.plan_type === 'string' ? rl.plan_type : null,
  };
}

/** Scan the file's lines in reverse for the last `token_count` record. */
function extractFromText(text: string): TranscriptMetrics | null {
  const lines = text.split('\n');
  for (let i = lines.length - 1; i >= 0; i--) {
    const line = lines[i]?.trim();
    if (!line || !line.includes('"token_count"')) continue;
    try {
      const rec = JSON.parse(line);
      if (rec?.payload?.type === 'token_count') return parseMetrics(rec);
    } catch {
      // skip malformed / partially-written trailing line
    }
  }
  return null;
}

export async function readLatestMetrics(
  transcriptPath: string | null | undefined,
): Promise<TranscriptMetrics | null> {
  if (!transcriptPath) return null;
  let stat;
  try {
    stat = await fs.stat(transcriptPath);
  } catch {
    return null; // transcript not readable
  }
  const cached = cache.get(transcriptPath);
  if (cached && cached.size === stat.size && cached.mtimeMs === stat.mtimeMs) {
    return cached.metrics;
  }
  let metrics: TranscriptMetrics | null = null;
  try {
    metrics = extractFromText(await fs.readFile(transcriptPath, 'utf8'));
  } catch {
    metrics = null;
  }
  cache.set(transcriptPath, { size: stat.size, mtimeMs: stat.mtimeMs, metrics });
  return metrics;
}
