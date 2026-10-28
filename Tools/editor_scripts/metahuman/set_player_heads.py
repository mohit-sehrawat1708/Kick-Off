"""
Chooses which built heads the game uses, and their order: keeps only the DA_PK_HeadSet entries named in -PKKeep=A,B,C
(in that order). Heads are built by build_player_heads.py; this only edits the set.

  UnrealEditor-Cmd PitchKings.uproject -run=pythonscript -script=Tools/editor_scripts/metahuman/set_player_heads.py -PKKeep=Omari,Mateo
"""
import unreal

SET = "/Game/PitchKings/Data/DA_PK_HeadSet"
args = [a.split("=", 1)[1] for a in unreal.SystemLibrary.get_command_line().split() if a.startswith("-PKKeep=")]
if not args:
    raise SystemExit("[HeadSet] no -PKKeep= list")
keep = args[0].split(",")
hs = unreal.load_asset(SET)
heads = {str(h.get_editor_property("name")): h for h in hs.get_editor_property("heads")}
missing = [n for n in keep if n not in heads]
hs.set_editor_property("heads", [heads[n] for n in keep if n in heads])
unreal.EditorAssetLibrary.save_asset(SET)
unreal.log("[HeadSet] %d heads: %s (missing %s, dropped %s)" % (len(hs.get_editor_property("heads")), [n for n in keep if n in heads], missing,
                                                                 [n for n in heads if n not in keep]))
