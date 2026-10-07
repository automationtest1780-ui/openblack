# Incrementum

Bible: gacha (`.claude/skills/story-writer/bibles/gacha.md`). It owns structure: layers, formats, lengths, cadence, review.
World bible: `world-bible.md` (v0.2). It owns facts, characters, themes, tone and story direction. Kept verbatim; don't edit it. v0.1, the original, is in `archive/`.
Structure notes: `structure-notes.md`. Incrementum's own structure settings that came out of world bible v0.1.

## Precedence
1. **World bible, Canon material**: fixed. Never contradict it, and never rephrase it in a way that changes the meaning.
2. **World bible, working material**: use it by default. Only the user revises it.
3. **Gacha bible**: governs structure. Where the world bible is more specific, the world bible wins. A conflict with a gacha hard rule goes to the user (gacha 16.2).
4. **This file and `structure-notes.md`**: project settings. They record which defaults Incrementum bends.
5. **Derived files** (character sheets, ledger): convenience views of 1–2. When they disagree with the world bible, the world bible wins, and the derived file gets fixed.

## Project settings (gacha 16.1)

### Premise / spine sentence
*Plain-spoken working people of a cold harbor town face a sky that has started to move. When a growing star pulls on the ground of Saltreach, four people who refuse to look away gather in the dead lighthouse to count, shield and fight the Plumbs, but each shift grows Wren further from the girl they found, until the sky gets a new fixed point.*
(Claude, 2026-10-06, on the user's delegation.)

### Protagonist role
- **The new keeper of the breakwater light.** The council hired them this season, after the last keeper's family left in the bad year. They're new to Saltreach and know nobody. Characters call them "Keeper". Unnamed, no set gender, no backstory beyond this.
- Speaks little: questions, choices, short commitments. Not voiced. A catalyst: never worshipped, never the star (gacha 5).
- Why they're there: on their first night (night three), Pell sent them up the old lighthouse with his bearing, so they were in the lamp room with Sera when the light fell. Their own lamp burned unwatched, and Nell steered home by it.
- Thematic job: the **second witness**. "Hold the chalk" makes them the person who shares Sera's count. Cosmology rule 2 says a shared count holds more, and Sera's fear is being the only one who believes.
- (Claude, 2026-10-06, on the user's delegation. See open question 9.)

### Hierarchy (gacha 3.1)
- **Part 1** = Saltreach: the Opening plus Acts I–V. Each **Act** is an arc of 3–6 chapters.
- The post-ending hook (Ferrow, Bellwater, Mount Quell) is Part 2 material. Each new region arrives as a new quartet arc (gacha 8.5).

### Reveal schedule
World bible §3, "What Players Learn and When", plus the Reveal column in `ledger.md`. Every piece's header sets a Reveal gate (an act). Wren's stage follows her arc beats in world bible §4.

### Invention policy
When neither bible covers something that matters, keep it mysterious on screen and add it to `open-questions.md`; don't make up lore (world bible, How to Use). Local color is fine (a net-loft gossip's name, tonight's soup) as long as it creates no fact anyone has to remember.

### Tone
World bible §8, "Tone Rules". These override gacha 12.4:
- Plain speech: workers, not prophets.
- Understatement is the house style. Fear comes out sideways: jokes (Bram), numbers (Sera, Ivo), silence.
- The Bloom has no words, and the Plumbs never speak.
- Every scene is about a person first and the sky second.

### Budget column (gacha 12.1, 13)
**Story-first VN.** Incrementum is a stage-node squad gacha like Arknights, NIKKE and Epic Seven, and those deliver story VN-style. Matching that is how it fits the genre. The terse voices still apply: reach the budget with more scenes and fuller exchanges, not longer lines.
- Overrides: pair-bond conversations stay under 300 words each (structure notes, item 2).
- Overrides: **quartet hub group scenes** (the five Lamp Room quartet scenes, WB §6) run 1,500-2,800 spoken words in 2-4 scenes, not the gacha 13 group-story range (3,400-6,750). Why: they are short home-base character pieces that carry no plot (WB §8; structure notes item 1), so the full range would mean padding. First used by 105 "Supper in the Lamp Room" (2026-10-06).
- Overrides: the act finale main chapter may run to 1.5x a main chapter (6,750-16,000), per gacha 3.6 finale weight. First used by 104 "A Finger More".
- Patch 000 (the Opening) was first written under the old bible at 1,035 words. It has since been revised into the VN Opening range (~2,250–4,500); it stands at 2,671.

### Design assumptions (gacha 16.3)
```
Version length and phases: ~6 weeks, two phases of ~3 weeks
First summon point: after the crater fight (Opening, scene 7/8); target under ~30 min
System unlock order: gacha, then upgrades, then the lamp room (home base), then endgame, over the first 1-2 weeks
First-clear rewards: premium currency on main-story first clears
Story difficulty: offered (yes)
Unowned-hero device: letters or rumor (world bible §8); the Saltreach four are the starter roster, so the campaign never needs an unowned hero
Bond gating: hero-protagonist bond chapters 1-3 reachable without duplicates; pair bonds (hero-hero) unlock from shared wins, thresholds set by design
Baseline package per pullable hero: gacha 7.1 default
Character story access: at banner start without owning the hero; unlock currency no
Archive rule: event stories archived when the event ends
Skip, Auto, Log and skip-summary UI: present
```

### Project review additions (run alongside gacha 15)
- Does any line name the Bloom, or state a cosmology answer, before the act that reveals it (world bible §3 table)?
- Does Wren's voice match her stage for this act (Fledgling, Kindling, Brightening, Called, Fixed)? Is the stage in the header?
- Do the Plumbs stay silent and the Bloom wordless?
- Is each pair bark the right version for the act (for example, Sera and Ivo before and after Act III)?
- Are absent heroes carried by letter or rumor only?
- Does each content drop give one scene to an existing bench hero? Are next-drop heroes mentioned by existing heroes first (structure notes, items 6–7)?
- Events running while an act is live are gated at the *previous* act's clear. While Act III is live, events never mention the ledger.
- Lamp Night and any event: the old lamp stays dark until Act V.
- Ferrow, Bellwater and Mount Quell heroes know only what their own sky showed them (roadmap §4.3).
- Is anything invented that should have gone to `open-questions.md`?
- **Act III and later (from the Act II continuity pass, 2026-10-06):**
  - The wall is full (night of day 22). Nothing new goes on the lamp-room wall; counts go on hands until a piece decides where (open question 38e). Sera keeps the chalk herself since 2-4 ("No. I'll hold it."); the keeper still counts with her but no longer holds it unless she gives it back on screen.
  - Sera and Bram are not speaking as Act III opens, beyond "I know" and "It's cold" (205). Their deal ("Eyes up there, feet down here") is broken off; don't restore it without an on-screen cause. Bram still brings supper every night, and Sera's tally still gets its stroke.
  - Sera's last tide figure is 2 h 10, plus the 2 h 40 on the leaf (now in the front of her tide book). She has not yet read the keeper's later gauge readings in the breakwater log; when she does, it's a scene.
  - Ivo knows about the cut leaf (205) and told Bram he would have cut it too. He has not been seen to connect it to himself aloud; keep it that way until his confession beat (OQ 2).
  - The town is under the oil ration and the glass stop; "Don't look up after supper" is council advice. The lamp room is the only lit window after supper.
  - Wren is Kindling at the start of Act III: the coat sleeves stop short of her wrists. Bram calls her "Sparrow". She has stopped glowing on purpose except in a fight, and she sits "in the middle" of the stair when the others quarrel.
- **Act IV and later (from the Act III continuity pass, 2026-10-07; full brief in `act4-starting-state.md`):**
  - The count is on **paper**: Sera's tide book in two columns ("S.V." / "I.T.") and the fair copy's margin. The lamp-room wall stays full; the glass holds only "bloom" (by the second window bar) and the 41 strokes under it from the still night. Don't wipe them without an on-screen cause.
  - **The keeper holds the chalk again** (305, "Hold the chalk."; "Keep it. Dusk. And after.").
  - Sera speaks to Ivo directly and by name; the post-Act III Sera/Ivo bark ("Call it." / "...Thank you. Left.") is live and the pre-Act III bark is retired. She has **not forgiven** him; don't write forgiveness in Act IV without a cause. Ivo has said "I am sorry" once (303); a second one needs a reason.
  - Sera and Bram: the deal's **first half is restored** (305: she asks, he finishes). The leaf is **not settled** ("That's for after supper."). Bram still won't pick a side ("supper's side"). WB §7 keeps "what he never said" for Act V.
  - **The ledger** is council property; it lives in the customs house and Ivo signs it out "to myself, as chair" at dusk and returns it each morning. Sera has asked for it at dusk on day 29. It is **not handed to Sera** until Act V ("Fixed. Witnessed by all of Saltreach."). Its last page stays blank.
  - Use "forty-one, less a hair" and "fourteen nights" for the first move; "error, mine"; the *Lark* night (Flint's light lit, seen by Ivo; the tide fifty minutes early). Don't re-litigate them.
  - Ivo still uses **no contractions**. "I do not know" is now allowed when it's true (301, 305), never as a dodge.
  - Measurement *seems* to slow it, and the town saw one held night. **Nobody explains why** (OQ 4). In Act IV the town counting together holds Wren down (4-3): show it, don't explain it.
  - Wren is Brightening, edging to Called: older teen, a steady light of her own, sees in the dark, sleeves near the elbow, slower voice. Her "up there" lines are sensory only (OQ 44); from Act IV she may say plainly that she came from the bloom and it wants her back (4-1), but not before 4-1.
  - Haze-Walkers are the Act III variant (OQ 39-41); they stand at lamp edges and walk straight lines. Plumbs stop lashing out near Wren only from Act IV (roadmap 4-1).
  - Council: three seats and the chair (Gage stays); the Harbormaster, the Chandler and the Shipwright walked out. The glass stop stands. The oil ration stands except for the one lifted night. "LOOK UP" is on the customs house door in red.
  - Nell's date is **the thirtieth, first light**; she has said "Where's south, now?" but not changed it. Roadmap 4-3 has her stay ("not tonight").

### Character sheet fields (gacha 16.4, plus)
Use the gacha 16.4 sheet. Incrementum adds:
- Wren: a stage table (act → stage → size and voice).
- For every hero: the signature item, victory line and death line from world bible §4.

### Recorded deviations from gacha defaults
- **Pair bonds are hero to hero** as well as hero to protagonist (structure notes, item 2; gacha 7.3).
- **Alternate versions:** the same person in a new role only, never an alt with a different personality (world bible §8; gacha 10.3).
- **Quartet scenes** close each act as a group story (structure notes, item 1; gacha 8.2). They're the starter roster, so everyone has them.

### Calendar (gacha 9.5, world-bible slot)
Early-autumn launch (OQ 13). Saltreach festivals (OQ 14; details in roadmap §4.2): **the Smokehouse Feast** (launch), **Lamp Night** (winter; the old lamp stays dark until Act V), **the Blessing of the Boats** (spring), **the Silver Fair** (summer herring run). The real-world anniversaries line up with the act finales (roadmap §1.2).

### Roster and plan
- Launch roster: `roster.md`. Sheets are in `characters/`, quartets in `groups/`.
- Part 1 plan: `roadmap.md` (Opening + 21 chapters + 2 interludes; launch = Opening + Act I).

## Factions and places
| Name | One line | Source |
|---|---|---|
| Saltreach | Cold harbor town, proud of being sensible, distrusts excitement | WB §2 |
| The lamp room | Top of the dead lighthouse; chalk wall; home base | WB §2 |
| Harbor council | Seven seats, chaired by Ivo, meets in the customs house | WB §2 |
| Breakwater light | New, efficient, unloved; the keeper's post | WB §2 |
| Salt marsh / crater | A bowl of fused glass with the reeds leaning in | WB §2 |

## Rule overrides
None.
