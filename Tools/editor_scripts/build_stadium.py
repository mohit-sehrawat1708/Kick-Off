"""Re-imports the generated stadium meshes and rebuilds / re-assigns their materials (one headless run):
python Tools/art/gen_stadium.py
UnrealEditor-Cmd.exe PitchKings.uproject -run=pythonscript -script=Tools/editor_scripts/build_stadium.py
"""
import os
import runpy

HERE = os.path.dirname(os.path.abspath(__file__))
runpy.run_path(os.path.join(HERE, "import_stadium.py"))
runpy.run_path(os.path.join(HERE, "make_stadium_materials.py"))
