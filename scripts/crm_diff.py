#!/usr/bin/python3
# --------------------------------------
# CRM (prefix-entity mapping) file
# difference checker. Every day a new
# CRM file is provided. If there are
# changes, the subnet activity config-
# ation file should be regenerated,
# and the Honas gathering process
# should be restarted to reflect the
# changes.
# --------------------------------------

import os
import argparse

HONAS_SUBNET_ACTIVITY_SCRIPT = "/home/gijs/honas/scripts/subnet_definitions_generator.py"
HONAS_GATHER_CONF_SUBNET_ACTIVITY_PATH = "/etc/honas/subnet_activity.json"

# Parse input arguments.
parser = argparse.ArgumentParser(description='CRM file difference checking tool')
parser.add_argument('-d', action='store', dest='crm_dir', help='Input directory containing CRM files', required=True)
parser.add_argument('-v', action='store_true', dest='verbose', help='Verbose output')
results = parser.parse_args()

# Get the two latest daily files, and take their difference.
latestfiles = os.popen("ls -tr " + results.crm_dir + " | tail -n 2").read().split('\n')

# take the difference between the file contents.
diff = ""
if len(latestfiles) >= 2:
	diff = os.popen("diff " + results.crm_dir + "/" + latestfiles[0] + " " + results.crm_dir + "/" + latestfiles[1]).read()
else:
	print("At least two CRM files are required!")
	exit(1)

# Check if a difference was returned.
if diff:
	# We found a difference, so the configuration should be regenerated.
	subactgen = os.popen(HONAS_SUBNET_ACTIVITY_SCRIPT + " -r " + results.crm_dir + "/" + latestfiles[1] + " -w " + HONAS_GATHER_CONF_SUBNET_ACTIVITY_PATH).read()
	if "Wrote JSON output to" in subactgen:
		if results.verbose:
			print("Succesfully generated subnet activity configuration file.")
			print("The Honas gathering process will automatically reload the new configuration file.")
	else:
		print("Failed to generate subnet activity configuration file!")
		exit(1)
else:
	# No difference was returned.
	if results.verbose:
		print("No difference was found in the latest CRM files.")
