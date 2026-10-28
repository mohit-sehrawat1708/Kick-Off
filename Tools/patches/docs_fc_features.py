# README + HOW_TO_PLAY for the FC 24-style feature layers.
import os, sys
ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
README = os.path.join(ROOT, "README.md")
GUIDE = sys.argv[1] if len(sys.argv) > 1 else None

s = open(README, encoding="utf-8").read()
def rep(a, b):
    global s
    if b in s:
        return
    if a not in s:
        sys.exit("README: not found: " + a[:60])
    s = s.replace(a, b, 1)

rep("| Knock-on (push ball into space) | F | RB / R3 |",
"""| Effort touch: knock the ball any way (tap short, hold long) | F + direction | R3, or RB + right stick flick |
| Controlled sprint (with ball): quicker than a jog, a touch every step | hold R | hold RB |""")
rep("| Pass to teammate in stick direction | J or Space | A |",
"""| Pass to teammate in stick direction | J or Space | A |
| Precision pass: to the exact spot aimed at (hold for power) | R + pass / through / lob | RB + A / Y / X |
| Swerve pass: bends round the defender in the lane | Ctrl + pass / through / lob | LT + A / Y / X |
| One-two (pass and run; your next pass returns it to the runner) | Z + pass | LB + A |
| Send a team-mate on a run in behind | tap Z | tap LB |
| Header / volley on demand (ball in the air coming to you) | pass or shoot | A or B |""")
rep("| Shoot (hold to charge, release) | K | B |",
"""| Shoot (hold to charge, release) | K | B |
| Shot technique (held at release): finesse / chip / power / low driven | R / Z / R+Z / Ctrl | RB / LB / RB+LB / LT |
| Timed finishing: press shoot again just before the strike | K again | B again |
| Fake shot (while charging a shot) | pass | A |""")
rep("| Shoulder barge (no ball, next to the carrier) | X | RB |",
"""| Shoulder barge (no ball, next to the carrier) | X | A |
| Teammate contain: the nearest team-mate presses the carrier | hold R (no ball) | hold RB |
| Keeper rush: your keeper charges the carrier | hold L (no ball) | hold Y |
| Skill moves (relative to facing): forward rainbow / nutmeg, diagonal step-over, sideways ball roll, back roulette | B + direction | right stick flick |""")
rep("| Team play style | 1 Balanced · 2 Possession · 3 Counter · 4 Long Ball · 5 High Press | D-pad left / right |",
"""| Team play style | 1 Balanced · 2 Possession · 3 Counter · 4 Long Ball · 5 High Press | D-pad left / right |
| Game plan mentality (Ultra defensive .. Ultra attacking) | 9 / 0 | LT + D-pad left / right |
| Substitutions | pause menu, SUBSTITUTIONS | Start, SUBSTITUTIONS |
| Skip goal replay | pass / shoot | A / B |""")
rep("## Dribbling (Active-Touch style)", """## FC 24-style features (added layer by layer)

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

## Dribbling (Active-Touch style)""")
open(README, "w", encoding="utf-8").write(s)

if GUIDE:
    g = open(GUIDE, encoding="utf-8").read()
    a = g.index("  Default keys (change them in SETTINGS):")
    b = g.index("SAVED DATA")
    g = g[:a] + """  Default keys (change them in SETTINGS):
  KEYBOARD                                     GAMEPAD
  Move ................... WASD / arrows       left stick
  Sprint ................. Left Shift          RT
  Controlled sprint ...... hold R              hold RB      (with the ball)
  Hold ................... Left Ctrl           LT           (jockey / shield / close control)
  Pass / Through / Lob ... J or Space / L / I  A / Y / X
    + R = precision pass (hold for power, release)   + Ctrl = swerve   + Z = one-two
  Shoot (hold, release) .. K                   B
    held at release: R finesse, Z chip, R+Z power shot, Ctrl low driven
    press shoot again just before the strike = timed finishing;  pass while charging = fake shot
  Effort touch ........... F + direction       R3, or RB + right stick
  Skill moves ............ B + direction       right stick flick
    forward: rainbow (nutmeg if a defender is tight)  diagonal: step-over  sideways: ball roll  back: roulette
  Send a run ............. tap Z               tap LB
  Tackle / Slide ......... E / C               B / X
  Switch player .......... Q                   LB
  Shoulder barge ......... X                   A
  Teammate press ......... hold R (no ball)    hold RB
  Keeper rush ............ hold L (no ball)    hold Y
  Header / volley ........ pass or shoot when the ball is in the air coming to you
  Skills (classic) ....... G drag-back, T feint, V flick-up    D-down, D-up, L3
  Team style / game plan . 1-5 / 9 and 0       D-pad left/right / LT + D-pad
  Pause (substitutions) .. Esc / P             Start

MATCH
  Two halves (teams change ends), stamina (bar under your player: sprinting tires you out, rest to recover),
  substitutions in the pause menu (5), advantage, goal celebration and replay (pass / shoot skips it).

""" + g[b:]
    g = g.replace("prototype build 0.5", "prototype build 0.6")
    open(GUIDE, "w", encoding="utf-8").write(g)
print("docs ok")
