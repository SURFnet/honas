#!/usr/bin/python
# ---------------------------------------------
# Query tool to find differences between packet
# capture and Honas Bloom filter contents. Also
# used to calculate the false positive rate of
# the Bloom filters.
# ---------------------------------------------

import sys
import csv
import dpkt
import json
from hashlib import sha256
import os

# Check if we have an input PCAP file.
pcapfile = ""
if len(sys.argv) > 1:
	pcapfile = sys.argv[1]
else:
	print("Please enter a PCAP file as first argument!")
	exit()

# Open input PCAP file.
pkt_count = 0
f = open(pcapfile, 'rb')

print("Processing DNS queries from input PCAP file.")

# Prepare Honas search query.
searchdata = { "groups" : [] }
searchdata["groups"].append({ "id" : 1 })
searchdata["groups"][0]["hostnames"] = {}

# Prepare a dictionary containing all unique domain names in the PCAP file.
# For convinience we store the number of times it occurred as value.
srcdomains = {}

# Read PCAP file.
for ts, pkt in dpkt.pcap.Reader(f):
	# Get the Ethernet part.
	eth = dpkt.ethernet.Ethernet(pkt)
	if eth.type != dpkt.ethernet.ETH_TYPE_IP:
		continue

	# Get the IP part.
	ip = eth.data

	# Make sure we only pick UDP packets
	if ip.p != dpkt.ip.IP_PROTO_UDP:
		continue

	udp = ip.data

	# Only pick packets for port 53 (DNS).
	if udp.dport != 53:
		continue

	# Create DNS packet structure, and only pick queries.
	dns = dpkt.dns.DNS(udp.data)
	if dns.opcode != dpkt.dns.DNS_QUERY:
		continue

	# Extract queries from query.
	for qname in dns.qd:
		# Only process A, NS, MX and AAAA records.
		if qname.type == 1 or qname.type == 2 or qname.type == 15 or qname.type == 28:
			# Store the domain name in the dictionary for comparison later.
			# First canonicalize the domain name! We need it to be in lower case.
			dnsname = qname.name.lower()
			if dnsname in srcdomains:
				srcdomains[dnsname] += 1
			else:
				srcdomains[dnsname] = 1

			# Create Honas JSON query for this domain name.
			searchdata["groups"][0]["hostnames"][dnsname] = sha256(dnsname).hexdigest()

			# Inrement counters.
			pkt_count += 1

# Write the Honas JSON query to a temporary file.
tmpfilename = "honas_tmp_query.json"
with open(tmpfilename, 'w') as tmpfile:
	tmpfile.write(json.dumps(searchdata, indent=4))

# Execute the query to Honas.
statefile = "/var/spool/honas/active_state"
searchresult = os.popen("/home/gijs/honas/build/honas-search " + statefile + " < " + tmpfilename).read()

# Parse the Honas search result and test whether false positives occurred.
jsonresult = json.loads(searchresult)
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
