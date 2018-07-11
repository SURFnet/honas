#!/usr/bin/python3
# -----------------------------------------------
# Compares the actual false positive rate and
# fill rate of all daily Bloom filters to the
# theoretical false positive rate, based on
# the initially chosen parameters
# -----------------------------------------------

import sys
import os
import glob
import ntpath
from datetime import datetime
import re
import csv

HONAS_INFO_BIN = "/home/gijs/honas/build/honas-info"

# Get target directory as first argument.
targetdir = ""
if len(sys.argv) > 1:
        targetdir = sys.argv[1]
else:
        print("Please enter the target directory as first argument!")
        exit()

# Open output file.
with open("act_vs_theor_fpr.csv", "w") as outputfile:
	outputfile.write("time,actfpr,fillrate\n")

	# Recursively find all daily Honas state files in the target directory.
	act_dict = {}
	for dayfile in glob.iglob(targetdir + "/**/2018-??-??.hs"):
		# Get the date from the filename.
		fn = ntpath.basename(dayfile)
		datestr = fn.replace(".hs", "")
		d = int(datetime.strptime(datestr, "%Y-%m-%d").timestamp())

		# Get the parameters and actual fill rate and false positive rate from the current file.
		info = os.popen(HONAS_INFO_BIN + " " + dayfile + " | grep 'Fill Rate:'").read()
		acts = re.findall(r"[-+]?\d*\.\d+|\d+", info)
		fillrate = acts[0]
		act_fpr = acts[1]

		# Store the fill rate and actual fpr.
		outputfile.write(str(d) + "," + str(act_fpr) + "," + str(fillrate) + "\n")
