#!/usr/bin/python3
# ----------------------------------
# Converts ground truth experiment
# results to CSV for documentation
# ----------------------------------

import csv
import sys

targetfile = ""
if len(sys.argv) > 1:
	targetfile = sys.argv[1]
else:
	print("Please enter the target file as first argument!")
	exit()

# Open target file.
with open(targetfile, 'r') as input_file, open(targetfile + '.csv', 'w') as csv_outfile:
	# Open CSV output.
	writer = csv.writer(csv_outfile)
	writer.writerow(["day", "inside", "total"])

	# Read from input file.
	while True:
		line1 = input_file.readline()
		line2 = input_file.readline()
#		line3 = input_file.readline()
		if not line2: break  # EOF

		# Parse lines read.
		#Statistics for 2018-07-01.hs.json:
		#[1 / 1] from the ground truth were also in the search results!
		date = line1.split(' ')[2].replace(".hs.json:\n", "")

		# Split results on first results line.
		comparison = line2[1 : line2.find(']')].split('/')
		inside = comparison[0].replace(' ', '')
		total = comparison[1].replace(' ', '')

		# Split results on second results line.
#		comparison = line3[1 : line2.find(']')].split('/')
#		notinside = comparison[0].replace(' ', '')

		# Write output to CSV.
		writer.writerow([ date, inside, total ])
