---
id: SUI-10
status: done
execution: build-now
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
