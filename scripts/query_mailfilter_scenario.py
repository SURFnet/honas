#!/usr/bin/python
# ---------------------------------------------
# Query tool to create Honas queries from a
# list of spamfilter PTR record queries.
# ---------------------------------------------

import sys
import csv
import json
from hashlib import sha256
import os
import dns.resolver

HONAS_STATE_DIR = "/var/spool/honas"
HONAS_BIN_PATH = "/home/gijs/honas/build/honas-search"

# Check if we have an input file containing spamfilter records.
spamfilterfile = ""
if len(sys.argv) > 1:
	spamfilterfile = sys.argv[1]
else:
	print("Please enter a spamfilter file as first argument!")
	exit()

print("Processing PTR queries from input spamfilter file.")

# Prepare Honas search query.
searchdata = { "groups" : [] }
searchdata["groups"].append({ "id" : 1 })
searchdata["groups"][0]["hostnames"] = {}

# Prepare a dictionary containing all unique IP-addresses in the spamfilter file.
# For convinience we store the number of times it occurred as value.
srcips = {}

# Read the spamfilter file.
with open(spamfilterfile, "r") as inpfile:
	reader = csv.reader(inpfile, delimiter=' ')
	for row in reader:
		ipaddr = row[1].strip()

		# Store the domain name in the dictionary for comparison later.
		# This way we remove the duplicates, which reduces lookup time.
		if ipaddr in srcips:
			srcips[ipaddr] += 1
		else:
			srcips[ipaddr] = 1

# Perform reverse lookups for the domain names.
for k, v in srcips.items():
	try:
		# Create the reverse DNS lookup for the IP address.
		n = str(dns.reversename.from_address(k)).rstrip('.')

		# Create Honas JSON query for this domain name.
		searchdata["groups"][0]["hostnames"][n] = sha256(n).hexdigest()

	except dns.exception.SyntaxError:
		print("Failed to parse " + k + "! Skipping.")

# Print statistics.
print("Processed " + str(len(srcips)) + " IP addresses.")

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
