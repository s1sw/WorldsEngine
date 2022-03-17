import glob, os, pathlib

script_dir = pathlib.Path(os.path.dirname(os.path.abspath(__file__)))
curr_dir = pathlib.Path.cwd()

subdirs = [
    'AssetCompilation',
    'Audio',
    'ComponentMeta',
    'Core',
    'Editor',
    'ImGui',
    'Input',
    'IO',
    # 'Libs',
    'Navigation',
    'PathTracer',
    'Physics',
    'Render',
    'Scripting',
    'Serialization',
    'Util',
    'VR'
]

source_exts = ['**/*.cpp', '**/*.hpp', '**/*.c', '**/*.h', '**/*.cc']

for subdir in subdirs:
    files = []
    subdir_path = pathlib.Path(pathlib.Path.cwd() / subdir)
    for ext in source_exts:
        files.extend(subdir_path.glob(ext))

    if len(files) == 0:
        print(f"Warning: subdirectory {subdir} does not contain any source files. Source list generation for this directory will be skipped.")
        continue

    with open(subdir_path / "meson.build", "w") as out_file:
        out_file.write("worlds_sources += [\n")
        for f in files[:-1]:
            out_file.write("  '" + str(f.relative_to(script_dir)).replace("\\", "/") + "',\n")
        out_file.write("  '" + str(files[-1].relative_to(script_dir)).replace("\\", "/") + "'\n]")

