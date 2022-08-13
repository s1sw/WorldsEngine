import os, shutil, json

project_dict = {}

with open("WorldsProject.json", "r") as f:
    project_dict = json.load(f)
    
print(f"Building {project_dict['projectName']}")

if os.path.exists("BuiltData"):
    shutil.rmtree("BuiltData")
    
shutil.copytree("Data", "BuiltData")

for copy_dir in project_dict["copyDirectories"]:
    if not os.path.exists("SourceData/" + copy_dir):
        print(f"Warning: Copy directory {copy_dir} doesn't exist.")
        continue
    shutil.copytree("SourceData/" + copy_dir, "BuiltData/" + copy_dir)

print("Build finished")
