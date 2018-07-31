#!/usr/bin/python3
# --------------------------------------
# Retrieves the sector of organizations
# including counts, ready for a plot.
# --------------------------------------

import csv
import argparse

# Parse input arguments.
parser = argparse.ArgumentParser(description='Sector plotting tool for Bloom filter results')
parser.add_argument('-r', action='store', dest='results_file', help='Input CSV containing aggregated results', required=True)
parser.add_argument('-m', action='store', dest='mapping_file', help='CSV mappings file containing entities and sectors', required=True)
parser.add_argument('-w', action='store', dest='output_file', help='Output CSV file containing sectors with numbers', required=True)
parser.add_argument('-v', action='store_true', dest='verbose', help='Verbose output')
results = parser.parse_args()

# Variables.
results_dict = {}
mapping_dict = {}
sector_dict = {}

# Open input and output files.
with open(results.results_file, 'r') as input_file, open(results.mapping_file, 'r') as mapping_file, open(results.output_file, 'w') as output_file:
	# Read results from input file into dictionary.
	reader = csv.reader(input_file)
	for row in reader:
		# Store the entity and occurrence count.
		results_dict[row[1]] = int(row[0])

	if results.verbose:
		print("Read " + str(len(results_dict)) + " from results file.")

	# Read in the mappings between entity and sector.
	mapping_reader = csv.DictReader(mapping_file, delimiter=',', quotechar='"')
	for row in mapping_reader:
		mapping_dict[row["Volledige naam klant"]] = row["DoelgroepCode"]

	if results.verbose:
		print("Read " + str(len(mapping_dict)) + " from mapping file.")

	# Sum the counters in the results dictionary per sector.
	for k, v in results_dict.items():
		try:
			sector = mapping_dict[k]
			if sector in sector_dict:
				sector_dict[sector] += v
			else:
				sector_dict[sector] = v
		except KeyError:
			print("Skipping " + k + ", no such key!")

	# Write the final contents to a CSV file.
	writer = csv.DictWriter(output_file, fieldnames=['sector', 'count'])
	writer.writeheader()
	for k, v in sector_dict.items():
		writer.writerow({'sector' : k, 'count' : v})

	if results.verbose:
		print("Wrote " + str(len(sector_dict)) + " entries to the output file.")
