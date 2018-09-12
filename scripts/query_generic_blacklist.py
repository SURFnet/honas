#!/usr/bin/python
# ---------------------------------------------
# Query tool to create Honas queries from a
# generic domain name blacklist.
# ---------------------------------------------

import csv
import json
from hashlib import sha256
import os
import ntpath
import argparse

HONAS_STATE_DIR = "/data"
HONAS_BIN_PATH = "/home/gijs/honas/build/honas-search"

# Parse input arguments.
parser = argparse.ArgumentParser(description='Query tool for generic domain blacklists')
parser.add_argument('-b', action='store', dest='blacklist_file', help='Input blacklist file containing domain names', required=True)
parser.add_argument('-s', action='store', dest='state_file', help='Honas state file to query', required=True)
parser.add_argument('-e', action='store', dest='entity_file', help='Input file containing all stored entities', required=True)
parser.add_argument('-v', action='store_true', dest='verbose', help='Verbose output')
results = parser.parse_args()

print("Processing domain names from input blacklist file.")

# Prepare Honas search query.
searchdata = { "groups" : [] }
searchdata["groups"].append({ "id" : 1 })
searchdata["groups"][0]["hostnames"] = {}

# Prepare a dictionary containing all unique Booter domain names.
# For convinience we store the number of times it occurred as value.
blacklistdomains = {}

# Read the spamfilter file.
with open(results.blacklist_file, "r") as inpfile:
	reader = csv.reader(inpfile)
	for row in reader:
		domainname = row[0].strip()

		# Store the domain name in the dictionary for comparison later.
		# This way we remove the duplicates, which reduces lookup time.
		if domainname in blacklistdomains:
			blacklistdomains[domainname] += 1
		else:
			blacklistdomains[domainname] = 1

# Print statistics.
print("Processed " + str(len(blacklistdomains)) + " Booter domain names.")

# Read all entities in a dictionary if possible.
entities = {}
with open(results.entity_file, 'r') as entity_file:
	entity_reader = csv.reader(entity_file)
	for row in entity_reader:
		ent_str = row[0]
		if ent_str in entities:
			entities[ent_str] += 1
		else:
			entities[ent_str] = 1

if results.verbose:
	print("Processed " + str(len(entities)) + " from input entities file.")

# Insert DNS queries in Honas search query.
q_count = 0
for k, v in blacklistdomains.items():
	# Create Honas JSON query for this domain name.
	searchdata["groups"][0]["hostnames"][k] = sha256(k).hexdigest()
	q_count += 1

	# Iterate through all entities in the entities file, and generate
	# a query for every combination of domain name and entity.
	for k1, v1 in entities.items():
		compound = str(k1) + '@' + k
		searchdata["groups"][0]["hostnames"][compound] = sha256(compound).hexdigest()
		q_count += 1

	# Also generate an UNKNOWN entity query to identify unmapped requests.
	unk = "UNKNOWN@" + k
	searchdata["groups"][0]["hostnames"][unk] = sha256(unk).hexdigest()
	q_count += 1

# Print statistics.
print("Generated " + str(q_count) + " queries.")

# Write the Honas JSON query to a temporary file.
tmpfilename = "honas_tmp_query.json"
with open(tmpfilename, 'w') as tmpfile:
	tmpfile.write(json.dumps(searchdata, indent=4, ensure_ascii=False))

# Execute query for the provided state file.
searchresult = os.popen(HONAS_BIN_PATH + " " + results.state_file + " < " + tmpfilename).read()

# Parse the Honas search result and test whether false positives occurred.
jsonresult = json.loads(searchresult)

# Check if there are results.
if len(jsonresult["groups"]) <= 0:
	print("No results from search job in " + results.state_file + "!")
else:
	print("Found search results in " + results.state_file + "!")

	# Write the search results to a JSON file.
	with open(ntpath.basename(results.state_file) + ".json", 'w') as outresultfile:
		outresultfile.write(json.dumps(jsonresult, indent=4))

