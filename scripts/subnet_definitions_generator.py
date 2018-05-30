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
results = parser.parse_args()

# Storing the mappings for generation.
input_entities = {}
input_mappings = {}
prefix_count = 0

# Open the input file and read in the mappings.
try:
	with open(results.input_file, 'r') as infile:
		reader = csv.reader(infile, delimiter=",")

		# Read out all rows from the input mapping file.
		for row in reader:
			prefix = ipaddress.ip_network(row[2])
			entity = row[4]
			input_entities[entity] = 0
			input_mappings[prefix] = entity
			prefix_count += 1

except FileNotFoundError:
	print("Failed to open input file " + results.input_file + "!")
	exit()

# Some statistics about the input.
print("Read " + str(len(input_mappings)) + " prefix-entity mappings from " + results.input_file)
print("The input file contains " + str(prefix_count) + " prefixes and " + str(len(input_entities)) + " entities")

# Generate JSON from input.
print(json.dumps(input_mappings))

# Open the output file for JSON generation.
#with open(results.output_file, 'w') as outfile:

