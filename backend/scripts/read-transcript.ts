// read-transcript.ts — CLI: print the latest context-window + rate-limit metrics
// from a Codex transcript. Usage: npm run transcript -- <path-to-rollout.jsonl>

import { readLatestMetrics } from '../src/transcript';

const path = process.argv[2];
if (!path) {
  console.error('usage: npm run transcript -- <transcript_path.jsonl>');
  process.exit(1);
}

const metrics = await readLatestMetrics(path);
if (!metrics) {
  console.error('no token_count metrics found in', path);
  process.exit(2);
}
console.log(JSON.stringify(metrics, null, 2));
