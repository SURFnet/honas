#!/usr/bin/python
# ---------------------------------------------
# Query tool to find differences between Booter
# blacklists and Honas Bloom filter contents.
# Also used to calculate the false positive
# rate of the Bloom filters.
# ---------------------------------------------

import sys
import csv
import json
from hashlib import sha256
import os

# Check if we have an input PCAP file.
booterfile = ""
if len(sys.argv) > 1:
	booterfile = sys.argv[1]
else:
	print("Please enter a Booter blacklist file as first argument!")
	exit()

print("Processing DNS queries from input PCAP file.")

# Prepare Honas search query.
searchdata = { "groups" : [] }
searchdata["groups"].append({ "id" : 1 })
searchdata["groups"][0]["hostnames"] = {}

# Prepare a dictionary containing all unique domain names in the PCAP file.
# For convinience we store the number of times it occurred as value.
srcdomains = {}

# Read the Booter blacklist file.
with open(booterfile, "r") as inpfile:
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

# Write the Honas JSON query to a temporary file.
tmpfilename = "honas_tmp_query.json"
with open(tmpfilename, 'w') as tmpfile:
	tmpfile.write(json.dumps(searchdata, indent=4))

# Execute the query to Honas.
#statefile = "/var/spool/honas/active_state"
statefile = "/var/spool/honas/2018-06-22T10:00:00.hs"
searchresult = os.popen("/home/gijs/honas/build/honas-search " + statefile + " < " + tmpfilename).read()

# Parse the Honas search result and test whether false positives occurred.
jsonresult = json.loads(searchresult)

# Check if there are results.
if len(jsonresult["groups"]) <= 0:
	print("The search job did not return any results!")
	exit()

# Get the hostnames in the result set.
hostnames = jsonresult["groups"][0]["hostnames"]

# Check whether all domain names in the PCAP files occurred in the Bloom filters.
notfound = []
for key, value in srcdomains.iteritems():
	if key not in hostnames:
		notfound.append(key)

# Print some statistics about the process.
print("Processed " + str(pkt_count) + " relevant DNS queries from the PCAP file.")
print("Of the " + str(len(srcdomains)) + " unique domain names in the PCAP file, " + str(len(hostnames)) + " were also found in the Bloom filters.")
print("The following " + str(len(notfound)) + " domain names were not found:")

# Print the domain names that were not found in the Bloom filters.
for nf in notfound:
	print(nf)
