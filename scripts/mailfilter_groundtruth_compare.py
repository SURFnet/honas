#!/usr/bin/python3
# --------------------------------------------
# Mailfilter scenario ground truth comparison
# tool for Bloom filter results.
# --------------------------------------------

import sys
import json
import glob
import argparse
import csv
import datetime
import ipaddress

# Parse input arguments.
parser = argparse.ArgumentParser(description='Mailfilter scenario ground truth comparison tool')
parser.add_argument('-g', action='store', dest='ground_truth', help='Input text file containing ground truth', required=True)
parser.add_argument('-j', action='store', dest='result_file', help='Input JSON file containing the Bloom filter results', required=True)
parser.add_argument('-v', action='store_true', dest='verbose', help='Verbose output')
parser.add_argument('-d', action='store_true', dest='dumpgt', help='Dump ground truth misses')
results = parser.parse_args()

# Storage variables.
blacklistuniqs = {}
groundtruth_dict = {}
in_groundtruth = 0
notin_groundtruth = 0
total = 0
misses = []

# The labels of correct prefixes.
labels = [ "SURFnet bv", "SURFnet netwerk" ]

# Open input results file and JSON file.
with open(results.ground_truth, 'r') as ground_file, open(results.result_file, 'r') as result_file:
	# Parse the JSON file.
	searchresults = json.loads(result_file.read())
	period_begin = searchresults['first_request']
	period_end = searchresults['last_request']

	if results.verbose:
		print("First request: " + str(period_begin) + ", last request: " + str(period_end))
		print("Parsing ground truth file...")

	# Parse the CSV ground truth file.
	reader = csv.reader(ground_file, delimiter=' ')
	for row in reader:
		# Get timestamp from date string.
		timestamp = int(datetime.datetime.strptime(row[0], '%Y-%m-%dT%H:%M:%S+02:00').timestamp())

		# Store the search result query if the timestamp falls in the stored region.
		if timestamp >= period_begin and timestamp <= period_end:
			blacklistuniqs[row[1]] = 0

	# Now convert IPs to PTR, takes much shorter time when only uniques are converted.
	for k, v in blacklistuniqs.items():
		try:
			ipptr = ipaddress.ip_address(k).reverse_pointer

			# Store the PTR record.
			groundtruth_dict[ipptr] = 0

		except ValueError:
			print("Failed to parse " + k + "! Skipping.")

	if results.verbose:
		print("Parsed " + str(len(groundtruth_dict)) + " IP-addresses from ground truth file.")
		print("Parsing JSON search results file...")

	# Parse all queries from the search results.
	all_results = 0
	for hostname in searchresults['groups'][0]['hostnames']:
		# Find out if there is an entity label.
		atsign = hostname.find('@')
		all_results += 1

		# A label must be present!
		if atsign <= 0:
			continue

		# Check if the label is one of the provided. For this scenario, the query must be
		# prepended with one of the labels in order for it to be correct.
		if hostname[0:atsign] not in labels:
			continue

		# Take off the label, and compare to the ground truth.
		hostname = hostname[atsign + 1:len(hostname)]
		total += 1

		# Check whether ground truth entry is also in search results.
		if hostname in groundtruth_dict:
			in_groundtruth += 1
		else:
			notin_groundtruth += 1
			misses.append(hostname)

	if results.verbose:
		print("[" + str(total) + " / " + str(all_results) + "] were declared to be valid.")

# Print statistics about the ground truth matching.
print("Statistics for " + results.result_file + ":" )
print("[" + str(in_groundtruth) + " / " + str(total) + "] from search results are also in the ground truth!")
print("[" + str(notin_groundtruth) + " / " + str(total) + "] from search results are not in the ground truth!")

if results.dumpgt:
	print("The following search results were not present in the ground truth:")
	index = 0
	for miss in misses:
		index +=1
		print("[" + str(index) + " / " + str(notin_groundtruth) + "] " + miss)
else:
	print("Use the -d switch to find out which search results were not present in the ground truth.")
