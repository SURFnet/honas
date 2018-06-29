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

HONAS_STATE_DIR = "/var/spool/honas"
HONAS_BIN_PATH = "/home/gijs/honas/build/honas-search"

# Check if we have an input PCAP file.
ndnfile = ""
if len(sys.argv) > 1:
	ndnfile = sys.argv[1]
else:
	print("Please enter a domain name blacklist file as first argument!")
	exit()

print("Processing DNS queries from input Booter blacklist file.")

# Prepare Honas search query.
searchdata = { "groups" : [] }
searchdata["groups"].append({ "id" : 1 })
searchdata["groups"][0]["hostnames"] = {}

# Prepare a dictionary containing all unique domain names in the PCAP file.
# For convinience we store the number of times it occurred as value.
srcdomains = {}

# Read the Booter blacklist file.
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

		# Create Honas JSON query for this domain name.
		searchdata["groups"][0]["hostnames"][dnsname] = sha256(dnsname).hexdigest()

# Print statistics.
print("Processed " + str(len(srcdomains)) + " Booter domain names.")

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
