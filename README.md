# JointLedger — The Joint That Held Nothing

**Unreal Engine 5.8 · Win64 · full C++ source · no third-party code · one runtime module**

A physics constraint whose two frames resolve to nothing is not an error in Unreal. The
engine only calls `InitConstraint` when at least one body was found, and it sets the broken
flag to `false` either way — so `IsBroken()` answers "no", nothing appears in the log, and
the joint simply never came into existence. From the outside it looks exactly like a healthy
one. You find out when a door does not open.

JointLedger walks every physics constraint in the running world and records, for each one,
which bodies it actually resolved to and whether the constraint was ever created.

```
JointLedger FAIL | 4 constraint(s) | 97 sample(s) @ 3.9 Hz (4.0 wanted) in 25.0 s
            | healthy 50% | 1 both-empty, 1 missing body, 0 never created, 0 asleep, 0 not judged

constraint          owner                     frame 1    frame 2   valid  note
Joint_HoldsNothing  JointLedgerDemoDirector_0 empty      empty     NO     BOTH FRAMES EMPTY -
  neither side names an actor or a component, so the engine never called InitConstraint at
  all. IsBroken() still returns false and nothing was logged: this joint holds nothing and
  looks healthy
Joint_Typo          JointLedgerDemoDirector_0 resolved   MISSING   yes    NAMED BODY MISSING -
  frame 2 asks for 'Cube_Swing_I', but that actor has no component of that name.
  It has: 'Cube_Beam', 'Cube_Hanging', 'Pillar_Healthy', 'Cube_Swing', … (and 2 more).
  Fix the name, not the joint
```

Every number above comes from a real run of the demo map that ships with the plugin.

## One empty frame is NOT a finding

This is the single decision that separates this tool from a naive one. The engine's own
comment, in the function that resolves the two sides:

```cpp
// If neither actor nor component name specified, joint to 'world'
if(Actor != NULL || ComponentName != NAME_None)
```

An empty frame is **how you anchor a body to the world**. It is how a door, a swing and a
pendulum are built. A tool that reports it would flag half of every level — the same mistake
a socket checker makes when it flags `hand_r` for not being a socket.

Only **both** sides empty is a fault. The report says so on the line itself: a world anchor
reads `ok - one side is empty on purpose: this is an anchor to the world`, because leaving
it blank would make the reader wonder whether the empty side was overlooked.

## Two measurements of the same joint, on purpose

* **The names** — resolved the way the engine does it: override first, then the actor (or the
  owner), an empty name meaning the root component, otherwise a search through the actor's
  components. Including the case nobody guesses: a `UChildActorComponent` resolves to the
  **root component of its child actor**, not to itself.
* **The handle** — `ConstraintInstance.IsValidConstraintInstance()`, which is simply whether
  the constraint handle is valid.

The first concludes from what is written down, the second measures what happened. When a
finding rests on one measurement it is a guess; when two independent ones agree it is a
result. It also means a mistake in either path shows up instead of passing silently.

## What a finding does *not* mean

* **It is not a claim about your project.** JointLedger watches what *ran*. A joint on an
  actor that never spawned is not in the report at all.
* **An empty frame is not a missing body.** See above — that distinction is the whole point.
* **"Nothing simulates" is a warning, never an error.** A ragdoll that is switched on later
  is indistinguishable from one that never wakes up. It is reported because that is also
  where a forgotten `SetSimulatePhysics` hides.
* **"Never created" is not a physics problem by itself.** It is reported only when at least
  one side *did* resolve — otherwise it is the consequence of another finding, and the
  report names the cause instead.
* **It is not a profiler.** Nothing here is a performance statement: frames, names and
  simulation state, never milliseconds and never solver iterations.
* **The list can be capped.** It tracks at most **512** constraints; when the cap is hit the
  report says so and the counts are incomplete. A silent cap would read like completeness.

## It can say "I don't know"

Not judged is not a finding and never changes the verdict. A constraint is not judged when it
was watched for less than `MinObservedSeconds`, or when its owner never had a name.

**`MinObservedSeconds` is 5 s here, not the fraction of a second a montage checker uses** —
and that is deliberate. A constraint is not an event, it is a state. The only thing that can
change is whether the bodies simulate, and a ragdoll is often switched on seconds after it
spawns. The gate warns you when you give it less time than the floor.

## Honest numbers

The report prints the **achieved** sample rate beside the wanted one, plus the longest gap
between two samples. Where there is no denominator, there is no number: with nothing
judgeable the healthy share reads `n/a` and the JSON carries `null` — never `0`, which would
be a figure nobody measured.

A run in which **no constraint existed at all** is an **error**, not a pass. It checked
nothing.

## A gate for your build server

```
UnrealEditor-Cmd.exe YourProject.uproject /Game/Maps/YourMap -game -unattended \
  -ExecCmds="JointLedger.Gate 60"
```

Exit code **0** clean, **1** warnings, **2** errors. The gate samples once more before it
judges — at 4 Hz the last reading is otherwise up to a quarter second old, and a ragdoll can
wake up in that time. The report lands in `Saved/JointLedger/report.json`.

Console commands: `JointLedger.Show`, `.Hide`, `.Reset`, `.Dump`, `.Bodies`, `.Report`,
`.Gate`. `.Bodies` is the one that helps while hunting: for every constraint whose named body
is missing, it prints what that actor really has.

## The demo map shows it rather than claiming it

`L_JointLedgerDemo` puts **four joints on one actor**, left to right: healthy, world anchor,
holds nothing, typo. The second and third look alike — one empty frame each way — and the
plugin separates them.

All four share an actor on purpose: the record key is **(actor, constraint)**, and a ragdoll
carries dozens of joints on a single actor. Keying by actor alone would average away the one
finding you are looking for.

The pillars are coloured by the **plugin's** verdict, not by the director's expectation. The
director counts what it *built* and logs both numbers; if they disagree, that is a finding
against the plugin, not against the map.

## Technical

One runtime module, no editor module. A tickable world subsystem samples the world, an `AHUD`
subclass draws the box with `UCanvas` — no widget, no material, no asset.

Dependencies: Core, CoreUObject, Engine and DeveloperSettings. **No `PhysicsCore`** — that is
the obvious reflex and it is wrong: physical materials live there, the constraint component
does not.

<!-- SF-STORE-BLOCK:BEGIN -->
## 🛒 Source-available — see before you buy

This repository contains the **full source** of a commercial Unreal Engine plugin. It is **source-available, not open source**: read it, evaluate it, then buy a license to use it. See **the Fab Content License Agreement / Unreal Engine EULA (purchase required)**.

**Get it / Buy:**
- **Buy on Fab** (this plugin): https://www.fab.com/listings/28474fb4-5444-4608-8720-333188b90445
- Fab store — all our UE5 plugins: https://www.fab.com/sellers/Silvan%20Teufel

### 📬 **Free UE5 Snippet-Pack**

10 ready-to-use C++/Blueprint building blocks (subsystems, versioned saves, async nodes, editor tooling) — MIT licensed. Get it by joining the newsletter — plus a heads-up when something new ships. Double opt-in, unsubscribe in one click, no address sharing.

👉 **[Get the free pack](https://silvan.teufel-engineering.com/newsletter/plugins/?q=gh)**

_© 2026 Silvan Teufel. All rights reserved._
<!-- SF-STORE-BLOCK:END -->
