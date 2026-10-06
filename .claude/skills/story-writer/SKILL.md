---
name: story-writer
description: Write and maintain story projects (game scenarios, gacha events, character stories, bond scenes, spine chapters, fiction) by following a story bible and the project's saved canon. Use this skill whenever the user asks to write, outline, draft, revise, or check any story content, scene, script, patch, event, chapter, character sheet, or continuity ledger, or mentions a story bible, a story project in stories/, or one of their characters or worlds, even if they don't say "skill" or "bible".
---

# Story Writer

Write story content governed by a **bible** (a normative style and structure spec) and the **project state** (the canon of one particular world). The bible says *how* to write. The project says *what is true*. Neither is optional: a draft that follows the bible but contradicts canon is wrong, and so is one that fits canon but breaks the bible.

## Layout

```
.claude/skills/story-writer/
├── bibles/README.md      # index of available bibles: what each is for, what state it needs
├── bibles/<bible>.md     # the bibles themselves, kept verbatim as the user wrote them
├── templates/<bible>/    # starter files for a new project under that bible
└── scripts/check_script.py   # mechanical checks on a finished script

stories/<project>/        # one folder per world, at the repo root
├── project.md            # names the bible and world bible, precedence, premise, tone, project review checks
├── world-bible.md        # optional: the user's own canon document for this world, kept verbatim
├── open-questions.md     # things neither bible settles; the place for unknowns instead of inventing them
└── ...                   # other files depend on the bible (sheets, ledger, index, written pieces)
```

There are two kinds of bible. A **structure bible** (in `bibles/`) is reusable across worlds and governs structure: content layers, formats, lengths, cadence and review. A **world bible** (in the project folder) is the user's canon for one world: its facts, characters, themes, tone, reveal schedule and story direction. It should read as principles and background, not format rules. The project's `project.md` holds the settings that connect the two and states which wins where. When it doesn't say, the world bible wins on facts, characters and tone, the structure bible wins on structure, and a real conflict goes to the user. Older bibles are kept in `archive/` folders for reference; don't write against them.

## Workflow

### 1. Find the project and its bible

- Look in `stories/` for the project the user means. If there is exactly one, use it. If there are several and it is unclear, ask.
- Read `stories/<project>/project.md`. Its `Bible:` line names the bible file.
- If no project exists yet, read `bibles/README.md`, pick the bible that fits (ask if more than one could), and set up a project from `templates/<bible>/` (see "New project" below).
- Read the whole bible file before writing anything. Bibles are short on purpose, and their rules interlock; skimming one section misses constraints set in another.

### 2. Load the canon that this request touches

Read `project.md` and the project's index of released content, then only the state files the request involves: the characters who appear, the ledger, and any earlier content the request refers back to. Don't read every file for a single bond scene, but always read the ledger when writing anything with canon weight, since locked facts there can't be contradicted.

If the project has a world bible, read the sections for every character and place involved, not just the derived sheets. The sheets are summaries and can lag behind. Check reveal timing too. Many worlds schedule what players learn and when, so a fact the writer knows may be one a character can't say yet. A line that leaks a later reveal is a continuity error even if the fact is true.

### 3. Check for missing inputs

Bibles usually require some sheets to be filled before writing (for example, the gacha bible wants a hero's character sheet filled before their story is written, gacha 16.4). When something required is missing:

- If the user gave enough to fill it, fill it, write it to the project, and tell them what you assumed.
- If filling it means inventing something that matters (a wound, a faction, a locked fact), draft a proposal, mark it `Status: proposed`, add it to `open-questions.md`, and ask before building a story on it. The user owns their canon.
- Small local color (a minor NPC's name, what's for supper) you may invent freely and mention, unless the project's invention policy says otherwise. Some world bibles ask for unknowns to stay mysterious on screen. Follow that: write around the gap, and log it as an open question.

### 4. Write in the bible's order

Produce output in whatever order the bible prescribes (the gacha bible: file header, planning sheet, scene cards, script, review answers; gacha 14). The planning sheets aren't overhead: they're where the story gets decided, and they make the script checkable.

**Script format** (use this unless the bible says otherwise, so `check_script.py` can read it):

```
### Scene 3 — Platform 9, before the rain
[Stage direction: short, playable, no narration of action.]
MIRA: One idea per line.
PROTAGONIST: Short questions.
```

Speaker names are uppercase followed by a colon. Stage directions go in square brackets on their own line. Write player choices as bracketed lines (`[Choice a: "..."]`) so they aren't counted as spoken words. Follow the bible's own format if it adds more; the gacha bible ends each story scene with a `[Skip summary: ...]` line, and the checker warns when one is missing.

### 5. Review before you hand it over

1. Run `python3 .claude/skills/story-writer/scripts/check_script.py <file>` (pass `--budget MIN-MAX` for this layer's word range in the project's chosen budget column). It reports spoken word count, scene count, overlong lines, and the average line length. Fix what it flags. Clipped, understated voices tend to land well under budget on a first draft. When that happens, look for thin scenes: a beat stated instead of played, or a relationship told rather than shown in an exchange. Deepen those. Don't add filler lines. If the source material itself calls for something shorter, record a budget override in `project.md` with the reason, rather than padding.
2. Run the bible's own review checklist honestly (the gacha bible's is section 15), plus any "project review additions" in `project.md`. Record the answers in the output file. Where the bible says a failed item means a rewrite (the gacha bible stars its hard-rule items), rewrite the scene; don't just annotate it. For softer items, fix the problem or record the deviation and why in the piece's header.
3. Reread one scene aloud in your head as each speaking character. If two characters could trade lines, the voices need work.

### 6. Save and update canon

- Save the piece in the project folder where the bible's template puts it, using a numbered, slugged name such as `patches/003-humming-box.md`.
- Add new facts to the ledger. Mark them locked or soft as the bible defines those terms.
- Update character sheets with anything that has now become true (new habit, new idle line, the bond secret revealed).
- Add the piece to the project's index.
- Tell the user what was created and which canon changed. Don't paste the whole script into chat unless they ask; they can open the file.

## When a request breaks the bible

Bibles mix hard rules with defaults. Bend a default when there's a good reason, and record it as a deviation. Hard rules are different: the user set them so that requests get held to them. If a request conflicts with a hard rule, name the rule (section number), then offer the closest compliant version. If the user insists, they're overriding their own rule, so do it, and note the override in the output file's header so it's visible later.

## New project

1. Ask for (or take from the conversation) the project name, premise, and whatever the bible's setup section requires.
2. Copy `templates/<bible>/` to `stories/<project-slug>/` and fill in what you know. Leave unknown fields as clearly marked `TODO`s rather than inventing canon.
3. If the user supplies a world bible, save it verbatim as `world-bible.md`, then build the derived files from it:
   - In `project.md`: precedence between the two bibles, the world's invention policy, its tone rules, and a "project review additions" list. That list holds every rule in the world bible that a draft could break (reveal timing, silent enemies, act-specific voice and so on).
   - Character sheets: fill each field from the world bible. Tag anything you inferred as derived and anything with no source as *proposed*. Point back to the world bible section.
   - Ledger: seed it with the world's facts, including when each may be revealed.
   - Open questions: copy the world bible's own open questions, and add the gaps the style bible needs filled that the world bible doesn't answer.
   - Where the two bibles disagree, note it in `project.md` and ask the user rather than silently picking one.
4. Show the user the filled-in `project.md` and the open questions before writing story content.

## Adding a bible

When the user provides a new bible:

1. Save it verbatim as `bibles/<short-name>.md`. Don't edit their rules; it's their spec.
2. Add an entry to `bibles/README.md`: what kind of story it's for, which project files it needs, its output order, and its review checklist location.
3. Create `templates/<short-name>/` with starter files for the state that bible needs. Read the bible to decide what that state is (sheets it requires, ledgers it maintains).
4. If the bible has no review checklist, `check_script.py` and the read-aloud test still apply.
