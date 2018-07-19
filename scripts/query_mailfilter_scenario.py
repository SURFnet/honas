#!/usr/bin/python3
# ---------------------------------------------
# Query tool to create Honas queries from a
# list of spamfilter PTR record queries.
# ---------------------------------------------

import sys
import csv
import json
from hashlib import sha256
import os
import ipaddress
import glob
import ntpath
import datetime

HONAS_STATE_DIR = "/data"
HONAS_BIN_PATH = "/home/gijs/honas/build/honas-search"
ENTITY_FILE = "entities_out.csv"

# Get the target directory for ground truth files.
targetdir = ""
if len(sys.argv) > 1:
	targetdir = sys.argv[1]
else:
	print("Please enter the target directory as first argument!")
	exit()

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

# Get all applicable file.
for fn in glob.iglob(targetdir + "/mail-2018-07-??.log"):
	print("Processing PTR queries from " + fn)

	# Get timestamp from input file.
	ind = fn.find("mail-")
	timestamp = fn[ind + len("mail-") : len(fn) - len(".log")]

	# Prepare Honas search query.
	searchdata = { "groups" : [] }
	searchdata["groups"].append({ "id" : 1 })
	searchdata["groups"][0]["hostnames"] = {}

	# Prepare a dictionary containing all unique IP-addresses in the spamfilter file.
	# For convinience we store the number of times it occurred as value.
	srcips = {}

	# Read the spamfilter file.
	with open(fn, 'r') as target_file:
		reader = csv.reader(target_file, delimiter=' ')
		for row in reader:
			ipaddr = row[1].strip()

			# Store the domain name in the dictionary for comparison later.
			# This way we remove the duplicates, which reduces lookup time.
			if ipaddr in srcips:
				srcips[ipaddr] += 1
			else:
				srcips[ipaddr] = 1

		# Print statistics.
		print("Processed " + str(len(srcips)) + " unique IP addresses.")

	# Perform reverse lookups for the domain names.
	q_count = 0
	for k, v in srcips.items():
		try:
			# Create the reverse DNS lookup for the IP address.
			n = ipaddress.ip_address(k).reverse_pointer
			q_count += 1

			# Create Honas JSON query for this domain name.
			searchdata["groups"][0]["hostnames"][n] = sha256(n.encode('utf-8')).hexdigest()

			# Iterate through all entities in the entities file, and generate
			# a query for every combination of domain name and entity.
			for k1, v1 in entities.items():
				compound = str(k1) + '@' + str(n)
				searchdata["groups"][0]["hostnames"][compound] = sha256(compound.encode('utf-8')).hexdigest()
				q_count += 1

			# Also generate an UNKNOWN entity query to identify unmapped requests.
			unk = "UNKNOWN@" + str(n)
			searchdata["groups"][0]["hostnames"][unk] = sha256(unk.encode('utf-8')).hexdigest()
			q_count += 1

		except ValueError:
			print("Failed to parse " + k + "! Skipping.")

	# Print statistics.
	print("Generated " + str(q_count) + " queries.")

	# Write the Honas JSON query to a temporary file.
	tmpfilename = "honas_tmp_query.json"
	with open(tmpfilename, 'w') as tmpfile:
		tmpfile.write(json.dumps(searchdata, indent=4, ensure_ascii=False))

	# Convert timestamp to directory format.
	timeobj = datetime.datetime.strptime(timestamp, "%Y-%m-%d").strftime("%d-%m-%Y")

	# Execute the query to Honas.
	filename = HONAS_STATE_DIR + "/" + timeobj + "/" + timestamp + ".hs"
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
