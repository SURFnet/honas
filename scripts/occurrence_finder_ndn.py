#!/usr/bin/python3
# -------------------------------------
# Tool for finding domain names that
# occur in multiple files for NDN.
# -------------------------------------

import sys
import json
import glob

# Get daily directory as first argument.
targetdir = ""
if len(sys.argv) > 1:
	targetdir = sys.argv[1]
else:
	print("Please enter the target directory as first argument!")
	exit()

content_dict = {}
files_count = 0

# Labels to exclude.
exclusions = [ "netSURF", "Nuffic" ]

# Find all daily JSON files.
for fn in glob.iglob(targetdir + "/2018-07-??.hs.json"):
	files_count += 1
	# Open file.
	with open(fn, 'r') as json_file:
		# Parse the JSON.
		contents = json.loads(json_file.read())

		# Check hostnames.
		for hostname in contents['groups'][0]['hostnames']:
			# Check whether we should exclude this label.
			atsign = hostname.find('@')
			if atsign != -1:
				label = hostname[0 : atsign]
				if label in exclusions:
					continue

			# Store the occurrence counter for this domain name.
			if hostname in content_dict:
				content_dict[hostname] += 1
			else:
				content_dict[hostname] = 1

print("Parsed " + str(files_count) + " files!")

# Count occurrences for hostnames, and print the highest 10.
sortedcontents = [(k, content_dict[k]) for k in sorted(content_dict, key=content_dict.get, reverse=True)]
index = 0
print("Printing Top-30 occurences...")
for k, v in sortedcontents:
	if index < 30:
		print(k + ", " + str(v))
		index += 1
	else:
		break
