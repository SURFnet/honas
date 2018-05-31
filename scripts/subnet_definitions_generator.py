#!/usr/bin/python3
# ---------------------------------------------------
# Honas Subnet Defintions Generator
# Author: Gijs Rijnders, SURFnet, 2018
# ---------------------------------------------------
# Converts entity/subnet mapping information from
# a CSV file into the required JSON mappings.
# ---------------------------------------------------

import sys
import argparse
import csv
import ipaddress
import json

# Parse input arguments.
parser = argparse.ArgumentParser(description='Honas subnet definitions file generator')
parser.add_argument('-r', action='store', dest='input_file', help='Input file containing entity-prefix mappings', required=True)
parser.add_argument('-w', action='store', dest='output_file', help='Output file receiving the JSON mappings')
parser.add_argument('-v', action='store_true', dest='verbose', help='Verbose output')
results = parser.parse_args()
outfilename = results.output_file

# Storing the mappings for generation.
input_entities = {}
input_mappings = {}
prefix_count = 0

# Open the input file and read in the mappings.
try:
	with open(results.input_file, 'r', encoding='UTF-8', newline='') as infile:
		reader = csv.DictReader(infile)

		# Read out all rows from the input mapping file.
		for row in reader:
			prefix = row['Prefix']
			entity = row['Volledige naam klant']
			input_entities[entity] = 0
			input_mappings[prefix] = entity
			prefix_count += 1

except FileNotFoundError:
	print("Failed to open input file " + results.input_file + "!")
	exit()

# Some statistics about the input.
if results.verbose:
	print("Read " + str(len(input_mappings)) + " prefix-entity mappings from " + results.input_file)
	print("The input file contains " + str(prefix_count) + " prefixes and " + str(len(input_entities)) + " entities")

# Create basic JSON structure for the subnet activity data.
json_data = { "subnet_activity" : [] }

# Prepare the data in the correct format.
for e, v in input_entities.items():
	# Find all prefixes associated with the prefix.
	associated_prefixes = []
	for p, pe in input_mappings.items():
		if e == pe:
			ipaddr = ipaddress.ip_network(p)
			associated_prefixes.append({ str(ipaddr.network_address) : ipaddr.prefixlen })

	# Add entity to the JSON data.
	json_data["subnet_activity"].append({ "entity" : e, "prefixes" : associated_prefixes })

# Generate JSON from input.
json_output = json.dumps(json_data)
if outfilename is None:
	print(json_output)
else:
	# Open the output file for JSON generation.
	with open(outfilename, 'w') as outfile:
		print(json_output, file=outfile)
		print("Wrote JSON output to " + outfilename)

