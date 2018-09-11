#!/usr/bin/python3
# ------------------------------------------------
# Honas state rotation script. Run regularly to
# automatically archive Honas state information.
# ------------------------------------------------

import datetime
import glob
import os
import argparse
import shutil

HONAS_STATE_DIR = "/var/spool/honas"
HONAS_CONFIG_FILE = "/etc/honas/gather.conf"
HONAS_DATA_ARCHIVE_DIR = "/data"
HONAS_COMBINE_BIN = "/home/gijs/honas/build/honas-combine"
HONAS_INFO_BIN = "/home/gijs/honas/build/honas-info"

# Parse input arguments.
parser = argparse.ArgumentParser(description='Honas state archiving, rotation and merging tool')
parser.add_argument('-v', action='store_true', dest='verbose', help='Verbose output')
results = parser.parse_args()

if results.verbose:
	print("Performing state rotation...")

# Calculate the number of state files required for a full day.
state_interval = 0
with open(HONAS_CONFIG_FILE, 'r') as conf_file:
	for entry in conf_file.read().split('\n'):
		if entry.find("period_length") != -1:
			state_interval = int(entry[len("period_length") + 1:len(entry)])
			break

required_state_files = int(86400 / state_interval)
completed_states = {}
state_files = {}

if results.verbose:
	print("State interval is " + str(state_interval) + ", " + str(required_state_files) + " states required for daily rotation")

# Get all available state files.
for filename in glob.iglob(HONAS_STATE_DIR + "/*.hs"):
	if results.verbose:
		print("Found state file: " + filename)

	state_date = datetime.datetime.strptime(os.path.basename(filename).replace(".hs", ""), "%Y-%m-%dT%H:%M:%S")
	state_date_simplified = datetime.datetime.strftime(state_date, "%d-%m-%Y")

	# Create state-count mapping for easy completion check.
	if state_date_simplified in completed_states:
		completed_states[state_date_simplified] += 1
	else:
		completed_states[state_date_simplified] = 1

	# Store the state file name with the mapping for reference.
	state_files[filename] = state_date_simplified

# Loop over all states and check which are completed.
for k, v in completed_states.items():
	if v >= required_state_files:
		# This state is completed, we can archive and merge.
		if results.verbose:
			print("Daily state for " + k + " is completed!")

		# Create new folder for archive in data directory.
		new_state_archive = HONAS_DATA_ARCHIVE_DIR + "/" + k
		try:
			os.mkdir(new_state_archive)
			if results.verbose:
				print("Created archive directory " + new_state_archive)
		except OSError:
			print("Failed to create archive directory: directory exists!")
			continue

		# Move all state files that apply to this directory.
		moved = 0
		dest_state = ""
		for s, t in state_files.items():
			if k == t:
				basefile = os.path.basename(s)
				shutil.move(s, new_state_archive + "/" + basefile)
				if not dest_state:
					dest_state = basefile
				moved += 1

				if results.verbose:
					print("Moved state file " + s + " to archive directory " + new_state_archive)
