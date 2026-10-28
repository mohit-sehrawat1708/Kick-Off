# Kick Off

11-a-side football in Unreal Engine 5.8: a full-size pitch, MetaHuman players, a 30,000-seat stadium with a 3D crowd,
and an AI opponent. Prototype; the in-game title is still its working name, *Pitch Kings*.

## Download and play (Windows)

1. Download `KickOff_Windows_v1.5.zip` from the [latest release](../../releases/latest) and unzip the whole folder.
2. Run `PitchKings.exe`. If Windows says `MSVCP140.dll` / `VCRUNTIME140.dll` is missing, run
   `Engine\Extras\Redist\en-us\vc_redist.x64.exe` once. SmartScreen: *More info → Run anyway* (the game is not code-signed).
3. `HOW_TO_PLAY.txt` in the zip has the controls and system requirements (Windows 10/11 64-bit, DirectX 12 GPU, 8 GB RAM).

## What's in this repository

The C++ source (`Source/`), config, and the Python / PowerShell tools that generate the stadium meshes, materials,
crowd and player heads (`Tools/`). The `Content/` folder (13 GB of assets) is **not** included: it holds Epic Games'
City Sample Crowds and MetaHuman source assets, which may not be redistributed. To build from source you need UE 5.8,
City Sample Crowds (free on Fab) in `Content/CitySampleCrowd`, and MetaHuman Creator; the scripts in
`Tools/editor_scripts` rebuild the game's own assets.

## Credits

- Football boots: "Vrfs boots" by [zanman730](https://sketchfab.com/zanman730),
  [CC Attribution 4.0](https://sketchfab.com/3d-models/vrfs-boots-b613346d117d49ad84073ac734396ece), recoloured and refitted.
- City skyline: "Homecoming Center Rooftop" panorama from [Poly Haven](https://polyhaven.com), CC0.
- Player heads: MetaHuman Creator presets (Epic Games). Clothing and crowd: City Sample Crowds by Epic Games (Fab).
- All clubs and players are fictional.

---

# Developer notes (Pitch Kings — realistic base)

Full-size 11-a-side football on a 105 × 68 m pitch with UE5 mannequin players, realistic daylight and a
code-built stadium. This is the foundation for future upgrades (Motion Matching, real animations, menus, modes).

## Run it

1. Open `PitchKings.uproject`. The editor opens `/Game/Maps/Stadium` (sun, sky, clouds, fog, post process + the `PKPitch` stadium actor).
2. Press **Play** (Alt+P) and click into the viewport. You are **BLUE** (white/blue kit), attacking right.

Rebuild after C++ changes (editor closed):

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" PitchKingsEditor Win64 Development "-Project=%CD%\PitchKings.uproject" -WaitMutex
```

Self-test (AI vs AI, logs stats, screenshots to `Saved/Screenshots`, quits after ~165 s). Add `-PKCloseCam` for a close animation camera:

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%CD%\PitchKings.uproject" -game -PKAutoTest -windowed -ResX=1600 -ResY=900
```

## Front end

Launching the game (or pressing Play in the editor) opens the front end over a slow orbit of the stadium:

| Page | What it does |
| --- | --- |
| Landing | Pitch Kings logo, "press any key" |
| Home | KICK OFF (you play BLUE), SPECTATE AI vs AI, TEAM SETUP, CONTROLS, QUIT; the next-match card shows both clubs, formations, styles, team ratings and your starting XI |
| Team setup | formation (4-4-2, 4-3-3, 4-2-3-1, 3-5-2, 5-3-2), play style, opponent club; pitch view of the XI and the 18-man squad. Pick a position on the pitch, then a squad player (bench players swap in, two positions swap the starters). Ring colour = position fit (blue natural, orange nearby, red out of position: out-of-position players play with lower ratings). AUTO PICK BEST XI, RESET TEAM, player detail with all six ratings and traits |
| Local 1v1 | two players on one screen, each with their own device: every connected device shows as a chip; push LEFT (BLUE, player 1) or RIGHT (RED, player 2) on that device, Enter / A / Start kicks off. Keyboard vs gamepad or gamepad vs gamepad. Arrow buttons work with the mouse |
| Settings | CONTROLS: rebind every keyboard action (click, press a key; Esc cancels, Backspace clears; a key already in use swaps), invert left/right and up/down, stick dead zone, vibration. GRAPHICS: display mode, resolution, overall quality + shadows / textures / effects / anti-aliasing / view distance, render scale, VSync, frame-rate limit, FPS counter, auto-detect. GAMEPLAY: match length, difficulty (computer team ratings), camera distance, shirt numbers, help panel, mini-map. Saved in the `PitchKingsSettings` slot; graphics in GameUserSettings.ini. Also reachable from the pause menu |
| Controls | keyboard and gamepad table | (shows your current bindings)
| Pause | Esc / P / Start in a match: resume, restart, main menu, quit |

Full time: R / Enter / Start plays again, Esc / B goes back to the main menu. Mouse, arrows / D-pad + Enter / A, and Esc / B all work in the menus.
The team sheet is saved (SaveGame slot `PitchKingsTeam`) and restored on the next launch. Clubs and players are fictional and generated from fixed seeds
(`PKTeamData.cpp`): Pitch Kings FC (you) and four opponents of different strength and style. The logo is drawn by `Tools/make_logo.py`
and imported by `Tools/editor_scripts/import_ui.py` into `Content/PitchKings/UI`.
Command-line test modes (`-PKAutoTest`, `-PKSpectate`, ...) and `-PKNoMenu` skip the front end; `-PKMenuShots` screenshots every page.

## Local 1v1 and controllers

- Input goes through `FPKInputRouter` (`PKInput.*`), a Slate input pre-processor that tags every key / stick event with its device
  (keyboard & mouse, gamepad 1, gamepad 2, ...). Single player: every device controls you. Local 1v1: each team reads only its
  own device, so the keyboard cannot move player 2 and the gamepad cannot move player 1.
- Each team has its own controlled player (gold ring and P1 tag for BLUE, cyan ring and P2 tag for RED), its own player card,
  its own shot-power bar and its own controls panel. Control follows the ball for both teams (passes, receptions, restarts, defending switch).
- Either player can pause (Esc / P / Start). Play style per player: keyboard 1-5, gamepad D-pad left / right.
- Controller feedback: rumble on kicks (harder for shots), being tackled or fouled, shoulder challenges, goals, and when you pick a side.
- Help panel shows gamepad buttons as soon as a gamepad is used (keyboard keys otherwise); in 1v1 each player sees their own device's prompts.
- Radial stick dead zone; analog triggers count for sprint / hold; held keys are released if the window loses focus.
- `-PKVersusTest`: keyboard vs a synthetic gamepad through the real router, setup page, kick-off, independent movement and pause (logs `[VersusTest]`).

## Packaged build (play on other PCs)

`Downloads\PitchKings_Windows_v0.5.zip` (266 MB; 400 MB unpacked) is a Windows 64-bit Shipping build: unzip, run `PitchKings.exe`
(`HOW_TO_PLAY.txt` inside has requirements, controls and the VC++ runtime installer path). Rebuild it with:

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%CD%\PitchKings.uproject" -noP4 -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -iostore -compressed -prereqs -archive -archivedirectory="%CD%\..\PitchKings_Build" -nodebuginfo -unattended
```

Measured (full match, 1600 x 900): about 0.9 GB RAM (1.6 GB committed), 0.65 GB video memory. Packaging notes: assets the C++
loads by path are listed in `DirectoriesToAlwaysCook` (DefaultGame.ini); the MCP editor plugins are editor-only; the stadium
rebuilds itself in `BeginPlay` because construction-script parts are not saved into cooked maps.

## Controls

| Action | Keyboard | Gamepad |
| --- | --- | --- |
| Move | WASD / arrows | Left stick |
| Sprint | Left Shift | RT |
| Jockey (no ball) / Shield or close control (with ball) | hold Left Ctrl | hold LT |
| Effort touch: knock the ball any way (tap short, hold long) | F + direction | R3, or RB + right stick flick |
| Controlled sprint (with ball): quicker than a jog, a touch every step | hold R | hold RB |
| Pass to teammate in stick direction | J or Space | A |
| Precision pass: to the exact spot aimed at (hold for power) | R + pass / through / lob | RB + A / Y / X |
| Swerve pass: bends round the defender in the lane | Ctrl + pass / through / lob | LT + A / Y / X |
| One-two (pass and run; your next pass returns it to the runner) | Z + pass | LB + A |
| Send a team-mate on a run in behind | tap Z | tap LB |
| Header / volley on demand (ball in the air coming to you) | pass or shoot | A or B |
| Through ball | L | Y |
| Lob / cross | I | X |
| Shoot (hold to charge, release) | K | B |
| Shot technique (held at release): finesse / chip / power / low driven | R / Z / R+Z / Ctrl | RB / LB / RB+LB / LT |
| Timed finishing: press shoot again just before the strike | K again | B again |
| Fake shot (while charging a shot) | pass | A |
| Standing tackle (no ball) | E | B |
| Slide tackle (no ball) | C | X |
| Drag-back (with ball) | G | D-pad down |
| Body feint (with ball) | T | D-pad up |
| Flick-up (pop the ball over a close defender) | V | L3 |
| Shoulder barge (no ball, next to the carrier) | X | A |
| Teammate contain: the nearest team-mate presses the carrier | hold R (no ball) | hold RB |
| Keeper rush: your keeper charges the carrier | hold L (no ball) | hold Y |
| Skill moves (relative to facing): forward rainbow / nutmeg, diagonal step-over, sideways ball roll, back roulette | B + direction | right stick flick |
| First touch | stick direction = touch direction, hold Ctrl / LT = cushion | |
| Team play style | 1 Balanced · 2 Possession · 3 Counter · 4 Long Ball · 5 High Press | D-pad left / right |
| Game plan mentality (Ultra defensive .. Ultra attacking) | 9 / 0 | LT + D-pad left / right |
| Substitutions | pause menu, SUBSTITUTIONS | Start, SUBSTITUTIONS |
| Skip goal replay | pass / shoot | A / B |
| Switch player (no ball) | Q | LB |
| Toggle help · Shirt numbers on/off · Restart after full time | H · N · R / Enter | — · — · Start |
| Pause menu · Main menu after full time | Esc / P · Esc | Start · B |

## FC 24-style features (added layer by layer)

1. **Dribbling**: Controlled Sprint (700 cm/s, a touch every step, the AI uses it under moderate pressure); Effort Dribble
   Touch in any direction (also cancels a shot being charged; sideways knocks with the outside of the foot).
2. **Passing**: Precision Pass with a power bar (ground pass arrives at ~3 m/s, the lob lands on the spot with air drag
   solved exactly); Swerve on demand (bends round the defender nearest the lane, also along the ground); headers /
   volleys on demand (the player attacks the ball at head height); crosses into the box are whipped in flatter.
3. **Shooting**: finesse, chip, power (longer wind-up), low driven; timed finishing (green tightens the shot, red spreads
   it); quick-release taps in the box; fake shot; the AI picks techniques (chip when the keeper is off his line ...).
4. **Defending**: teammate contain (second-man press on demand); a dynamic back line that steps up and drops as one, covers
   a fullback caught upfield, recovery sprints. Downed players are not chosen as pass targets.
5. **Team play**: send a run, one-two (also played by the AI), game plans (mentality pushes the block, line and press;
   the computer goes for it when losing late and sits deeper when winning).
6. **Goalkeeper**: keeper rush on demand, one-on-one narrowing and smothers, claiming crosses, reading the curl of shots.
7. **Skill moves**: step-over, ball roll, roulette (full spin), rainbow flick, nutmeg, with procedural leg poses and a
   chance to wrong-foot the defender; technical AI players use them.
8. **Match systems**: stamina (sprinting drains it; a tired player is slower, can't sprint when empty, is less accurate;
   bar under your player), two halves with ends changed, substitutions from the pause menu (5; the computer makes its
   own at stoppages), advantage rule (called back if the ball is lost straight away), goal celebration and an instant
   replay (5 s recorded at 30 Hz incl. kicks / dives / headers; skip with pass / shoot).
9. **PlayStyles**: 23 in all (new: Whipped Pass, Incisive Pass, Long Ball Pass, Trickster, Quick Step, Relentless,
   Bruiser, Intercept, Block, Aerial, Jockey, Acrobatic, Far Throw, Dead Ball, Chip Shot, Slide Tackle), each with a
   gameplay effect; elite players (OVR 80+) have a PlayStyle+ version of one (shown with +, ~60 % stronger).
10. **AcceleRATE**: Explosive (quick off the mark), Controlled, Lengthy (slow start, faster in a full sprint), from the
    player's attributes; shown in the team sheet.

`-PKAutoTest -PKFeatureTest` runs staged scenarios for every layer and logs the measurements (`-PKFeatureTest=N` runs
layer N only): precision / swerve / lob accuracy, headers on demand, each shot type, timed finishing, fake shot,
teammate press on / off, line spread new / old, runs and one-twos, game plans, keeper rush / one-on-ones / crosses
new / old (`-PKOldKeeper`, `-PKOldLine`), each skill move vs a defender, advantage, an AcceleRATE race.
`-PKSubsTest` and `-PKReplayTest` screenshot the substitutions page and a goal celebration + replay.

## Animation fitted to the ball

- **Kicks**: every clip's striking-foot speed at contact is measured at start-up. A kick estimates how fast the ball
  will leave, picks the technique that fits (side-foot pass up to ~11 m/s, laces / running strike above; the player
  slows into a soft pass on the run) and plays it at the rate that makes ball speed ~1.3x (side foot) to ~1.45x
  (laces) the foot speed. The wind-up keeps its length (a faster swing starts further back in the run-up), so the
  power shot still winds up longer and the quick release shorter. Shot speed rises only slowly above ~130 km/h.
  The autotest line reports `kick fit avg / misfit` (58 % of kicks misfit before, ~7 % after).
- **Keepers**: a shot on goal is read early and the dive or catch starts so the hands reach the ball at the clip's
  contact frame (late reactions join the dive near its stretch instead of starting it after the catch).
- **Headers and thigh control** play at the rate that meets the ball on its own time (joining the move part-way if
  needed); the ball keeps flying instead of hanging in the air waiting for the head.
- **Goal celebration**: the "aeroplane" - arms flat and swept back, hands open (fingers straightened out of the run
  clip's fist), banking from side to side; every arm, hand and finger segment is aimed outright so the run cycle
  can't twist them. `-PKCelebrateShots -PKNoMenu` screenshots a celebration run from the side, front and back.

## Kits and player looks

Every player wears his club's kit and has his own look, all painted on the UE mannequin by two materials built by
`Tools/editor_scripts/make_kit_material.py` (run headless with `UnrealEditor-Cmd ... -run=pythonscript -script=...`):

- **M_PK_Kit** (body): each point of the mesh is classified by its distance to the mannequin's bones in the reference
  pose (pre-skinned position), so the shirt, sleeves (short, keeper long), shorts, socks with turnover, boots (sole and
  side flash), collar / cuff / hem trims, skin, hair, brows, eyes, mouth and beard land in the right places with no
  textures. Shirt patterns: plain, stripes, hoops, sash, halves, pinstripes. Block shirt numbers on the back, crest on
  the chest.
- **M_PK_KitShell** (masked, world position offset): a second copy of the body that follows its pose (leader pose)
  and stands the cloth off the skin: sleeve and shorts hems, flared shorts, sock tops, hair volume.
- Club kits (`PKTeams::MakeKit`): Pitch Kings FC blue pinstripes / white shorts, Red Rovers red-white hoops, Crimson
  Athletic crimson-black stripes with gold, Union Rojo red with a white sash, Scarlet City scarlet-maroon halves; each
  keeper in his own colours with long sleeves and gloves.
- Player looks (`PKTeams::MakeLook`, fixed per player): 7 skin tones, 6 hair colours, 5 hair styles (shaved .. big),
  stubble / beard, 7 boot colours, height 0.95-1.04 and build following the Physical rating.
- `-PKKitShots -PKNoMenu` lines both teams up and screenshots fronts, backs, keeper and a head close-up.

## Dribbling (Active-Touch style)

- Steps are read from the animated feet: a step is the swinging foot passing under the body near the ground
  (`APKFootballer::UpdateFootSteps`), which also measures each player's real step rhythm. The ankle bones are used:
  the toe bones are dropped from the lower mesh LODs, so with the match camera far players stopped "stepping".
- A dribble touch happens on a step, with the foot that is swinging, when the ball is at that foot: every step in close
  control, every 2nd step at jog, every 3rd-4th when sprinting (fewer, longer touches with poor Control). With an
  opponent within 3.5 m the rhythm is one step shorter (more, smaller touches).
- Turns are taken with the outside of the foot on the side of the turn (right foot cutting right): the cut waits for
  that foot's step, sends the ball up to 55 degrees across (40 with the laces) and plays a procedural foot pose (leg
  out, toes in, rolled onto the outer edge). The other foot only cuts with its inside if the ball would be overrun
  first. Gentle steering (12-35 degrees) also prefers the outside foot, waiting one step at most.
- Off-step touches are a last resort: if the body is about to run over the ball and no foot comes through in the next
  0.09 s; if a foot swung in the last 0.12 s it takes it.
- Each touch is planned: the ball gets exactly the speed (rolling friction included) to arrive where that player's foot
  will be N steps later, using his real acceleration towards his top speed. Low Control adds direction / weight error.
- Between touches nothing pulls the ball: the player lengthens or shortens his stride a little (up to +18 % / -28 %) so the
  foot meets it, and if the ball is off the requested line (after a turn or a bad touch) he steers to the ball first.
- No input: the ball is stopped under the sole. Shielding keeps the old close-touch behaviour.
- `-PKDribbleTest` (with `-PKAutoTest`) runs a solo course (jog, sprint, zig-zag, close control, turn back, stop, cuts)
  and logs the touch / distance metrics per phase (with `-PKCloseCam`, `Dribble_cut_N` screenshots of outside cuts);
  add `-PKOldDribble` to measure the previous system. The autotest line splits touches into on-step, fallback, overrun,
  outside and inside. In a full AI match 63 % of touches land on steps (28 % before the ankle / outside-foot tuning).

## Movement skills

| Skill | How it works |
| --- | --- |
| Jog / sprint | 5.6 / 8.4 m/s; acceleration falls off near top speed; turning circle widens with speed |
| Plant & cut | A hard change of direction at pace bleeds speed before you turn |
| Touch dribbling | The ball is never glued: each touch pushes it ahead (small when jogging, big when sprinting); lose it if you outrun it |
| Close control | Hold the modifier with the ball: walking pace, tiny touches, ball steered to your line |
| Shield | Modifier with an opponent near: back into them, ball protected, harder to steal |
| Jockey | Modifier without the ball: face the ball, side-step, low stance |
| Knock-on | Kick the ball into space and sprint onto it (lifts it slightly) |
| Flick-up | Scoop the ball over a challenger and run onto it (AI uses it too with good control) |
| Slide tackle | C / X when defending: real slide with get-up; a successful slide can trip the carrier |

## Match readability

- **Movement is camera-relative:** right on the keyboard / stick is always right on the screen (the broadcast camera looks along +Y, so world +X is screen-left; the mini-map is drawn the same way).
- **Ball-taker arrow:** a team-coloured arrow floats over whoever has the ball or will reach it first (projected ball path vs each player's running speed).
- **Ball highlights:** the ball glows slightly; a white ring sits under it while it is in the air and a yellow ring marks where a lofted ball will land.
- **Shirt numbers:** every player has his number (1 = keeper) in a team-coloured tag above the head; the controlled player's tag is gold with P1, a booked player shows a yellow card next to it. N toggles them.
- **Player card (top right):** controlled player's number, role, overall, PAC / CTL / PAS / SHO / DEF / PHY and traits. Both teams' play styles show under the score.

## Physical play and the laws

| Rule | How it works |
| --- | --- |
| Body contact | players no longer block each other rigidly (they used to get wedged between two opponents); overlapping bodies are pushed apart, the stronger / faster player giving less ground |
| Shoulder challenge | running side by side with the carrier: Physical + Defending vs Physical (+ shield) decides who stumbles; the loser is knocked off the ball or bounced away. AI does it automatically, X / RB for you |
| Push | barging or running into the carrier from behind is usually a foul |
| Tackle fouls | slide tackles from behind or that miss the ball, and mistimed standing tackles, can be fouls (Defending rating lowers the chance; booked players are more careful) |
| Free kick | at the spot, opponents 9.15 m away, a 3-4 man wall inside 32 m; the AI shoots from inside ~29 m |
| Penalty | a foul in the box: best finisher takes it, everyone else outside the box, keeper on the line |
| Cards | yellow by severity, second yellow = red, denying an obvious goal-scoring chance = straight red; a sent-off player leaves the pitch (red marks next to the score) |
| Offside | in the opponents' half, ahead of the ball and the second-last defender when a team-mate passes, and then plays the ball (not from goal kicks, corners or throw-ins). The AI avoids offside passes but misjudges a few |
| Stalemate guard | a carrier who has lost touch of the ball for a second leaves it loose for both teams |

## Ball in the air

| Situation | What the ball does |
| --- | --- |
| Short pass (< 9 m) | rolls |
| Medium pass (9-18 m) | skids off the turf, small hops |
| Long / driven pass (> 18 m) | flies knee-to-waist high, then bounces |
| Defender in the passing lane | the pass is chipped over him |
| Lob, cross, switch of play | high arc with slight curl; landing ring shows where it drops |
| Clearance / goal kick | high and long toward the forwards |
| Shot | rises with power; poor finishers and volleys can fly over the bar |
| Sprint dribble touches / heavy first touch | ball bobbles or pops up off the shin |
| Bounces | grass restitution 0.62, the ball keeps bouncing before it rolls |

Wingers carry the ball down the line in the final third and cross; forwards attack the near and far post and a
midfielder arrives at the edge of the box, so crosses end in headers and volleys.

## Ratings, traits and play styles

| System | Effect |
| --- | --- |
| Pace | top speed |
| Control | dribble touch distance and frequency, heavy-touch risk on the first touch, harder to rob |
| Passing | pass direction / weight error |
| Shooting | shot error, power, shooting range for the AI |
| Defending | tackle / interception success |
| Physical | shielding strength |
| Traits | Technical, Finesse Shot (far-post curl), Power Shot, Tiki Taka, Rapid, Press Proven, Anticipate |
| Team styles | Balanced, Possession (patient, safe), Counter Attack (direct, through balls, deep block), Long Ball (lofted passes), High Press (high line, two pressers) |

The AI for non-selected players marks goal-side in its own half, offers passing angles around the ball carrier, times runs in behind the last defender (through balls are aimed at the run), carries the ball into open space and attacks the goal in the final third.

## Football animations (Mixamo soccer pack, retargeted to the UE mannequin)

All 54 clips from Downloads\Soccer Game Pack were imported and IK-retargeted onto the mannequin by
Tools/editor_scripts/import_soccer_pack.py (into Content/Characters/Mannequins/Anims/Soccer, prefix PK_).
PKAnimInstance blends them over locomotion as full-body actions, makes them in-place, and applies their body
travel to the capsule (slides, dives, trips). Each kick releases the ball on the clip's contact frame (when the striking foot
reaches the ball spot), the kicker turns to the target during the wind-up, the ball is eased onto the striking foot, and
play rate varies ±10 % so repeated kicks don't look identical.

Contact rules: each clip stores where its contact bone (foot / head / knee) is on the contact frame. During the wind-up
the ball is brought to exactly that point and leaves only on that frame; if the ball has run away it is an air kick
and the ball does not move. A pass or shot chosen while the ball is more than ~70 cm from the feet is held for up to
0.7 s while the player catches the ball up. Headers, thigh control and flick-ups also wait for their contact frame.
Dribble touches stay within 40 degrees of where the body faces, and off-ball players at walking pace turn to face
the ball (side-step / back-pedal with the 8-way locomotion).

| Action | Clip | When |
| --- | --- | --- |
| Pass (standing) | KickSoccerball_2 (right foot) / KickSoccerball (left foot) | foot chosen by pass direction |
| Volley | ScissorKick | dropping ball near goal |
| Running pass / running shot | StrikeFowardJog | passes and shots on the move |
| Shot / lob | SoccerPenaltyKick | standing shots, lobs, clearances |
| Header | HeaderSoccerball | dropping ball at head height |
| Thigh control | KneeingSoccerball | receiving a bouncing ball (45–110 cm) |
| Standing tackle | SoccerTackle (poke section) | E / B |
| Slide tackle | SoccerTackle_2 / SoccerTackle / SoccerTackle_3 (random variant) | C / X, AI from range |
| Trip → fallen → get up | SoccerTrip, FallenIdle, StandingUp | carrier taken out by a slide |
| Keeper dive L / R | GoalkeeperDivingSave / _2 | wide shots |
| Keeper catch chest / high / low | GoalkeeperCatch_2 / _3 / GoalkeeperCatch | by ball height |
| Keeper smother / gather | GoalkeeperBodyBlock / GoalkeeperScoop | low fast / slow balls |
| Keeper hold | GoalkeeperCatch_2 (frozen) | ball in hands until release |
| Keeper roll / throw / punt | GoalkeeperPass / OverhandThrow / DropKick | distribution |
| Keeper beaten | GoalkeeperMiss | conceding a goal |
| Keeper idle / side-shuffle | GoalkeeperIdle / GoalkeeperSidestep(_2) | keeper locomotion |
| Throw-in hold + throw | ThrowIn | throw-ins |

### Animation test tools

| Flag | What it does |
| --- | --- |
| `-PKAnimTest` | every action side-on in groups of six, labelled, screenshotted |
| `-PKClipSheet` | all 54 retargeted clips at 25 / 50 / 80 % of their length |
| `-PKClipSheet -PKClipFrames=PK_X@0.4,PK_X@0.6,...` | exact frames (red / yellow squares = right / left foot); Tools/frames.ps1 wraps it |
| `-PKScenario` | stages dives, catches, slide + trip, header, throw-in, keeper release, thigh control, keeper shuffle and captures bursts |
| `-PKAutoTest -PKCloseCam` | full AI match with close camera and event screenshots |
| `-PKAutoTest -PKCloseCam -PKContactShots` | slow-motion screenshot (PKContact_*) at the contact frame of 30 kicks / headers / touches |
| `-PKAutoTest -PKNoWatch` | baseline with off-ball ball-watching disabled (compare the facing numbers) |
| `-PKAutoTest -PKSandwich` | 12 times: a carrier wedged between two touching opponents; logs whether anyone is stuck (`-PKHardCollision` = old rigid capsules for comparison) |
| `-PKAutoTest -PKFoulTest -PKCloseCam` | stages a penalty, a free kick with a wall and a red card, with screenshots |

The autotest log line also reports kicks, air kicks, contact error, kick facing error and off-ball facing.

## Code map

| System | File |
| --- | --- |
| Constants (pitch, ball, speeds, kits) | `PKTypes.h` |
| Footballer: mesh, kits, stances, movement model, kick/tackle/dive | `PKFootballer.*` |
| Locomotion (8-way walk/jog, keeper idle/shuffle) + football action layer + procedural lean, crouch, head look | `PKAnimInstance.*` |
| Ball: fixed-step physics, curl, posts/net, held/placed states | `PKBall.*` |
| Match: 4-4-2 AI, dribbling, steals, pickups, restarts, clock | `PKGameMode.*` |
| Input + broadcast camera, menu camera, pause | `PKPlayerController.*` |
| Front end (Slate): landing, home, team setup, controls, pause | `PKMenu.*` |
| Clubs, squads, formations, position fit, auto pick | `PKTeamData.*` |
| Team sheet + save game | `PKGameInstance.*` |
| HUD | `PKHUD.*` |
| Stadium (construction script) | `PKPitch.*` |

Content: `Content/Characters/Mannequins` (UE template mannequin), `Content/PitchKings/Materials` (M_PK_Surface, M_PK_Grass, M_PK_Crowd), `Content/Maps/Stadium`.
`Tools/ue_mcp.py` drives the open editor over MCP (used to build the materials and the map).

## Known limits / next upgrades

- The soccer pack has no celebrations, ground first-touch trap or dribble-turn clips; locomotion is still the template walk/jog blend (Motion Matching from Epic's Game Animation Sample is the next locomotion upgrade).
- No substitutions, advantage rule, stamina, menus or Season mode yet; keepers are only booked (no substitute keeper).
- Crowd is a material on stepped tiers, not individual spectators.
