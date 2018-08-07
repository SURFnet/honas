#!/usr/bin/python3
# -------------------------
# Makes an overview of the
# hits we got from the ex-
# periments using the Pa-
# reto principle.
# -------------------------

import sys
import csv

targetfile = ""
if len(sys.argv) > 1:
	targetfile = sys.argv[1]
else:
	print("Please enter target file containing CSV data as first argument!")
	exit()

# Import CSV data into dictionary.
datadict = {}
total = 0
with open(targetfile, 'r') as inpfile:
	reader = csv.reader(inpfile)
	for row in reader:
		datadict[row[1]] = int(row[0])
		total += int(row[0])

# Take out records until we reach 80% coverage.
percentage_eighty = int(total * 0.8)
print("Total is: " + str(total) + ", 80% of total is: " + str(percentage_eighty))
counter = 0
sorteddata = [(k, datadict[k]) for k in sorted(datadict, key=datadict.get, reverse=True)]
for k, v in sorteddata:
	counter += v
	print(k + "," + str(v))
	if counter >= percentage_eighty:
		break
