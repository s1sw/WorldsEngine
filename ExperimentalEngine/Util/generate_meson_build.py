import glob, os, pathlib

script_dir = pathlib.Path(os.path.dirname(os.path.abspath(__file__)))
curr_dir = pathlib.Path.cwd()

source_exts = ['*.cpp', '*.hpp', '*.c', '*.h', '*.cc']

files = []

for ext in source_exts:
	files.extend(curr_dir.glob(ext))

with open("meson.build", "w") as out_file:
	out_file.write("worlds_sources += [\n")
	for f in files[:-1]:
		out_file.write("  '" + str((curr_dir / f).relative_to(script_dir)) + "',\n")
	out_file.write("  '" + str((curr_dir / files[-1]).relative_to(script_dir)) + "'\n]")
	