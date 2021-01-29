#!/usr/bin/python
import glob

src_file_list = glob.glob("*.cpp") + glob.glob("*.h") + glob.glob('*.c') + glob.glob('*.cc')

source_str = "sources = [\n"

for f in src_file_list:
    source_str += f"    '{f}',\n"

source_str += "]\n"

template_contents = ""

with open("meson.build.template", "r") as templ_file:
    template_contents = templ_file.read()


with open("meson.build", "w") as out_file:
    out_file.write(template_contents.replace("%SOURCES_ARRAY%", source_str))
