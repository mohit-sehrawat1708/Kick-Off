"""Broadcast lighting for /Game/Maps/Stadium: sun, sky light, fog and the unbound post-process volume. Run headless:
UnrealEditor-Cmd.exe PitchKings.uproject -run=pythonscript -script=Tools/editor_scripts/set_stadium_lighting.py

The scene is physically lit (sun 80,000 lux): emissive materials are authored in nits.
"""
import unreal

MAP = "/Game/Maps/Stadium"
unreal.EditorLoadingAndSavingUtils.load_map(MAP)


def setp(obj, name, value):
    try:
        obj.set_editor_property(name, value)
    except Exception as e:  # property names drift between engine versions: report and carry on
        unreal.log_warning("[Lighting] %s.%s: %s" % (obj.get_name(), name, e))


for a in unreal.EditorLevelLibrary.get_all_level_actors():
    c = a.get_class().get_name()
    if c == "DirectionalLight":
        # Late-afternoon sun from behind the main camera's left shoulder: a long, readable shadow edge on the pitch.
        a.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=-30.0, yaw=50.0), False)
        l = a.light_component
        setp(l, "intensity", 80000.0)
        setp(l, "use_temperature", True)
        setp(l, "temperature", 5100.0)  # less yellow wash in the sun
        setp(l, "light_source_angle", 4.0)  # soft penumbra on the roof shadow (2.4 still cut the pitch with a razor edge)
        setp(l, "contact_shadow_length", 0.02)  # contact shadows under boots and the ball
        setp(l, "shadow_slope_bias", 0.5)  # engine defaults
        setp(l, "shadow_bias", 0.5)
        setp(l, "atmosphere_sun_light", True)
        unreal.log("[Lighting] sun set")
    elif c == "SkyLight":
        l = a.light_component
        setp(l, "intensity", 2.6)  # lifts the shaded half of the pitch: bounce in the shade (2.0 left it flat dark green)
        setp(l, "real_time_capture", True)
        unreal.log("[Lighting] sky light set")
    elif c == "ExponentialHeightFog":
        f = a.component
        setp(f, "fog_density", 0.2)  # aerial haze: the city recedes into atmosphere
        setp(f, "start_distance", 12000.0)       # the bowl stays clear; the city fades into haze
        setp(f, "fog_height_falloff", 0.18)
        unreal.log("[Lighting] fog set")
    elif c == "PostProcessVolume":
        s = a.settings
        # Exposure: narrow histogram range so cuts between the sunny pitch and the shaded stands do not pump.
        for name, val in (("override_auto_exposure_min_brightness", True), ("auto_exposure_min_brightness", 12.6),
                          ("override_auto_exposure_max_brightness", True), ("auto_exposure_max_brightness", 13.6),
                          ("override_auto_exposure_speed_up", True), ("auto_exposure_speed_up", 2.0),
                          ("override_auto_exposure_speed_down", True), ("auto_exposure_speed_down", 1.0),
                          # Lift the stands in the roof shadow a little (local exposure), as broadcast cameras do.
                          ("override_local_exposure_shadow_contrast_scale", True), ("local_exposure_shadow_contrast_scale", 0.62),
                          ("override_local_exposure_highlight_contrast_scale", True), ("local_exposure_highlight_contrast_scale", 0.85),
                          ("override_bloom_intensity", True), ("bloom_intensity", 0.7),
                          # no motion blur: it smeared players and shirt numbers on the match camera and ghosted restarts
                          ("override_motion_blur_amount", True), ("motion_blur_amount", 0.0),
                          ("override_bloom_threshold", True), ("bloom_threshold", 1.5),
                          ("override_vignette_intensity", True), ("vignette_intensity", 0.28),
                          ("override_white_temp", True), ("white_temp", 6100.0),  # a touch warmer grade
                          ("override_scene_fringe_intensity", True), ("scene_fringe_intensity", 0.0),
                          ("override_ambient_occlusion_intensity", True), ("ambient_occlusion_intensity", 0.8),
                          ("override_color_saturation", True), ("color_saturation", unreal.Vector4(1.04, 1.04, 1.04, 1.0)),
                          ("override_color_contrast", True), ("color_contrast", unreal.Vector4(1.06, 1.06, 1.06, 1.0)),
                          ("override_white_temp", True), ("white_temp", 6300.0)):
            setp(s, name, val)
        a.set_editor_property("settings", s)
        a.set_editor_property("unbound", True)
        unreal.log("[Lighting] post process set")

unreal.EditorLoadingAndSavingUtils.save_current_level()
unreal.log("[Lighting] saved " + MAP)
