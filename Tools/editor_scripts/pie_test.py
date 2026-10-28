"""Play-In-Editor smoke test (run with the full editor, not the commandlet):
UnrealEditor.exe PitchKings.uproject -ExecutePythonScript=Tools/editor_scripts/pie_test.py [-PKNoMenu]

Waits for the editor, starts PIE in the Stadium map, takes screenshots during play, stops PIE and quits the editor.
Everything it does is logged with a [PIETest] prefix.
"""
import unreal

STATE = {"t": 0.0, "step": 0}
SHOTS = [(18.0, "PIE_01"), (26.0, "PIE_02"), (34.0, "PIE_03")]
lev = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def log(msg):
    unreal.log("[PIETest] " + msg)


def tick(dt):
    STATE["t"] += dt
    t = STATE["t"]
    step = STATE["step"]
    if step == 0 and t > 8.0:
        log("starting PIE")
        lev.editor_request_begin_play()
        STATE["step"] = 1
        STATE["t0"] = t
    elif step == 1:
        world = ues.get_game_world()
        if world:
            log("PIE world is up: " + world.get_name())
            STATE["step"] = 2
        elif t - STATE["t0"] > 60.0:
            log("PIE did not start")
            STATE["step"] = 9
    elif step >= 2 and step - 2 < len(SHOTS):
        at, name = SHOTS[step - 2]
        if t - STATE["t0"] > at:
            world = ues.get_game_world()
            if world:
                unreal.SystemLibrary.execute_console_command(world, "HighResShot 1280x720 filename=" + name)
                log("screenshot " + name)
            STATE["step"] = step + 1
    elif step - 2 >= len(SHOTS) and step < 9:
        if t - STATE["t0"] > SHOTS[-1][0] + 4.0:
            log("ending PIE, still in play: %s" % lev.is_in_play_in_editor())
            lev.editor_request_end_play()
            STATE["step"] = 9
            STATE["t9"] = t
    elif step == 9 and t - STATE.get("t9", t) > 5.0:
        log("done, quitting editor")
        STATE["step"] = 10
        unreal.SystemLibrary.quit_editor()


unreal.register_slate_post_tick_callback(tick)
log("registered")
