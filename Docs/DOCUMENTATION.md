# JointLedger — Documentation

**Unreal Engine 5.8 · Win64 · full C++ source · one runtime module · no third-party code**

## What it does

JointLedger samples every `UPhysicsConstraintComponent` in the running world and records, per
constraint, what each of its two frames resolved to and whether the constraint instance was
ever valid. At the end it judges, writes a JSON report and can end the process with an exit
code your build server understands.

It changes nothing. No constraint is created, broken or re-targeted.

## Install

1. Copy the plugin into `YourProject/Plugins/JointLedger`.
2. Enable it (Edit → Plugins → Code Plugins → JointLedger), restart.
3. Play. `JointLedger.Show` draws the panel.

## The five findings

| Finding | Severity | What it means | Where the fix is |
| --- | --- | --- | --- |
| `BOTH FRAMES EMPTY` | error | Neither side names an actor or a component | two names |
| `NAMED BODY MISSING` | error | A name is set, no component has it | **one name** |
| `never created` | error | Bodies resolved, the constraint handle never became valid | physics setup |
| `nothing simulates` | warning | Bodies exist, none of them ever simulated | `SetSimulatePhysics` |
| `anchored to world` | warning | One frame empty — **only when you ask for it** | nothing; it is intentional |
| `not judged` | — | Watched too briefly, or the owner had no name | nothing |

`anchored to world` is **off by default** (`bWorldAnchorIsAFinding = false`). Switch it on
only if your project genuinely never uses world anchors; otherwise it will list every door.

## Settings

Project Settings → Plugins → JointLedger.

| Setting | Default | Why |
| --- | --- | --- |
| `bShowOverlay` | true | Draw the panel. |
| `SampleHz` | 4.0 | A constraint is a state, not an event. Sampling faster costs time and measures the same thing. |
| `MinObservedSeconds` | 5.0 | "Never simulated" needs a denominator. A ragdoll is often switched on seconds after it spawns. |
| `MaxBothFramesEmpty` | 0 | Always an error; it only fails to look like one. |
| `MaxNamedBodyMissing` | 0 | Same. |
| `MaxNeverInitialised` | 0 | Same. |
| `MaxNothingSimulates` | 4 | Sleeping ragdolls are normal. |
| `bWorldAnchorIsAFinding` | **false** | An empty frame is how you anchor to the world. |
| `bReportNothingSimulates` | true | |
| `bBothFramesEmptyIsError` | true | Switching it off makes it a **warning**, never silence. |
| `bNoConstraintsIsAnError` | true | A run without constraints checked nothing. |
| `bReportUnjudged` | true | An abstention you cannot see is indistinguishable from a missed joint. |
| `MaxTrackedConstraints` | 512 | The cap is reported when it is hit. |
| `MaxBodyNamesListed` | 8 | The list is the *reason* for the typo finding; the cap is disclosed. |

## Console commands

| Command | What it does |
| --- | --- |
| `JointLedger.Show` / `.Hide` | Panel on/off. |
| `JointLedger.Reset` | Forget everything and start the run over. |
| `JointLedger.Dump` | Every constraint to the log, findings and all. |
| `JointLedger.Bodies` | For every missing-body finding: what that actor really has. |
| `JointLedger.Report [name]` | Write `Saved/JointLedger/<name>.json`. |
| `JointLedger.Gate <s> [-noexit]` | Measure, report, exit 0/1/2. |

`-noexit` measures and judges but does not end the process — otherwise the gate could never
be tried out in a running session, and a gate you can only fire once is an untested gate.

## The report

`Saved/JointLedger/report.json`:

| Key | Note |
| --- | --- |
| `verdict`, `exitCode` | PASS/WARN/FAIL and 0/1/2. |
| `constraintsSeen`, `samplesTaken` | |
| `wantedHz`, `achievedHz` | Both, always. A measurement that hides its own quality asks to be believed rather than checked. |
| `longestGapSeconds` | The worst gap between two samples. |
| `healthySharePercent` | **`null`** when nothing was judgeable — never `0`. |
| `anyConstraintSeen`, `hitTrackingCap` | |
| `records[]` | Per constraint: `frame1`, `frame2`, `wanted1/2`, `resolved1/2`, `isWorldAnchor`, `wasEverValid`, `body1/2EverSimulated`, `observedSeconds`, `fault`, `note`, `availableBodyNames`, `bodyNamesOmitted`. |

## How a frame is resolved

Exactly as the engine does it (`PhysicsConstraintComponent.cpp:50-147`):

1. `OverrideComponent1/2` if set — done.
2. Otherwise the actor from `ConstraintActor1/2`; if that is null, the **owner**.
3. If neither an actor nor a name was given -> **empty frame**. The engine's own comment at
   this line reads: `// If neither actor nor component name specified, joint to 'world'`.
4. An empty name → the actor's `RootComponent`.
5. Otherwise a search through `GetComponents()` by name. If the name hits a
   `UChildActorComponent`, the engine takes the **root component of the child actor**.
6. Nothing found → **missing** (the typo case).

Steps 3 and 6 look identical from a distance and mean opposite things. Keeping them apart is
the whole job.

## Limits

1. It records what each constraint was in **this** run. A joint on an actor that never
   spawned is not in the report at all — this tool watches what ran, it does not search your
   project.
2. A constraint watched for less than `MinObservedSeconds` is not judged.
3. **One empty frame is not a finding.** Only both sides empty is a fault.
4. `nothing simulates` is a warning, never an error.
5. Validity is read from the constraint handle itself, not concluded from the names.
6. At most `MaxTrackedConstraints` constraints; the cap is reported.
7. **It is not a profiler:** frames, names and simulation state, never milliseconds and never
   solver iterations.
8. It changes nothing.

## Tests

Eight automation tests under `JointLedger.*`, run them with
`Automation RunTests JointLedger`. Every rule was **sabotaged once** to prove the tests bite —
12 of 12 sabotages caught. That run found a real gap: `IsBothFramesEmpty` could be flipped
from `&&` to `||` without a single test complaining, because the world-anchor branch hides
the difference in the classifier. The counter-check is in the suite now.

## Technical

One runtime module, no editor module. `UTickableWorldSubsystem` + `AHUD`/`UCanvas`; no widget,
no material, no asset. Dependencies: Core, CoreUObject, Engine, DeveloperSettings. **No `PhysicsCore`** - that is
the obvious reflex and it is wrong: physical materials and settings live there, the constraint
component does not.
