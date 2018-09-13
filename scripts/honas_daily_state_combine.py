#!/usr/bin/python3
# ----------------------------------------------
# Aggregates (combines) all hourly states of a
# day of Honas Bloom filters.
# ----------------------------------------------

import os
from datetime import datetime
from shutil import copyfile
import logging
import logging.handlers

HONAS_COMBINE_BIN = "/home/gijs/honas/build/honas-combine"
HONAS_INFO_BIN = "/home/gijs/honas/build/honas-info"
HONAS_DATA_ARCHIVE_DIR = "/data"
HONAS_ROTATION_FILE = HONAS_DATA_ARCHIVE_DIR + "/.honas_state_rotation"

# Initialize Syslog.
log = logging.getLogger('honas_state_rotate')
log.setLevel(logging.DEBUG)
handler = logging.handlers.SysLogHandler(address = '/dev/log')
formatter = logging.Formatter('%(module)s: %(message)s')
handler.setFormatter(formatter)
log.addHandler(handler)

# Check if the state rotiation file exists.
if not os.path.exists(HONAS_ROTATION_FILE):
	log.debug("Failed to open " + HONAS_ROTATION_FILE + "! The file does not exists.")
	exit(1)

# Read the rotation file to see which states must be merged.
with open(HONAS_ROTATION_FILE, 'r') as rotation_file:
	to_rotate = rotation_file.read().split('\n')
	# Loop through all lines in the rotation file.
	for line in to_rotate:
		# Check if the line is not empty
		if not line:
			log.debug("Skipping empty directory...")
			continue

		# Create absolute path.
		merge_path = HONAS_DATA_ARCHIVE_DIR + "/" + line

		# Try to open the directory in case.
		try:
			# Copy a file to the destination file for the aggregation process.
			first_iteration = os.listdir(merge_path)
			destination_file = ""
			if len(first_iteration) > 0:
				try:
					tmp_fn = first_iteration[0].replace(".hs", "")
					state_time = datetime.strptime(tmp_fn, "%Y-%m-%dT%H:%M:%S")
					destination_file = merge_path + "/" + state_time.strftime("%Y-%m-%d") + ".hs"
					copyfile(merge_path + "/" + first_iteration[0], destination_file)
					log.debug("Created destination file " + destination_file)
				except ValueError:
					log.debug("The destination file " + tmp_fn + ".hs already exists! Skipping...")
					continue
			else:
				log.debug("Failed to merge states in " + merge_path + "! No state files were found.")
				continue
		except FileNotFoundError:
			log.debug("Failed to open directory " + merge_path)
			continue

		# Was the previous step succesful?
		if len(destination_file) > 0:
			# Iterate through all files again, and aggregate them.
			for state_file in first_iteration:
				srcfile = merge_path + "/" + state_file
				os.popen(HONAS_COMBINE_BIN + " -v " + destination_file + " " + srcfile)

		log.debug("Combined states in " + merge_path + " into " + destination_file)

		# Check the fill rate of the final combined state, to catch cases where (s/m)^k > 1/2.
		state_info = os.popen(HONAS_INFO_BIN + " " + destination_file).read()
		for info_line in state_info.split('\n'):
			fillrate_index = info_line.find("Fill Rate:")
			if info_line and fillrate_index >= 0:
				log.debug(info_line)
				break

# Delete the state rotation file. It will be recreated by the rotation script once required.
os.remove(HONAS_ROTATION_FILE)
log.debug("Deleted state rotation file " + HONAS_ROTATION_FILE)
