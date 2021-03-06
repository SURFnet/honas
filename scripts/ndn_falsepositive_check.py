#!/usr/bin/python3
# --------------------------------------------
# NDN scenario false positive comparison
# tool for Bloom filter results.
# --------------------------------------------

import json
import argparse
import csv

# Parse input arguments.
parser = argparse.ArgumentParser(description='NDN scenario ground truth comparison tool')
parser.add_argument('-g', action='store', dest='ground_truth', help='Input text file containing ground truth', required=True)
parser.add_argument('-j', action='store', dest='result_file', help='Input JSON file containing the Bloom filter results', required=True)
parser.add_argument('-b', action='store', dest='blacklist_file', help='Input NDN blacklist file', required=True)
parser.add_argument('-v', action='store_true', dest='verbose', help='Verbose output')
parser.add_argument('-d', action='store_true', dest='dumpgt', help='Dump ground truth misses')
results = parser.parse_args()

# Storage variables.
searchresult_dict = {}
in_groundtruth = 0
notin_groundtruth = 0
total = 0
misses = []
groundtruth_dict = {}
blacklist_dict = {}

# Open input results file and JSON file.
with open(results.ground_truth, 'r') as ground_file, open(results.result_file, 'r') as result_file, open(results.blacklist_file, 'r') as blacklist_file:
	# Parse blacklist file.
	bl_reader = csv.reader(blacklist_file)
	for blent in bl_reader:
		blacklist_dict[blent[0]] = 0

	if results.verbose:
		print("Processed " + str(len(blacklist_dict)) + " entries from the NDN blacklist file.")

	# Parse the JSON file.
	searchresults = json.loads(result_file.read())
	period_begin = searchresults['first_request']
	period_end = searchresults['last_request']

	if results.verbose:
		print("First request: " + str(period_begin) + ", last request: " + str(period_end))
		print("Parsing ground truth file...")

	# Parse the CSV ground truth file.
	# 1531403599, Q(Q), 145.220.24.179, IN, A, ftp.xenonbooter.xyz.
	all_groundtruth = 0
	reader = csv.reader(ground_file, delimiter=',')
	for row in reader:
		# Get timestamp from date string.
		timestamp = int(row[0])

		# Only store query types Q(Q).
		if row[1].strip() != "Q(Q)":
			continue

		all_groundtruth += 1

		# Store the search result query if the timestamp falls in the stored region.
		if timestamp >= period_begin and timestamp <= period_end:
			# Strip the trailing dot from the hostname.
			hostname = row[5].rstrip('.').strip()

			#  Insert all domain name parts, starting from the first dot onwards.
			i = 0
			next = hostname
			while i != -1:
				# Check if the domain name appears in the ground truth.
				if next in blacklist_dict:
					# Store the domain name in a dictionary (enforcing uniqueness).
					total += 1
					groundtruth_dict[next] = 0

				# Find next label separator.
				i = hostname.find('.', i + 1)

				# Get the next domain name.
				next = hostname[i + 1 : len(hostname)]

	# Debugging
#	for k,v in groundtruth_dict.items():
#		print(k)

	if results.verbose:
		print("[" + str(total) + " / " + str(all_groundtruth) + "] from ground truth appear in blacklist.")
		print("Parsing JSON search results file...")

	# Parse all queries from the search results.
	for hostname in searchresults['groups'][0]['hostnames']:
		# Find out if there is an entity label.
#		atsign = hostname.find('@')

		# A label must be present!
#		if atsign <= 0:
#			continue

		# Check if the label is one of the provided. For this scenario, the query must be
		# prepended with one of the labels in order for it to be correct.
#		if hostname[0:atsign] not in labels:
#			continue

		# Take off the label, and compare to the ground truth.
#		if atsign > 0:
#			hostname = hostname[atsign + 1:len(hostname)]

		# Check search result against the NDN blacklist, and
		# store the search result in a dictionary.
		if hostname in blacklist_dict:
			searchresult_dict[hostname] = 0

	if results.verbose:
		print("Parsed " + str(len(searchresult_dict)) + " domain names from search results file.")

#	for k, v in searchresult_dict.items():
#		print(k)

	# Walk the dictionary to check the search results against the ground truth.
	for k, v in searchresult_dict.items():
		# Check whether ground truth entry is also in search results.
		if k in groundtruth_dict:
			in_groundtruth += 1
		else:
			notin_groundtruth += 1
			misses.append(k)

# Print statistics about the ground truth matching.
print("Statistics for " + results.result_file + ":" )
print("[" + str(in_groundtruth) + " / " + str(len(searchresult_dict)) + "] from the search results were also in the ground truth!")

# Only print non-existent entries if there are any.
if notin_groundtruth > 0:
	print("[" + str(notin_groundtruth) + " / " + str(len(searchresult_dict)) + "] from the search results were not in the ground truth!")

	if results.dumpgt:
		print("The following search results were not present in the ground truth:")
		index = 0
		for miss in misses:
			index +=1
			print("[" + str(index) + " / " + str(notin_groundtruth) + "] " + miss)
	else:
		print("Use the -d switch to find out which search results were not present in the ground truth.")
