import glob

src_file_list = glob.glob("*.a")

print("physx_deps = [")

for f in src_file_list:
    f = f.replace(".a", "")
    f = f.replace("lib", "")
    print(f"    cc.find_library('{f}', dirs: meson.project_source_root() + '/External/Lib/linux64dbg'),")

print("]")
