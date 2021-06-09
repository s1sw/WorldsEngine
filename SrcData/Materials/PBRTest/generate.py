import os

base_str = ""

with open("base.json", "r") as b:
	base_str = b.read()

i = 0
for met in range(0, 2):
	for rgh in range(0, 11):
		mat_str = base_str.replace("%MET%", str(float(met))).replace("%RGH%", str(rgh / 10))
		with open("test" + str(i) + ".json", "w") as f:
			f.write(mat_str)
		i += 1

os.system("pause")