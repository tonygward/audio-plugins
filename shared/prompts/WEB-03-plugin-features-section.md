---
id: WEB-03
status: ready
execution: deferred
priority: 1
model: sonnet
effort: low
dependencies: []
conflicts: []
---

# WEB-03 — Add Features after What's New on beta website

**Source request:** [WEB-03 website issue](https://github.com/tonygward/songsmith-website/issues/19). **Owner requested RUN NOW.**

**Execution routing:** WEB tasks are excluded by the functional Matrix worker's code-level allowlist. This prompt is deliberately `deferred` for that worker, not a silent bypass. **Run immediately through a separately authorised website/portal development session**, with its own test/deploy verification. Do not change the worker allowlist just to pick up this task.

## Target
Website repository: `tonygward/songsmith-website`. Existing `public/index.html` order is Latest release → What's New → Getting Started. Insert a visually consistent **Features** / **Meet the Plugins** section **between What's New and Getting Started**.

## Content
Provide concise, musician-friendly overviews of currently available beta plugins, verifying implemented feature sets instead of advertising roadmap-only capabilities:
- **Songsmith EQ:** shape frequencies and tonal balance; describe implemented EQ features accurately.
- **Songsmith Analog Compressor:** control dynamics with classic analogue-style compression character.
- **Songsmith Maximiser:** control peaks and increase perceived loudness in a transparent, useful way.
Keep copy short (1–3 sentences per plugin), avoid invented features or product promises, and only describe a plugin as downloadable when it is actually released.

## Acceptance criteria
- Section order: **What's New → Features → Getting Started**.
- Accurate, clear overview of each currently distributed plugin.
- Responsive layout consistent with website branding; semantic headings and accessibility.
- Existing release status, What's New rendering, downloads and Cloudflare Access unaffected.
- Tests pass; deploy only through existing website process, verify live site and report PR/deploy details.
