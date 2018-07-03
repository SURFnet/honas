#!/usr/bin/python
# ---------------------------------------------
# Query tool to create Honas queries from a
# list of Booter record queries.
# ---------------------------------------------

import sys
import csv
import json
from hashlib import sha256
import os

HONAS_STATE_DIR = "/var/spool/honas"
HONAS_BIN_PATH = "/home/gijs/honas/build/honas-search"
ENTITY_FILE = "entities_out.csv"

# Check if we have an input file containing Booter domain names.
booterfile = ""
if len(sys.argv) > 1:
	booterfile = sys.argv[1]
else:
	print("Please enter a Booter file as first argument!")
	exit()

print("Processing domain names from input Booter file.")

# Prepare Honas search query.
searchdata = { "groups" : [] }
searchdata["groups"].append({ "id" : 1 })
searchdata["groups"][0]["hostnames"] = {}

# Prepare a dictionary containing all unique Booter domain names.
# For convinience we store the number of times it occurred as value.
booters = {}

# Read the spamfilter file.
with open(booterfile, "r") as inpfile:
	reader = csv.reader(inpfile)
	for row in reader:
		booter = row[0].strip()

		# Store the domain name in the dictionary for comparison later.
		# This way we remove the duplicates, which reduces lookup time.
		if booter in booters:
			booters[booter] += 1
		else:
			booters[booter] = 1

# Print statistics.
print("Processed " + str(len(booters)) + " Booter domain names.")

# Read all entities in a dictionary if possible.
entities = {}
with open(ENTITY_FILE, 'r') as entity_file:
	entity_reader = csv.reader(entity_file)
	for row in entity_reader:
		ent_str = row[0]
		if ent_str in entities:
			entities[ent_str] += 1
		else:
			entities[ent_str] = 1

# Insert Booter queries in Honas search query.
q_count = 0
for k, v in booters.items():
	# Create Honas JSON query for this domain name.
	searchdata["groups"][0]["hostnames"][k] = sha256(k).hexdigest()
	q_count += 1

	# Iterate through all entities in the entities file, and generate
	# a query for every combination of domain name and entity.
	for k1, v1 in entities.items():
		compound = str(k1) + '@' + k
		searchdata["groups"][0]["hostnames"][compound] = sha256(compound).hexdigest()
		q_count += 1

# Print statistics.
print("Generated " + str(q_count) + " queries.")

# Write the Honas JSON query to a temporary file.
tmpfilename = "honas_tmp_query.json"
with open(tmpfilename, 'w') as tmpfile:
	tmpfile.write(json.dumps(searchdata, indent=4))

# Execute the query to Honas.
for filename in os.listdir(HONAS_STATE_DIR):
	if filename.endswith('.hs'):
		# Execute query for this state file.
		searchresult = os.popen(HONAS_BIN_PATH + " " + HONAS_STATE_DIR + "/" + filename + " < " + tmpfilename).read()

		# Parse the Honas search result and test whether false positives occurred.
		jsonresult = json.loads(searchresult)

		# Check if there are results.
		if len(jsonresult["groups"]) <= 0:
			print("No results from search job in " + filename + "!")
			continue
		else:
			print("Found search results in " + filename + "!")

		# Write the search results to a JSON file.
		with open(filename + ".json", 'w') as outresultfile:
			outresultfile.write(json.dumps(jsonresult, indent=4))
