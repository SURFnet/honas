#!/usr/bin/python
# ---------------------------------------------
# Query tool to find NDN IoCs in Honas Bloom
# filters.
# ---------------------------------------------

import sys
import csv
import json
from hashlib import sha256
import os
import glob
import ntpath

HONAS_STATE_DIR = "/data"
HONAS_BIN_PATH = "/home/gijs/honas/build/honas-search"
ENTITY_FILE = "entities_out.csv"

# Check if we have an input domain names file.
ndnfile = ""
if len(sys.argv) > 1:
	ndnfile = sys.argv[1]
else:
	print("Please enter a domain name file as first argument!")
	exit()

print("Processing DNS queries from input domain name file.")

# Prepare Honas search query.
searchdata = { "groups" : [] }
searchdata["groups"].append({ "id" : 1 })
searchdata["groups"][0]["hostnames"] = {}

# Prepare a dictionary containing all unique domain names in the domain names file.
# For convinience we store the number of times it occurred as value.
srcdomains = {}

# Read the domain names file.
with open(ndnfile, "r") as inpfile:
	reader = csv.reader(inpfile)
	for row in reader:
		dnsname = row[0].lower()

		# Store the domain name in the dictionary for comparison later.
		# First canonicalize the domain name! We need it to be in lower case.
		if dnsname in srcdomains:
			srcdomains[dnsname] += 1
		else:
			srcdomains[dnsname] = 1

# Print statistics.
print("Processed " + str(len(srcdomains)) + " Booter domain names.")

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

# Process the domain names and generate a Honas query file.
q_count = 0
for k, v in srcdomains.items():
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

# Execute the query to Honas.
for filename in glob.iglob(HONAS_STATE_DIR + "/**/2018-??-??.hs"):
	# Execute query for this state file.
	searchresult = os.popen(HONAS_BIN_PATH + " " + filename + " < " + tmpfilename).read()

	# Parse the Honas search result and test whether false positives occurred.
	jsonresult = json.loads(searchresult)

	# Check if there are results.
	if len(jsonresult["groups"]) <= 0:
		print("No results from search job in " + filename + "!")
		continue
	else:
		print("Found search results in " + filename + "!")

	# Write the search results to a JSON file.
	with open(ntpath.basename(filename) + ".json", 'w') as outresultfile:
		outresultfile.write(json.dumps(jsonresult, indent=4))
