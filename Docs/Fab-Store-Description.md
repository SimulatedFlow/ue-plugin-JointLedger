# JointLedger - The Joint That Held Nothing

**Unreal Engine 5.8 · Win64 · full C++ source · no third-party code · one runtime module**

A physics constraint whose two frames resolve to nothing is not an error in Unreal. The engine
calls InitConstraint only when at least one body was found, and it sets the broken flag to
false either way - so IsBroken() answers "no", nothing appears in the log, and the joint never
came into existence. From the outside it looks exactly like a healthy one. You find out when a
door does not open.

JointLedger walks every physics constraint in the running world and records what each of its
two sides actually resolved to, and whether the constraint was ever created.

```
JointLedger FAIL | 4 constraint(s) | 97 sample(s) @ 3.9 Hz (4.0 wanted) in 25.0 s
            | healthy 50% | 1 both-empty, 1 missing body, 0 never created, 0 asleep, 0 not judged

Joint_HoldsNothing  empty     empty    NO   BOTH FRAMES EMPTY - neither side names an actor
  or a component, so the engine never called InitConstraint at all. IsBroken() still returns
  false and nothing was logged: this joint holds nothing and looks healthy
Joint_Typo          resolved  MISSING  yes  NAMED BODY MISSING - frame 2 asks for
  'Cube_Swing_I', but that actor has no component of that name. It has: 'Cube_Beam',
  'Cube_Hanging', 'Pillar_Healthy', 'Cube_Swing', ... Fix the name, not the joint
```

Every number above comes from a real run of the demo map that ships with the plugin.

**One empty frame is NOT a finding**

This is the decision that separates this tool from a naive one. The engine's own comment, in
the function that resolves the two sides, reads: "If neither actor nor component name
specified, joint to 'world'". An empty frame is how you anchor a body to the world - how a
door, a swing and a pendulum are built. A tool that reports it would flag half of every level.

Only both sides empty is a fault. And the report says so on the line itself: a world anchor
reads "ok - one side is empty on purpose: this is an anchor to the world". Leaving that blank
would make the reader wonder whether the empty side was overlooked.

**Two independent measurements of the same joint**

The names are resolved exactly the way the engine does it, including the case nobody guesses:
a ChildActorComponent resolves to the root component of its child actor, not to itself. The
existence of the joint is read separately, from the constraint handle. One concludes from what
is written down, the other measures what happened. A finding that rests on one measurement is
a guess; two that agree are a result.

**It names the components the actor really has**

Without that list the report states a symptom. With it, it states the typo: frame 2 asks for
'Cube_Swing_I', and the actor has 'Cube_Swing'. The console command JointLedger.Bodies prints
the same thing for every missing-body finding while you are hunting.

**It keeps the farthest state, not the last**

A constraint that is created late, or a ragdoll that wakes up late, was still there. Taking
the state at the end of the run would report every tidied-up ragdoll as never existing -
correctly computed and completely worthless.

**"Never" needs a denominator**

MinObservedSeconds is 5 s by default, not the fraction of a second a montage checker uses, and
that is deliberate: a constraint is a state, not an event. The only thing that can change is
whether the bodies simulate, and a ragdoll is often switched on seconds after it spawns. The
gate warns you when you give it less time than the floor.

**Honest numbers**

The report prints the achieved sample rate beside the wanted one, plus the longest gap between
two samples. Where there is no denominator there is no number: with nothing judgeable the
healthy share reads n/a and the JSON carries null, never 0. A run in which no constraint
existed at all is an error, not a pass - it checked nothing.

**A gate for your build server**

UnrealEditor-Cmd.exe YourProject.uproject /Game/Maps/YourMap -game -unattended \
  -ExecCmds="JointLedger.Gate 60"

Exit code 0 clean, 1 warnings, 2 errors. The gate samples once more before it judges: at 4 Hz
the last reading is otherwise up to a quarter second old, and a ragdoll can wake up in that
time. The report lands in Saved/JointLedger/report.json with every number the summary
mentions.

**The demo map shows it rather than claiming it**

L_JointLedgerDemo puts four joints on one actor, left to right: healthy, world anchor, holds
nothing, typo. The second and third look alike - one empty frame each way - and the plugin
separates them. In the picture the anchor hangs as a swinging pendulum while the typo's cube
simply falls.

All four share an actor on purpose: the record key is (actor, constraint), and a ragdoll
carries dozens of joints on a single actor. Keying by actor alone would average away the one
finding you are looking for. The pillars are coloured by the plugin's verdict, not by the
director's expectation; the director counts what it built and logs both numbers, and if they
disagree that is a finding against the plugin.

**Honest about its own limits**

* It records what each constraint was in this run. A joint on an actor that never spawned is not in the report at all - this tool watches what ran, it does not search your project.
* An empty frame is not a missing body. That distinction is the whole point.
* "Nothing simulates" is a warning, never an error: a ragdoll switched on later is indistinguishable from one that never wakes up.
* "Never created" is reported only when at least one side did resolve - otherwise the report names the cause instead of the consequence.
* A constraint watched for less than MinObservedSeconds is not judged.
* It tracks at most 512 constraints and says so when it hits the cap.
* It is not a profiler: frames, names and simulation state, never milliseconds and never solver iterations.
* It changes nothing - no constraint is created, broken or re-targeted.

**Technical**

One runtime module, no editor module. A tickable world subsystem samples the world, an AHUD
subclass draws the box with UCanvas - no widget, no material, no asset.

22 pure rule functions on a Blueprint function library, eight automation tests, and every rule
sabotaged once to prove the test bites - 12 of 12 sabotages caught. That run found a real gap:
one rule could be inverted without any test complaining, because another branch hid the
difference. The counter-check is in the suite now.

Dependencies: Core, CoreUObject, Engine and DeveloperSettings. No PhysicsCore - that is the
obvious reflex and it is wrong: physical materials live there, the constraint component does
not.
