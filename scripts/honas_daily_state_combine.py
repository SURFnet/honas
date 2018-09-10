#!/usr/bin/python3
# ----------------------------------------------
# Aggregates (combines) all hourly states of a
# day of Honas Bloom filters.
# ----------------------------------------------

import sys
import os
from datetime import datetime
from shutil import copyfile
from subprocess import call

HONAS_COMBINE_BIN = "/home/gijs/honas/build/honas-combine"

# Get daily directory as first argument.
targetdir = ""
if len(sys.argv) > 1:
	targetdir = sys.argv[1]
else:
	print("Please enter the target directory as first argument!")
	exit()

# Copy a file to the destination file for the aggregation process.
first_iteration = os.listdir(targetdir)
destination_file = ""
if len(first_iteration) > 0:
	state_time = datetime.strptime(first_iteration[0].replace(".hs", ""), "%Y-%m-%dT%H:%M:%S")
	destination_file = targetdir + "/" + state_time.strftime("%Y-%m-%d") + ".hs"
	copyfile(targetdir + "/" + first_iteration[0], destination_file)
	print("Created destination file " + destination_file)

# Was the previous step succesful?
if len(destination_file) > 0:
	# Iterate through all files again, and aggregate them.
	for state_file in first_iteration:
		srcfile = targetdir + "/" + state_file
		call([ HONAS_COMBINE_BIN, destination_file, srcfile ])
