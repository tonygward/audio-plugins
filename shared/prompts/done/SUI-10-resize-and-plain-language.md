---
id: SUI-10
priority: 3
model: sonnet
effort: medium
dependencies: ["SUI-09"]
conflicts: []
---

# SUI-10 · Resizable editors and clear control help

**Recommended model:** Sonnet.  
**Estimated effort / usage:** Medium (relative, not a token estimate).  
**Model escalation:** Opus only if a significant shared editor architecture decision is unavoidable. Never switch to Opus silently: record why, scope its use to the difficult design/validation step, then return to Sonnet.  

Follow `CLAUDE.md` and `docs/style-guide.md`. Use the EQ as the resizing reference. Audit the three current plugins, with the Analog Compressor as the priority, and use the same acceptance standard for new plugins.

- Support live resizing by dragging the editor where the host permits it, with sensible minimum and maximum sizes and an interface size control where useful. Keep the layout legible, crisp and usable across the supported range, including the existing 75–200% scale options where available. Persist the editor size through reopening and session recall. Loading a sound preset should keep the current window size.
- Use short, plain labels that describe the job. Give every interactive control a concise, plain-language tooltip explaining what changes when used, including icon buttons, menus, meter selectors and resize controls. Match accessible names to the visible labels. Keep established parameter IDs stable.
- Use **All** on the Analog Compressor's ratio panel. Explain the action in its tooltip. Update any conflicting shared wording rule and tests for this specific label.
- Verify resize, hit targets, tooltips, no clipping and session recall in a real host and standalone builds at supported display scales on Windows and macOS. Capture before and after screenshots and extend existing layout and wording checks.


## Lifecycle update — 10 November 2026 (authoritative)

This section supersedes older scheduling/status/completion wording in this prompt. Follow [the current execution policy](https://github.com/tonygward/audio-plugins/blob/main/docs/songsmith-execution-policy.md).

- [AUTO-11 #175](https://github.com/tonygward/audio-plugins/issues/175): board statuses, verified visible manual order, atomic claim and handoff. Todo is ready. Doing is a run-now request, picked up ahead of Todo unless the prompt is deferred; once picked up it is claimed with a start comment. Blocked is excluded from automatic pickup.
- [AUTO-12 #180](https://github.com/tonygward/audio-plugins/issues/180): standard bounded Claude task loop. Defaults 120 minutes elapsed/run, three total repair iterations and an explicit finite configured Claude turn/token budget. One task/run, one active job; verified weekly/scoped usage below 60%, unknown/stale blocks new agent work.
- On limit/blocker, save branch/SHA/PR/checks/delivery stage, comment with progress, actual reason, remaining work and resume action, leave open and set Blocked. No automatic next-poll retry. Explicit Todo/run-now resumes saved work without duplicate coding or publication.
- [AUTO-10 #157](https://github.com/tonygward/audio-plugins/issues/157): trusted delivery and finalisation. Exact-head required checks and relevant native CI precede automatic merge. Plugin changes require release, both suite installers, private R2 and matching authenticated beta downloads. Website-only tasks verify live deployment; infrastructure-only tasks verify deployed runtime/workflows; no unrelated plugin release. Parent epics/historical trackers are not additional jobs.
- Final task-appropriate acceptance evidence comment must succeed before issue close and board Done. A failed board update after close retries finalisation only. Keep merged work intact while delivery is pending. Prompt archives alone are not delivery evidence.
- Qualifying plugin main builds automatically increment only the third beta VERSION component. Major/minor increments deferred; documentation-only changes do not bump or publish.

This archived implementation remains historical and must not be requeued; reconcile delivery evidence only.
