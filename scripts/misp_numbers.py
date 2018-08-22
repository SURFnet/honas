#!/usr/bin/python

import sys
import json

targetfile = ""
if len(sys.argv) > 1:
	targetfile = sys.argv[1]
else:
	print("Please enter target file as first argument!")
	exit()

# Counting variables.
ioccount = 0
iocswithna = 0

# Load and parse input file.
with open(targetfile, 'r') as mispfile:
	iocs = json.loads(mispfile.read())

	# Parse IoCs to see if it contains Network Activity attributes.
	response = iocs["response"]
	for event in response:
		ioccount += 1
		ev = event["Event"]

		# Check its attributes.
		attributes = ev["Attribute"]
		for att in attributes:
			cat = att["type"]
			if cat.lower() == "domain":
				iocswithna += 1
				break

# Print statistics.
print("Processed a total of " + str(ioccount) + " Indicators of Compromise.")
print(str(iocswithna) + " of those contained domain attributes.")
print("[" + str(iocswithna) + " / " + str(ioccount) + "] IoCs are applicable for threat detection (" + str((float(iocswithna) / float(ioccount)) * 100.0) + " %).")
