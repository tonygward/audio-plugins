# Songsmith backlog: issue and prompt template

When a task is ready to run, and when a prompt has to exist, is set by the [execution policy](songsmith-execution-policy.md#1-backlog-issues-and-prompts). This page only says what an issue and a prompt contain.

## The issue

- **Title** starts with the stable ID: `ACOMP-16: Continuous Ignore Below up to 500 Hz`. Search `audio-plugins`, `songsmithstudio` and `songsmith-website` for the ID and for similar work first, and update what exists instead of duplicating it.
- **Body**: enough for Matrix to build the prompt when there is no prompt file. Use these headings, each with real text:
  - `## Objective` (and the user value)
  - `## Scope` or `## Implementation`
  - `## Acceptance criteria`
  - `## Tests` (a combined `## Acceptance criteria and tests` counts for both)
- If a prompt file exists, link it near the top of the issue (path and permalink).

## The prompt

- **Where:** `<plugin>/prompts/` for one plugin (`eq`, `analog-compressor`, `maximiser`, `compressor`), `shared/prompts/` for work across the suite. Name it `NN-slug.md` in a plugin folder or `ID-slug.md` in `shared/prompts/`. If the code lives in another repository, say which.
- **Metadata** (optional; the restricted format is in the [Matrix README](../tools/matrix/README.md#metadata-and-normalisation)):

  ```yaml
  ---
  id: EQ-41
  priority: medium
  effort: low
  dependencies: ["EQ-32"]
  conflicts: []
  ---
  ```

  Leave out `model:`: Sonnet is the default and Matrix always runs Sonnet. Never add a status or execution line; the board owns both.
- **Body**, in this order: the issue link; objective and user value; current and desired behaviour; scope; implementation guidance; acceptance criteria; tests; dependencies and risks; build and release expectations (which plugins, whether users will notice, so a changelog note is needed).
- Do not repeat the execution policy in a prompt. End with a link to it if you like, nothing more.

## Writing it down is not running it

Creating an issue or a prompt does not mean Matrix queued, started, merged or delivered it. Say what actually happened: requested, running, merged, released, delivered. Never invent evidence.
