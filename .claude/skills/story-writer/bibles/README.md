# Bibles

Style bibles are game-agnostic and govern **structure**. Each project's world bible (in `stories/<project>/world-bible.md`) governs facts, characters, themes and tone.

## gacha — `gacha.md` (v2, research-built)

- **For:** live-service and gacha game story: main story, interludes, events, group stories, character and bond stories, chats, mail, hub lines, barks.
- **Basis:** built by a research workflow on 2026-10-06 from patterns in long-running gacha games. Sources are in `gacha-sources.md`. Structures name the games they follow. Blocks labeled "Craft suggestion" are optional house defaults.
- **Project files** (template: `templates/gacha/`):
  - `project.md`: the project settings in gacha §16.1 (premise and spine, protagonist, hierarchy, reveal schedule, invention policy, tone, budget column, design assumptions §16.3, project review additions, character sheet fields, recorded deviations)
  - `characters/<name>.md`: one sheet per hero (§16.4)
  - `ledger.md`: continuity ledger (§17): Fact, Layer, Locked, Owner, Reveal, Source
  - `index.md`: released content, in release order
  - `open-questions.md`: unknowns to resolve with the user instead of inventing them
  - `world-bible.md` (optional): the user's canon for this world, verbatim
  - `patches/NNN-slug.md`: one file per piece
- **Output order:** file header (§14.1), planning sheet (§14.2), scene cards (§14.3), script (§14.4; other formats §14.5), review answers (§15).
- **Budgets:** §13. Pass the project's column range to `check_script.py --budget`.
- **Review:** §15 checklist, plus the project review additions. A starred item failing means a rewrite; anything else is fixed, or the deviation is recorded.
- **Hard rules:** §1 (five rules). Breaking one needs the user's written override in `project.md`.

## Archive

- `archive/gacha-v1.md`: the first gacha bible, written by another AI. Replaced by v2. Kept for reference only; don't write to it.
