# Gacha Story Bible (Structure)

What this is: the structure spec for writing story in a live-service gacha game. It says what content exists, what shape each piece takes, how long it runs, when it ships, how a script is laid out, and how a draft is reviewed. It is game-agnostic.

What it is not: a source of facts, characters, themes or tone. Those belong to the project's **world bible**. Section 16 says where a world bible and the project settings plug in.

Stance: we are not trying to reinvent the genre. Structures are modeled on proven games that have run profitably for years (Fate/Grand Order, Arknights, Blue Archive, Genshin Impact, Honkai: Star Rail, NIKKE, Zenless Zone Zero, Limbus Company, Fire Emblem Heroes, Wuthering Waves); each section names the games it follows. Craft suggestions are house defaults, labeled as such, and are not market-tested. When the world bible has a good reason to bend a default, bend it and record why. When this bible says **Hard rule**, don't bend it without the user's written override.

## Contents

0. [How to use this bible](#0-how-to-use-this-bible)
1. [Hard rules](#1-hard-rules)
2. [Content layers](#2-content-layers)
3. [Main story structure](#3-main-story-structure)
4. [Opening and first summon](#4-opening-and-first-summon)
5. [The protagonist](#5-the-protagonist)
6. [Character debut and banner package](#6-character-debut-and-banner-package)
7. [Character stories, bonds and the profile kit](#7-character-stories-bonds-and-the-profile-kit)
8. [Factions and group stories](#8-factions-and-group-stories)
9. [Events and the live-ops calendar](#9-events-and-the-live-ops-calendar)
10. [Reruns, returns and alternate versions](#10-reruns-returns-and-alternate-versions)
11. [Hub, home base and barks](#11-hub-home-base-and-barks)
12. [Scene craft and pacing](#12-scene-craft-and-pacing)
13. [Length budgets](#13-length-budgets)
14. [Planning sheets and script format](#14-planning-sheets-and-script-format)
15. [Review checklist](#15-review-checklist)
16. [Plugging in a world bible and project settings](#16-plugging-in-a-world-bible-and-project-settings)
17. [Continuity ledger](#17-continuity-ledger)

---

## 0. How to use this bible

- **Defaults, with reasons.** Most rules are written as *Default* plus *Why*. If you understand the why and the world bible or the user gives a better reason, deviate and note it in the piece's header (`Deviations:`).
- **Two kinds of default.** A *structure* default names the games it is modeled on. A block labeled **Craft suggestion (house default, not market-tested)** is optional: use it when it helps, drop it without recording a deviation.
- **Hard rules** (section 1) protect players from things that reliably drive them away. Breaking one needs the user's explicit override, recorded in `project.md`.
- **Design assumptions** (16.3) are product-side guarantees (bond gating, archive rule, baseline package, story difficulty, unlock order). A project fills them once in its settings; writers check against them, they don't re-decide them per piece.
- **Output order for any piece:** (1) header, (2) the planning sheet for its layer, (3) scene cards, (4) script, (5) review answers. Sheets come first because that is where the story gets decided, and they make the script checkable. See section 14.
- **Who wins:** the world bible wins on facts, characters, tone and reveal timing. This bible wins on structure, format, length and review. A real conflict goes to the user.

---

## 1. Hard rules

Five rules. Each one has a public failure behind it.

1. **The main story never requires owning a hero.** Unowned heroes speak and act in story; where a fight needs them, lend them as a trial or guest unit, or (if the world bible prefers) tell their part through letters or rumor. Ownership unlocks bond content, voice lines and hub presence, not plot.
   *Why:* story is the free sales pitch for a hero. Gating it behind a pull excludes free players and makes the story look like an ad. (FGO guest Servants, Genshin and HSR trials, FEH Forging Bonds "without needing the unit".)
2. **A main chapter may reference events; it must remain understandable without them, and any event it references must be archived.**
   *Why:* Genshin's unarchived Unreconciled Stars introduced Scaramouche, who later became central; late joiners were lost, and it produced petitions and press. HSR answered with Conventional Memoir; Arknights archives Intermezzi and Side Stories on a fixed rule.
3. **Every story scene ships with a skip summary.** One or two sentences, written with the scene, saying what happened and who was there. This covers story scenes (main, interlude, event, group, character and bond scenes); chat threads, barks, voice lines, mail and profile entries are exempt.
   *Why:* players split into readers and skippers. Skip with summary keeps skippers following, so later banners still land. HSR added it in 3.4/3.6; WuWa's unskippable launch dialogue drew press backlash.
4. **The first bond chapters are reachable by every owner.** Higher chapters may sit behind investment or duplicates; the opening ones may not. The exact thresholds are a design assumption (16.3).
   *Why:* NIKKE puts episodes 1-5 within bond levels 1-10, which everyone can reach, and only raises the cap with limit breaks. Gating the first chapters makes free players feel locked out.
5. **Don't spoil a pullable hero's fate in marketing, and don't invalidate their banner fantasy without an on-screen, caused change.** Change is allowed; it must be shown and caused, not announced or asserted. Game design owns kits; story checks against the banner fantasy and teaser recorded on the hero's sheet (16.4).
   *Why:* players paid for that character's fantasy. Genshin event announcements that spoiled a character's fate drew complaints.

---

## 2. Content layers

Every piece of story belongs to exactly one layer. Pick the layer before writing.

| Layer | Job | Typical cadence | Canon weight | Unlock | Modeled on |
|---|---|---|---|---|---|
| **Main story** (Part > Arc > Chapter > Stage) | Move the long plot; introduce regions and headliners | 1 chapter per 1-2 versions | Locked canon | Story progress | Genshin Archon Quests, HSR Trailblaze, Arknights Main Theme, FGO Singularities/Lostbelts |
| **Interlude / continuance** | Fill gaps between main chapters; revisit old arcs | Between main chapters | Canon | Main story progress | HSR Trailblaze Continuance, Limbus Intervallo, FGO 1.5 |
| **Flagship event** | A complete short story starring the version's new hero(es) | 1 per version (~6 weeks); festival weight in an arc-finale version (3.4) | Canon, self-contained | Time-limited, then archived | Genshin flagship events, Arknights Side Stories, Blue Archive events, FGO events |
| **Minor / evergreen event** | Login reason; light framing on a mode | 2-4 per version | Color | Time-limited | Genshin minigame events, Arknights Contingency Contract, Blue Archive raids |
| **Seasonal / gag / collab event** | Festival fun, alternate versions, crossovers | Yearly slots (section 9.5) | Semi-canon or non-canon, labeled | Time-limited, archived or rerun | FGO summer and Halloween, Blue Archive swimsuit, NIKKE x Chainsaw Man |
| **Group story** | Make a faction or quartet feel like a unit | 1 per faction intro, more via events | Canon | Event or faction progress | Blue Archive Group Story, FEH Forging Bonds, ZZZ faction intros |
| **Character story** (premium) | Why this hero is worth wanting; a story they own | At banner launch | Personal canon | Available at banner start without owning the hero (an unlock currency is acceptable, e.g. Genshin Story Keys); permanent | Genshin Story Quests, HSR Companion Missions, WuWa Companion Stories, ZZZ Agent Stories |
| **Bond chapters** | Intimacy between hero and protagonist | 3-5 per hero at launch | Personal canon | Ownership + bond level | NIKKE Bond Episodes, Blue Archive Relationship Stories, FGO Interludes, Arknights Operator Records |
| **Trial stage framing** | Let anyone try a banner hero in a short fight | Per banner | Color to personal canon | Free, during the banner | Genshin and HSR character trials |
| **Profile kit** | Short text entries and voice set | At launch | Personal canon | Ownership + bond level | Genshin Character Stories, Arknights Archive files, FGO profiles |
| **Chat / daily interaction** | Daily drip of personality | Ongoing | Color to personal canon | Ownership, daily | Blue Archive MomoTalk and Café, NIKKE Advise and Messenger |
| **Event / holiday mail** | A hero's note for a birthday, festival or event, often with a small gift | Birthdays and calendar slots | Color | Ownership (birthdays) or all players (festivals) | Genshin birthday mail, HSR phone messages |
| **Barks and hub lines** | Ambient presence | Ongoing | Color | Ownership, context | Genshin voice-overs, Arknights assistant lines, Blue Archive Memorial Lobby |

**Default for side layers:** canon, but never required to follow the main story. *Why:* Arknights and Limbus keep main and side content distinct; Genshin spreads required context across World Quests, item text and events, and players ended up needing summary videos.

---

## 3. Main story structure

*Modeled on: Genshin (nations as chapters, Acts per release), HSR (worlds as arcs), FGO (Singularities then Lostbelts, Part 1 then Part 2), Arknights (Acts > Episodes > stages), Blue Archive (Volumes per school), Limbus (each Canto centered on one Sinner), NIKKE (paired chapter drops).*

### 3.1 Hierarchy

| Level | Unit | Ships as | Default size |
|---|---|---|---|
| Part / Season | The game's first 2-4 years | Ends in a finale, relaunches as Part 2 | 3-6 arcs |
| Arc | One region, world or faction, with its own cast and banner set | Spans several versions | 3-6 chapters |
| Chapter (Act) | One release drop | Ships with that version's banners | 8-14 stages |
| Stage (Quest) | One play session; one battle with scenes around it | — | 1-3 scenes |

*Why:* each arc can be marketed as a new "season" with a new setting and cast, while the larger plot continues. A new region is also the natural point for lapsed players to return.

### 3.2 Arc shape

Default escalation across an arc:

1. **Arrival (early chapters):** local, character-focused. Introduce the arc's cast through a small problem. Teach the place.
2. **Complication (middle):** a betrayal, a reveal, or a cast member's want colliding with the mission.
3. **Finale:** world-level stakes for this arc. Best production of the arc (CG, animated cutscene, unique boss, ending song). Ships with the arc's flagship hero.
4. **Close locally, keep one global question open.** The arc's conflict resolves. The game-wide mystery moves one step.

*Why:* payoff every arc keeps players satisfied; one unresolved spine keeps them speculating (Genshin's sibling and Celestia, FGO's Incineration then the Alien God, HSR's Stellaron). Arknights' episodes grew from 10 to 24 main stages across Act I, which signaled scale.

**Default: one meta-mystery, two at most.** *Why:* stacking mysteries that never pay off reads as a stalled story (long-running complaints about Genshin's sibling plot and Arknights' delayed Doctor reveals; praise for Blue Archive's Volume Final and FGO's Part 1 finale).

### 3.3 Chapter shape (stage-node default)

**Researched structure.** For squad and stage-map games, a chapter is a sequence of stages. Each stage carries a short scene before and/or after its battle; the boss stage gets a longer cutscene. Each stage fits one mobile session and doubles as a team-building check. The chapter opens by re-establishing place and job in-world (12.3) and closes on a hook into the next drop. *Modeled on:* Arknights, NIKKE and Blue Archive chapters.

> **Craft suggestion (house default, not market-tested).** A beat sheet that fits the structure above. Adapt freely.
>
> 1. **Where and why, in-world.** First lines state place and today's job. Don't assume last month was read.
> 2. **Mission in one sentence.** The UI objective matches the spoken one.
> 3. **The chapter's featured hero wants something** that is not the mission.
> 4. **Travel or investigation.** Two stages at most before conflict.
> 5. **Midpoint turn:** the obvious fix is wrong, or the featured hero's want collides with the mission.
> 6. **Light scene.** A meal, a joke, a walk.
> 7. **Set-piece / boss stage.** Dialogue during action stays short.
> 8. **Cost.** Something small and permanent changes: trust, a place, a rule.
> 9. **Button.** A one-line hook for the next chapter that does not withhold the point of this one.

### 3.4 Cadence

- **Default:** one main chapter every 1-2 versions (every 6-12 weeks). Interludes, events and character stories fill the gaps.
- **Per-phase split:** within a version, a main chapter or interlude opens phase 1 and the flagship story event lands in phase 2 (or vice versa), so the two big text drops never share a window.
- **Finale exception:** when the main chapter is an arc finale, that version's event drops to festival or light weight (Genshin 4.8 delayed an event story to make room for an Archon Quest).
- Announce an arc's span when it starts (HoYoverse publicly said Amphoreus would run 3.0-3.7).
- Slower cadences are fine for story-first games (Limbus about one Canto per 4 months; FEH one 13-chapter Book per year) as long as something fills the gap.

*Why:* main story is the most expensive content. Interleaving with cheaper content keeps a steady drumbeat without burning out the team. Droughts with nothing in between produce "dead game" talk (early Arknights CN had 4-10 month gaps).

### 3.5 Banner tie-in

- Each chapter introduces or spotlights the heroes on sale in that version or the next.
- The arc finale ships with the arc's flagship limited hero.
- The free story cast stays central. Banner heroes join the plot; they don't push the free leads out of it.

*Why:* FGO opened Lostbelt 1 with a pickup for its Servants; HoYo and Kuro banners feature the current region's cast. Long-lived games keep their free partners at the center (Mash, Amiya, March 7th and Dan Heng).

### 3.6 Parts and relaunches

After 2-4 years, end Part 1 with a large finale (it doubles as an anniversary moment), optionally run an interlude ("1.5"), then open Part 2 with a new frame. *Why:* closure and buzz, and a clean jumping-on point that allows a tone or setting shift without retcons (FGO Part 1 > 1.5 > Part 2; Blue Archive Volume Final; Arknights Acts; Limbus Seasons).

### 3.7 System unlocks and rewards

The unlock order and whether a story difficulty exists are design assumptions (16.3); the project sets them once. The story's job:

- Announce each unlock the design assumptions place in a chapter as a story moment or reward (a ritual, a key handed over, a room opened).
- Don't write a scene that assumes a system the player hasn't unlocked yet.

*Why:* spreading complexity over the first week or two lowers early cognitive load and gives day-2, day-3 and day-7 hooks (HSR, Genshin, Arknights, NIKKE, FGO). First clears paying premium currency lets the story fund early pulls. Power walls are a top complaint; Arknights added Story Environment as a fix.

### 3.8 Catch-up

Main story is permanent. Plan, from day one, for: replay with skip summaries, a story recap per chapter (written by you; see 14.2), reduced stamina on older chapters before a new one, and returner events. *Why:* returning players are cheap to win back only if they can reach the current arc and its banners in a session or two (HSR Story Recap, Arknights story review, FGO AP campaigns, HSR returner events).

---

## 4. Opening and first summon

*Modeled on: Genshin (twins vs the Unknown God), HSR (Herta Space Station attack, ~30 minutes to first warp), Arknights (Chernobog extraction), FGO (Fuyuki).*

Default shape:

1. **Cold open with a glimpse of power or loss** (1-3 minutes). Sets up the long mystery and shows the power ceiling the player is working toward.
2. **Control within minutes.** A fight, even a scripted one.
3. **Cut to a modest start.** A person, a place, a job. No cosmology.
4. **Free story leads join.** Two or three heroes who are given, not pulled, and who carry continuity forever.
5. **First summon, framed in the fiction** (a ritual, a recruitment, a ship taking on crew), paired with a beginner banner that guarantees a top-rarity hero.
6. **Systems drip in** over the following days (3.7).

**Default: first summon as early as the story allows.** Treat ~30 minutes as the upper comfort bound and 60 as the outer limit. Stage-node games commonly get there sooner than open-world ones (researchers' own knowledge, not sourced). *Why:* the first hour is widely treated as make-or-break, and rerollers punish long tutorials (HSR's ~30-minute pre-warp tutorial drew complaints).

Avoid: bleak, jargon-heavy, lore-dumping openings. WuWa rewrote ~90% of its story after CBT, softening the tone and refocusing on the protagonist, and its launch opening was still called "word salad".

---

## 5. The protagonist

*Modeled on: Arknights' Doctor, Blue Archive's Sensei, HSR's Trailblazer, FGO's Master, NIKKE's Commander, ZZZ's Proxy.*

- **Role:** an outsider or amnesiac with a role title, at the center of a home faction (a company, school, train, task force). The outsider frame lets others explain the world naturally (the "ignorant interlocutor": a short question, a one- or two-line answer, a demonstration next scene). The world bible sets the actual role (16.1).
- **Job in scenes:** catalyst and relationship hub. The heroes carry the drama; the protagonist enables, asks, decides.
- **Voice:** a person, not a blank. Give attitude choices (deadpan, kind, absurd) that don't branch the plot.
- **Default dialogue weight:** short lines, questions, choices, small commitments. A longer speech only at an arc climax, and only if the world bible gives the protagonist a voice for it.

*Why:* the Trailblazer's chaotic choices became a meme and a breakout; Sensei's defined responsibility is the core of Blue Archive. The two failures are opposite: the passive cipher (Genshin's Traveler criticism) and the self-insert surrounded by admirers (an anecdotal criticism, from a single player review, of NPCs worshipping the HSR protagonist). Kuro's WuWa rewrite shows the protagonist still needs presence and personal stakes.

---

## 6. Character debut and banner package

*Modeled on: Genshin (teaser > Character Demo > banner + Story Quest), HSR (paired reveals ~3 weeks out; Firefly), WuWa (Companion Story opens with the banner), FGO and Arknights (chapter or event plus banner together).*

### 6.1 The launch package

Every limited hero launches as one unit, timed together:

| Piece | When | Story's part |
|---|---|---|
| Teaser / reveal | ~3 weeks before (HSR paired reveals) | One-line hook, one image, consistent with the story; no fate spoilers (Hard rule 5) |
| Trailer / demo | Optional suggestion: ~1 week before | Voice and attitude; no spoilers of their story's turn |
| Story appearance | Same day as banner (or earlier, 6.2) | Event, chapter or character story where they lead |
| Trial stage | During the banner | Short framing scene (13) |
| Banner | Opens with the story | — |
| Character story / bond / profile kit | Live at banner start | Section 7 |

*Why:* story is the reason to want the hero, and it arrives exactly when the player decides whether to pull. miHoYo said at GDC 2021 that characters are the foundation of the business and that its narrative runs as parallel plotlines, one per character, in a shared world.

### 6.2 Story-first introductions

**Default: introduce headline heroes in story one or more versions before they are pullable.** Give them a question the story pays off, and open their banner when it does. This is the proven practice: Firefly was in HSR's Penacony story from 2.0 and became pullable in 2.3, the version that closed the arc; Genshin's Archons debut in Archon Quests before their banners.

> **Craft suggestion (house default, not market-tested).** For heroes who can't get a story-first debut, mention them in existing heroes' lines one drop before release, so they arrive already known. Optional.

### 6.3 Debut story jobs

A debut story (event lead, chapter feature or character story) does four jobs:

1. **Show the hook in the first scene:** a visible behavior, before exposition.
2. **Give a want that isn't "help the protagonist".**
3. **Put the want under pressure** before the banner ends.
4. **Leave a reason to read their bond chapters** after the pull.

**Deliver the banner fantasy** (protection, competence, chaos, softness, rivalry...) in scene 1, complicate it by the end, and don't betray it in the debut. *Why:* the debut is the sales pitch; complication makes them interesting, betrayal makes the purchase feel wrong (Hard rule 5).

---

## 7. Character stories, bonds and the profile kit

*Modeled on: Genshin (Story Quests, Hangouts, Friendship-gated Character Stories), NIKKE (Bond Episodes at 1/3/5/7/10, Advise), Arknights (Operator Records at Trust thresholds), FGO (Interludes at Bond 2-5), Blue Archive (Relationship Stories via MomoTalk).*

### 7.1 Baseline package per pullable hero (default)

The package is a design assumption (16.3); this is the default the project starts from.

| Piece | Default | Unlock |
|---|---|---|
| Profile kit | 4-6 entries: background, a personal object, 2-4 story entries | Bond levels |
| Voice set | ~30-50 lines (section 11.1) | Ownership; some lines by bond |
| Bond chapters | At least 3 (default 3-5) | Ownership + bond levels; first chapters reachable by all owners (Hard rule 4) |
| Chat threads | At least 2 (default 2-4) at launch | Bond levels; lead into bond chapters |
| Pair lines | At least 2 "about X" lines on related heroes | Ownership |
| Top rarity: premium character story | 1 (7.2) | Available at banner start without owning the hero |
| Lower rarity: smaller owned format | 1 hangout-style story | Ownership |

**Protected principle: no hero gets nothing.** Every pullable hero gets at least the first five rows; the last two scale with rarity. Anything beyond ships later as additions (new chapters, rerun stories). *Why:* fans read uneven depth as "the developer doesn't care about my character" (Genshin 4-stars without Hangouts, FGO Servants without Interludes for years). A fixed template keeps writing cost predictable and fairness visible.

**Minute targets** (reading time; words derived in 13): bond chapter 2-4 min (light) / 3-6 min (VN); premium character story 15-25 min (light) / 20-35 min (VN); hangout-style story 5-10 min (light) / 8-15 min (VN).

### 7.2 Premium character story

**Researched structure.** Lives on the hero's own screen. Permanent. Opens with the banner and is readable without a pull (an unlock currency is acceptable). Self-contained: it doesn't advance the main plot. It works as a long sales pitch during the banner and keeps selling the hero after it ends. *Modeled on:* Genshin Story Quests, HSR Companion Missions, WuWa Companion Stories (open with the banner), ZZZ Agent Stories.

> **Craft suggestion (house default, not market-tested).** Five beats: (1) a mundane request that hits the hero's wound; (2) they handle it the way their contradiction demands; (3) the protagonist sees the private version, once; (4) they choose a slightly different action, not a new personality; (5) a new habit or line proves the change.

### 7.3 Bond track

**Researched structure.** 3-5 chapters at fixed bond levels, spread out: early chapters close together, the last behind the top cap. Higher caps may tie to duplicates or upgrades; the first chapters don't (Hard rule 4; thresholds in 16.3). Lower-rarity or free heroes get their own smaller format, not nothing. Each chapter is a short goal along a long grind; quiet ties to duplicates reward spending without locking anyone out. *Modeled on:* NIKKE Bond Episodes (1/3/5/7/10, cap raised by limit breaks), Arknights Operator Records at Trust thresholds, FGO Interludes, Genshin Hangouts (5-6 endings).

> **Craft suggestion (house default, not market-tested).** Each chapter: no main plot, one private fact, one gesture or boundary, a callback to an earlier chapter. End on the hero, not on the protagonist's feelings.

### 7.4 Daily interaction loop

A seconds-long daily touch: a two-choice chat, a hub tap, a gift reaction. Better answer gives more bond, never zero. Limit to a few per day.

*Why:* NIKKE Advise (3 per day, 100 or 50 bond exp) and Blue Archive Café plus MomoTalk keep a large roster alive at low writing cost. Hubs that feel like chores (too many taps or menus) get outsourced to guides.

### 7.5 Unowned heroes

Unowned heroes appear fully in story (Hard rule 1). Ownership adds: bond chapters, voice set, hub presence, chat. A hero's character story is readable without owning them.

---

## 8. Factions and group stories

*Modeled on: ZZZ (factions of four released across consecutive patches), Blue Archive (schools and clubs; Group Stories), NIKKE (squads), Arknights (faction-centric events), Limbus (each Canto centered on one Sinner), FEH Forging Bonds (four characters per event).*

### 8.1 Groups as the release unit

- **Default group size: 3-5** (ZZZ: 4; Blue Archive clubs 2-5; NIKKE squads 3-5), with a clear identity: a job, an institution, a look.
- **Release:** the group's story plus its first one or two members together, the rest over the next one or two versions. Don't leave a group incomplete for long, and don't drop members to filler rarity.
- **Synergy:** a full-group team bonus rewards collecting but is never required. *Why:* AFK Arena's faction-locked requirements were criticized as a spending squeeze.

### 8.2 Three story layers for a group

1. **Main or faction arc** brings the group in.
2. **Group story** (an event or permanent group chapter) where members play off each other.
3. **Bond chapters** for each member.

*Why:* one group story sells several heroes at once; each member makes the others more interesting (Blue Archive formally separates Main, Group and Relationship stories).

### 8.3 Member rotation

Default rhythm: one group arc, then each member gets a turn as a chapter's or event's featured hero, so every fan knows their favorite's spotlight is coming. *Why:* Limbus centers each Canto on one Sinner, giving each of its fixed cast a guaranteed spotlight.

### 8.4 Pair matrix

For each group, write the pairings before the scenes: for a group of four that is six pairs, each with one line on how they get along and one tension. Pair content (lines, banter, joint events) comes from the matrix. *Why:* pair content drives fan discussion and team choices, and a matrix prevents the continuity errors that build up in long-running games with many writers.

### 8.5 Adding regions and groups

- Launch with 3-4 fully realized groups; hint at others as rivals or rumors.
- Add a new region roughly yearly, or a new group every few versions, each with its own culture, headliners and multi-version arc.
- Put the player's own home faction at the center; other groups are visited, allied or hired. Any new group can then be hosted as guests.

*Why:* a small launch cast keeps quality high (Blue Archive, ZZZ; contrast large, thin launch rosters). A new region resets novelty and hype (Genshin about one nation a year; HSR a world every few versions). Anchors like the Astral Express, Rhodes Island and Schale let any guest join.

---

## 9. Events and the live-ops calendar

*Modeled on: Genshin and HSR (6-week versions, two phases), FGO JP (yearly calendar and reruns), Arknights (Side Stories, Intermezzi, archive rules), Blue Archive (event per club, seasonal alts), NIKKE (collabs, anniversaries).*

### 9.1 The version cycle

- **Default version: ~6 weeks, two banner phases of ~3 weeks.** Every phase has at least one event.
- **Per version:** one main-story beat (main chapter or interlude), **one flagship story event**, and 2-4 light events with framing text only.
- **Per-phase split (same as 3.4):** the main chapter or interlude opens phase 1 and the flagship story event lands in phase 2, or vice versa. When the main chapter is an arc finale, that version's event drops to festival or light weight.
- Keep the rhythm. Players budget currency around it; HSR's shortened 4.5 (~5 weeks) was news.

### 9.2 Flagship story event

**Researched structure.**

- **Runs 14-21 days.** Story releases in 3-5 staggered parts over the first 4-7 days; fully readable well before the end. Keep story requirements light; don't hide parts behind long currency grinds.
- **Stars the version's new hero(es)**, who also get an event bonus, alongside existing heroes.
- **Self-contained arc** with a beginning, turn and resolution. Small hooks into the main story are welcome; nothing essential hangs on the event (Hard rule 2). Archived per the archive rule (16.3).

*Why:* focusing the expensive writing in one event per cycle keeps quality up; staggered parts give a daily login reason and a shared discussion moment; 2-3 weeks fits currency farming and catch-up (Genshin, FGO, Arknights, Blue Archive).

> **Craft suggestion (house default, not market-tested).**
>
> - **Cast:** 1 banner lead (2 if a pair launches), 1 existing favorite, 1 local NPC who wants something small, the protagonist as facilitator. No more than 6 speaking roles.
> - **NPC peaks (craft judgment):** when an NPC carries an emotional peak, let a pullable or free-cast hero carry the reaction forward so the feeling attaches to someone players keep.
> - **Beat sheet across the parts:**
>
> | Part | Beats |
> |---|---|
> | 1 | Hook image: the lead doing what they're known for, interrupted. The protagonist arrives because someone asked. The lead's want, said out loud. |
> | 2 | Complication from established world rules (no new magic system). A light, daily-life scene that proves the lead's voice. The existing favorite clashes with or recognizes the lead. |
> | 3 | Midpoint reveal that reorders the want. A plan. |
> | 4 | The plan fails for a character reason. Set-piece. |
> | 5 | The lead's choice costs them the easy win. Button: a changed habit, a line they'll repeat in bond content, one soft link outward. |

### 9.3 Light and evergreen events

Boss rushes, minigames, currency farms, login events. Framing text only: 1-3 short scenes, or a few lines per stage. *Why:* the calendar never goes dark, and writers get breathing room (Arknights Contingency Contract, Blue Archive raids, HoYo mode events). Alternate story-heavy and gameplay-heavy events; runs of text-heavy events cause skip fatigue.

### 9.4 Canon tiers

Label every event with one tier and keep the label visible to players:

| Tier | Meaning | Examples |
|---|---|---|
| **Canon** | Happened; may deepen characters; never required for the main story | Arknights Side Stories, Genshin festivals |
| **Semi-canon** | Happened, lighter tone; contradictions are a festival, a dream, "a possible afternoon" | Summer and holiday events |
| **Non-canon** | Gag, parody, April Fools, most collabs | FGO gag events, collabs |

Light events borrow characters; they don't edit the main story. Don't retcon a death, betrayal or alignment for a joke. Default: one small consequence survives from a canon or semi-canon event (a nickname, a promise, an unlocked place).

### 9.5 The yearly calendar

Default yearly beats (fit them to the game's setting; the world bible names the festivals):

| Slot | Content | Notes |
|---|---|---|
| New Year / Lunar New Year | Seasonal event, festive alts, holiday mail | Genshin Lantern Rite is an annual pillar |
| Valentine's | Light event, chat or bond extras, mail | Often gag-adjacent |
| April Fools | 1-7 day gag mode or parody | Cheap, highly shareable |
| Summer | Biggest event after the anniversary; swimsuit or holiday alts | FGO, Blue Archive, Genshin island events |
| Half-anniversary | Second peak: strong story beat, rewards | |
| Anniversary | The year's peak: main-story climax or new arc, a premium or free-choice hero, generous rewards | FGO JP 8th anniversary ~$40M in 17 days (Sensor Tower) |
| Halloween | Semi-canon or gag event, alts | |
| Christmas / year end | Seasonal event, alts, holiday mail | |
| Collab | 2-3 weeks, from year 1-2 on | 9.6 |

*Why:* anniversaries bring lapsed players back and drive revenue peaks; players treat them as a promise, and stingy ones get review-bombed (Genshin 2021).

### 9.6 Collabs

Self-contained, 1-3 chapters, 2-3 weeks, non-canon by default, one collab hero free and the rest on a limited banner. Hold the first collab until the game's own cast is established (year 1 or later). Expect few or no reruns because of licensing. *Why:* NIKKE x Chainsaw Man (20 days, 2 chapters, free Himeno); collabs too early overshadow the original cast.

### 9.7 Archive and rerun

- **Archive rule:** a design assumption (16.3), set at launch. Default: event stories become permanently readable when the event ends, or after its first rerun. Limited rewards may stay time-bound.
- **Reruns:** major story events return about 12-18 months later.

*Why:* archives turn events into long-tail content and protect newcomers (Arknights Record Restoration, HSR Conventional Memoir, Blue Archive archive tab). Reruns buffer production (industry inference): without them the team must ship brand-new events every phase (FGO and Arknights run reruns routinely).

---

## 10. Reruns, returns and alternate versions

*Modeled on: FGO (summer Servants, Alters, rerun interludes), Blue Archive (bracketed alts such as Swimsuit and New Year), Limbus (Identities of 12 fixed Sinners), NIKKE and HSR (seasonal and new-path variants), Arknights (new Records and modules for older operators).*

### 10.1 Banner reruns

**Default: a rerun brings something new in story.** An event appearance, new voice lines, a new outfit with a scene, a new bond chapter, or a link to the current arc. *Why:* gives owners a reason to return, gives new players a reason to pull, and shows old heroes still count.

### 10.2 Bench heroes

Older heroes keep appearing: cameos in events, lines in chats, a scene in interludes. **Default: every content drop gives one scene to an existing hero not on the current banner.** *Why:* heroes who vanish after their banner are one of the most common community complaints, and it weakens reruns.

### 10.3 Alternate versions

Plan alts from the start, but give each a story reason to exist. Three types:

| Type | Identity | Story requirement |
|---|---|---|
| **Seasonal** (summer, festive) | Same person, same memory and bond | A seasonal event that features them |
| **New role** (new element, path, job) | Same person, changed by events | An on-screen cause in main or event story |
| **Other self** (alter, other timeline, identity) | Different self; may differ in personality | A world-level reason such selves exist |

State the type in the UI label and in the story. The world bible decides which types the game allows; many worlds use only the first two.

*Why:* alts resell existing affection at lower writing cost. Backlash comes when an alt is only a stronger copy with no story reason, or makes the original obsolete ("paying more to stand still"). Keep alts to a pace the world can justify.

---

## 11. Hub, home base and barks

*Modeled on: Blue Archive (Café, Memorial Lobby, MomoTalk), NIKKE (Lobby, Outpost, Advise, Messenger), Arknights (RIIC base, assistant lines, faction furniture), Genshin (voice-overs, birthday mail), HSR (phone messages, the Express).*

### 11.1 Voice set per hero (default ~30-50 lines)

| Group | Count | Notes |
|---|---|---|
| Greeting / home screen | 3-5 | By time of day |
| Idle | 2-4 | No required facts |
| Tap / interaction | 3-5 | Keep it in character, not suggestive by default |
| Chat-about-self ("About me") | 2-4 | Some unlock by bond |
| About other heroes | 2-6 | From the pair matrix (8.4) |
| Bond unlocks | 3-5 | Track bond chapters |
| Combat (enter, skill, ultimate, hit, down, win) | 8-12 | Under 10 words; from the hero's backstory |
| Upgrade / ascension | 3-4 | |
| Birthday | 1 + yearly mail | Free yearly social moment per hero |
| Seasonal | 4-6 | New Year, summer, Halloween, year end |

### 11.2 Hub lines

- The home base hosts owned heroes; one is "on duty" with context lines tied to story progress, time, recent fights and login streak.
- Default: line pools change after each main chapter, so returning feels noticed.
- Hub actions take seconds. *Why:* NIKKE and Blue Archive hubs are tolerated because they are short; chore-like bases are not.

### 11.3 Pair barks

Short lines (1-2 each) that fire when two specific heroes share a team or the hub. Write them from the pair matrix; version them after major arc beats so they track the relationship.

### 11.4 Chat messages and mail

Short text threads (no new art) that deliver personality between updates and lead into bond chapters at milestones. Event and holiday mail is the same idea once a year per slot: a short note in the hero's voice, often carrying a gift. *Why:* the cheapest way to make older or less popular heroes feel looked after (MomoTalk, HSR phone, ZZZ Knock Knock, Genshin birthday mail).

---

## 12. Scene craft and pacing

*Modeled on: VN-format presentation in FGO, Arknights, Blue Archive, NIKKE, Reverse: 1999, Limbus; ZZZ's mixed formats; Blue Archive's comedy-to-climax tone.*

### 12.1 Size and stopping points

Minute targets are silent reading time, excluding battles. The project picks one column in its settings (16.1); section 13 turns these into words.

| Unit | Light / open-world | Story-first VN (Arknights, Blue Archive, FGO style) |
|---|---|---|
| Scene: one location, one objective | 1-3 min | 2-4.5 min |
| Episode: one stage or event part | 3-8 min | 5-15 min |

- Every episode ends at a clear stopping point, with a battle, choice or hook before the next. *Why:* mobile play is short bursts (benchmarks put sessions at ~8-15 minutes; treat as an estimate). Genshin quests that span "real-life days" lose the thread.
- **Alternate heavy and light.** After a death, a meal. After a battle, a walk.

### 12.2 Presentation

- Default VN layout: sprites over a background, name plate, 2-3 line text box, tap to advance, with Auto, Skip and Log.
- **One idea per line.** Default average ~12-15 words per line, which fits a 2-3 line VN box of roughly 40-90 characters per box line depending on language and font. Soft cap ~25 words; hard cap 40, only for a single joke or confession.
- **Save the bigger format for climaxes:** CG, animated cutscene, song or credits. ZZZ moves between motion comics, stills and cutscenes; Blue Archive ran credits over a song at an unexpected climax. These are the clips that get shared.
- **No black-screen narration of action.** If it matters, a character reacts to it, or the stage direction is a playable beat.

### 12.3 Exposition

- **Main-story and event scenes: the first lines restate place and goal in-world.** Skip-friendly. Bond, chat and hub scenes may start mid-moment.
- **Jargon principle:** tie every new term to something on screen, and keep the first hours light. Keep a glossary in the UI; the script must not depend on it. *Why:* WuWa's "jargon-infested" opening; Genshin added a glossary late after complaints.
- **Jargon budget: default 3 new terms per chapter**, tunable in project settings.
- **No proper noun before it is shown or glossed,** with one exception: a cold open may name something it doesn't explain when the name is the hook. Prefer common words.
- **The ignorant interlocutor** asks; someone answers in one or two lines; the next scene demonstrates.
- **Companion or mascot characters react; they don't recap.** Recaps go in the quest menu (the skip summary). *Why:* the 8,000-upvote Paimon complaint.
- Characters don't say the theme. They say what they want right now. Antagonists' points of view are revealed over time, not monologued, and a layered antagonist can later be a banner hero.

### 12.4 Tone

**Default: comedy as the everyday tone; earn the serious arc on top of it,** and turn running gags into the payoff at the climax. *Why:* Blue Archive's manzai banter under a heartfelt story; NIKKE "came for the butts, stayed for the story"; HSR's meme humor. The world bible's tone rules override this default.

### 12.5 Density

Long, dense text is fine in one or two prestige arcs a year (FGO's Avalon le Fae, ~350k words; Limbus Cantos). Don't write every scene at that density. Daily and event content stays lighter.

### 12.6 Voice and localization

- Voice selectively by default: combat lines, key story lines, climaxes. Text-only main story is acceptable in VN-style games (Arknights).
- Write for translation: avoid puns that won't survive, keep the glossary locked, and give each hero a voice guide (would say / would never say) so localizers know how far to adapt.
- Voiced lines and subtitles must match.

---

## 13. Length budgets

**Estimates, tune per project.** Nothing here is measured data. Budgets are derived from one planning rate: **225 spoken words per minute of silent reading** (an estimate; game dialogue is commonly read at roughly 200-250 wpm). Words = reading minutes x 225, rounded. Reading minutes come from 12.1 and 7.1 and exclude battles and cutscene playback. Spoken words are what `check_script.py` counts (dialogue lines only; stage directions and choices in brackets don't count).

The project picks one column in its settings (16.1) and records any override there.

| Piece | Light / open-world | Story-first VN | Scenes | Notes |
|---|---|---|---|---|
| Scene | 1-3 min · ~200-700 | 2-4.5 min · ~400-1,000 | 1 | 12.1 |
| Episode (stage or event part) | 3-8 min · ~700-1,800 | 5-15 min · ~1,100-3,400 | 1-3 | 12.1 |
| Opening (to first summon) | 5-10 min · ~1,100-2,250 | 10-20 min · ~2,250-4,500 | 4-8 | Total play under ~30 min (4) |
| Main chapter | 10-20 min · ~2,250-4,500 | 30-60 min · ~6,750-13,500 | 8-16 | 8-14 stages (3.1) |
| Arc finale chapter | up to 1.5x main chapter | up to 1.5x main chapter | | Best production goes here |
| Prestige arc chapter | project sets it | project sets it | | Only for story-first games (12.5) |
| Interlude | 5-10 min · ~1,100-2,250 | 10-25 min · ~2,250-5,600 | 3-6 | |
| Flagship event (all parts) | 15-35 min · ~3,400-7,900 | 30-70 min · ~6,750-15,750 | 10-18 | 3-5 parts, each one episode |
| Festival / gag event | 5-12 min · ~1,100-2,700 | 10-25 min · ~2,250-5,600 | 5-10 | Half stakes |
| Collab event | 10-25 min · ~2,250-5,600 | 20-50 min · ~4,500-11,250 | 1-3 chapters | |
| Light event framing | 0.5-2 min · ~100-450 | 1-3 min · ~225-675 | 1-3 | |
| Trial stage framing | ~0.5-1.5 min · ~100-300 | ~0.5-1.5 min · ~100-300 | 1 short | Lead's hook behavior; no plot |
| Group story | 10-15 min · ~2,250-3,400 | 15-30 min · ~3,400-6,750 | 4-8 | |
| Premium character story | 15-25 min · ~3,400-5,600 | 20-35 min · ~4,500-7,900 | 5-8 | 7.1 |
| Hangout-style story (lower rarity) | 5-10 min · ~1,100-2,250 | 8-15 min · ~1,800-3,400 | 2-4 | 7.1 |
| Bond chapter (each) | 2-4 min · ~450-900 | 3-6 min · ~700-1,350 | 1-2 | 3-5 per hero (7.1) |
| Bond / hub scene, short | ~0.5-1 min · ~120-250 | ~0.5-1 min · ~120-250 | 1 | 8-16 lines |
| Chat thread | ~40-150 words | ~40-150 words | — | No skip summary |
| Event / holiday mail | ~50-200 words (prose) | ~50-200 words (prose) | — | Not checked by the script |
| Profile entry | ~60-200 words (prose) | ~60-200 words (prose) | — | Not checked by the script |
| Bark / voice line | under 20 words | under 20 words | — | Combat lines under 10; not checked by the script |

When a first draft lands under budget, look for thin scenes (a beat stated instead of played) and deepen them. Don't pad. If the material calls for something shorter, record a budget override in `project.md`.

---

## 14. Planning sheets and script format

### 14.1 File header (every piece)

```
# <Title>
Layer: main chapter | interlude | flagship event | festival event | collab | light event | trial framing | group story | character story | hangout | bond chapter | hub/chat | mail | barks
Canon tier: locked | canon | semi-canon | non-canon
Arc / version: <arc id>, <version or phase>
Featured hero(es): <names>   Banner tie-in: <banner or "none">
Prerequisite: none | <main chapter id>
Reveal gate: <latest point in the world's reveal schedule this piece may use>
Budget: <MIN-MAX> spoken words (column: light | VN)
Deviations: <defaults bent, and why, or "none">
```

### 14.2 Planning sheet

Fill the fields for the piece's layer. Fields marked * apply to all layers.

- *Player fantasy (one line): what this piece sells.
- *Featured hero: want / obstacle / cost.
- *Recap blurb (3 lines, written first): becomes the in-game chapter recap.
- *Link outward (one sentence or "none"): to the main story, a group, or a future hero.
- Main chapter: arc position (arrival / complication / finale), system unlock or reward announced (per 16.3), stages list.
- Event: part split and unlock days, featured and existing heroes, event bonus heroes, canon tier.
- Character or bond: bond level or unlock, private fact revealed, habit or line gained.
- Group story: the pairs from the matrix this piece moves.
- *New locked facts (for the ledger).
- *Button line.
- *Aftertaste: new idle or hub lines, pair barks, chat thread, bond teaser.

### 14.3 Scene card (one per scene)

- Location and on-screen objective.
- Who wants what at the start; what changes by the end.
- Two lines only this character could say.
- Skip summary (Hard rule 3): what a skipper still gets.

### 14.4 Script

Scenes use `### Scene N — Title`. Dialogue is `NAME: line`, uppercase name and colon. Stage directions are in square brackets on their own line. Player choices are stage-direction lines, `[Choice a: "..."]`, `[Choice b: "..."]`, followed by a converge direction, so they are not counted as spoken words. Each story scene ends with its skip summary as a bracketed line.

```
### Scene 3 — Platform 9, before the rain
[Rain on the glass roof. MIRA holds a humming box at arm's length.]
MIRA: Gate 3 closes at six. I don't open parcels.
PROTAGONIST: Even when they hum?
MIRA: Especially then.
[Choice a: "What's in the box?"]
[Choice b: "Let's go."]
[Choices converge.]
MIRA: Nobody carries my parcels.
[Skip summary: Mira refuses to open a humming parcel she must deliver to Gate 3 before six.]
```

Section separators (`---`) between parts are fine. Planning sheets and cards go above the first scene heading so the script checker counts only the script.

### 14.5 Other formats

- **Chat thread:** `### Scene N — Chat: <hero>` with the same `NAME: line` format. No skip summary (Hard rule 3 covers story scenes).
- **Voice set / barks:** a table: Trigger | Line | Unlock. Keep under the budget in 13.
- **Profile entries and mail:** short prose under `#### Profile N — Title` or `#### Mail — Occasion`, with the unlock.
- The word-count check (`check_script.py`) is not used for voice sets, barks, profiles or mail; check those by hand against section 13.

---

## 15. Review checklist

Run after drafting, after `check_script.py` (where it applies, 14.5). Record the answers at the end of the file. A "no" on a starred item (hard rules only) means rewrite that scene, not annotate it. Everything else is a judgment call: fix, or record the deviation and why.

**Hard rules**
- [ ] * Can someone who owns none of these heroes follow the main story? (1)
- [ ] * If this is a main chapter: does it stay understandable without any event, and is every event it references archived? (2)
- [ ] * Does every story scene end with a skip summary? (3)
- [ ] * Does this contradict the hero's banner fantasy or teaser as recorded on their sheet? (5)
- [ ] * Teaser or announcement copy only: does it spoil a pullable hero's fate? (5)
- [ ] * Does this piece assume anything that contradicts the project's design assumptions? (4, 16.3)

**Structure**
- [ ] Main-story and event scenes: can a skipper state today's goal from the first two lines?
- [ ] Does the featured hero want something that isn't the protagonist?
- [ ] Is there a cause between scene N and N+1?
- [ ] Did someone change a habit, not just receive information?
- [ ] Is the button a feeling or a fact, not a trailer?
- [ ] Does this drop give one scene to a bench hero?
- [ ] Does the free story cast still matter?
- [ ] Within budget for the project's column (13)?

**Craft**
- [ ] Within the jargon budget, every term tied to something on screen?
- [ ] Line length near the 12.2 default; nothing over the hard cap?
- [ ] Does the companion react rather than recap?
- [ ] Do heavy and light scenes alternate?
- [ ] Could two characters trade lines unnoticed? (If yes, rewrite.)
- [ ] Is the protagonist a catalyst: not silent, not worshipped, not the star?
- [ ] Canon tier labeled; no joke event editing the main story?
- [ ] Project review additions (16.1) answered?

**Common failures (check, then fix or record a deviation)**
- A traditional chosen-one RPG plot chopped into patches. *(craft judgment)*
- An opening monologue that defines the cosmology. *(WuWa launch opening, section 4)*
- Three new factions and five new resource names in chapter 1. *(WuWa jargon criticism; Genshin's late glossary, 12.3)*
- A villain who explains the theme. *(craft judgment)*
- A twist that deletes the last chapter's cost. *(craft judgment)*
- Black-screen text describing a fight or a kiss. *(craft judgment)*
- The protagonist praised for existing. *(anecdotal: a single review of HSR, section 5)*
- A companion who recaps what the player just saw. *(Paimon complaint, 12.3)*
- A scene whose only job is to advertise the next banner by name. *(craft judgment)*
- Story text and gameplay objective that disagree. *(craft judgment)*
- A hero who appears for their banner and is never seen again. *(community complaints, 10.2)*

---

## 16. Plugging in a world bible and project settings

This bible is empty of content on purpose. The world bible fills the content slots; `project.md` holds the project settings, including a pointer to each world bible section.

### 16.1 Project settings (`project.md`)

Fill once per project, revise when the user changes them:

| Setting | What it holds |
|---|---|
| Premise / spine sentence | One sentence: the frame no banner may break and the meta-mystery's end state (3.2, 3.6) |
| Protagonist role | Role title, home faction, how much they speak, whether voiced (5) |
| Reveal schedule | What players may learn and when; feeds the header's Reveal gate (14.1) and the ledger (17) |
| Invention policy | What a writer may invent without asking (minor NPCs, places, terms) and what must be proposed first |
| Tone | Tone rules from the world bible; overrides 12.4 |
| Budget column | Light / open-world or story-first VN (12.1, 13), plus any budget overrides |
| Design assumptions | The 16.3 block, filled |
| Project review additions | Every world rule a draft could break; run alongside section 15 |
| Character sheet fields | The 16.4 fields, plus any project-specific fields (for example a per-scene stage tag) |

### 16.2 World-bible slots

| Slot in this bible | What the world bible supplies |
|---|---|
| Premise and spine (3.2, 3.6) | The one frame no banner may break; the meta-mystery and its end state |
| Hierarchy (3.1) | What a Part, an arc and a chapter are in this world (acts, regions, cantos) |
| Reveal schedule (14.1 Reveal gate, 17) | What players may learn and when. A line that leaks a later reveal is a continuity error, even if true |
| Opening (4) | The cold open, the modest start, the free story leads, how summoning exists in the fiction |
| Protagonist (5) | Role title, home faction, how much they speak, whether they are voiced |
| Groups (8) | Factions or quartets, their roles, fault lines and the pair matrix |
| Debut (6.3) | Each hero's want, wound, contradiction, banner fantasy, voice (would say / never say), hook behavior, bond secret |
| Unowned heroes (Hard rule 1, 7.5) | Trial or guest units, or another device (letters, rumor) |
| Alternate versions (10.3) | Which alt types exist; many worlds allow only the same person in a new role |
| Calendar (9.5) | The world's festivals and seasons that fill the yearly slots |
| Tone (12.4) | Tone rules, speech style, things that never speak, comedy ceiling |
| Hub (11.2) | The home base, who waits there, what it shows |
| Naming and new regions (8.5) | Naming conventions and how new groups are built |
| Glossary (12.3) | The locked term list and when each term may first be used |

Where the world bible is silent, use this bible's defaults. Where it is more specific, it wins. Where it contradicts a hard rule, ask the user.

### 16.3 Design assumptions

Product-side guarantees the story relies on. The project fills this once (defaults shown); writers don't re-decide it per piece, and the review checklist only asks whether a piece contradicts it.

```
Version length and phases: ~6 weeks, two phases of ~3 weeks (9.1)
First summon point: <stage or scene>; target as early as the story allows, upper bound ~30 min (4)
System unlock order: gacha, then upgrades, then base or hub, then endgame, over the first 1-2 weeks (3.7)
First-clear rewards: premium currency on main-story first clears (3.7)
Story difficulty: offered (yes / no) (3.7)
Unowned-hero device: trial / guest units | letters or rumor | other (Hard rule 1)
Bond gating: chapter thresholds <levels>; chapters reachable without duplicates: <which> (Hard rule 4, 7.3)
Baseline package per pullable hero: 7.1 default | <project version> (no hero gets nothing)
Character story access: at banner start without owning the hero; unlock currency <yes/no> (7.2)
Archive rule: event stories archived <when the event ends | after first rerun> (Hard rule 2, 9.7)
Skip, Auto, Log and skip-summary UI: present (Hard rule 3)
```

### 16.4 Character sheet

Filled from the world bible before writing a hero's story; leave nothing blank that the piece depends on, and mark guesses as proposed.

```
Name / role / group:
Rarity and release (version, phase):
Banner fantasy:
Want (now) / Need (unspoken) / Wound (one sentence) / Contradiction:
Voice: would say (5) / would never say (5) / verbal tic / silence:
Hook behavior (before speaking):
Bond secret (only the protagonist learns; not a power upgrade):
Pairs (from the matrix):
Alt policy:
Current state: habits gained, hub lines, appearances
```

The teaser line, once marketing sets it, is recorded under Banner fantasy so Hard rule 5 can be checked against it.

---

## 17. Continuity ledger

Keep one ledger per project: Fact | Layer | Locked | Owner | Reveal | Source.

- **Locked:** main-story deaths, alliances, the nature of the threat, the protagonist's role. Changing one needs an on-screen cause.
- **Soft:** event jokes, dreams, festival moments, semi-canon and non-canon content. Mark them soft in the piece header.
- **Reveal:** the earliest point players may learn the fact on screen.
- **New heroes** need a reason to be here today that uses an existing group or place. "I sensed your destiny" isn't one.
- **Time:** the main story is roughly chronological. Events happen "during the same season" unless they cite a main-story result. A main chapter may reference events; it must remain understandable without them, and any event it references must be archived (Hard rule 2).
- **Retcons:** prefer "we were wrong" over "it didn't happen". If it didn't happen, say who lied.
