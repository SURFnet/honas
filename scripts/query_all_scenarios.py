#!/usr/bin/python3
# ---------------------------------------------------|
# Query tool that performs queries for all scenarios |
# currently in the Bloom filter system. This script  |
# can be run periodically, for example using cron,   |
# to retrieve search results automatically.          |
# ---------------------------------------------------|

import os
import json
import logging
import logging.handlers
import glob

HONAS_QUERY_JOBS_CONF_DIR = "/etc/honas/periodic_search_jobs.conf"
HONAS_GENERIC_BLACKLIST_QUERY_SCRIPT = "/home/gijs/honas/scripts/query_generic_blacklist.py"
HONAS_DATA_ARCHIVE_DIR = "/data"

# Initialize Syslog.
log = logging.getLogger('honas_searchjob_execution')
log.setLevel(logging.DEBUG)
handler = logging.handlers.SysLogHandler(address = '/dev/log')
formatter = logging.Formatter('%(module)s: %(message)s')
handler.setFormatter(formatter)
log.addHandler(handler)

# Check if the search jobs configuration file exists.
if not os.path.exists(HONAS_QUERY_JOBS_CONF_DIR):
	log.debug("The configuration file " + HONAS_QUERY_JOBS_CONF_DIR + " does not exist!")
	exit(1)

# Retrieve all available state files in the data archive.
state_files = []
for state_file in glob.iglob(HONAS_DATA_ARCHIVE_DIR + "/**/????-??-??.hs"):
	state_files.append(state_file)

# Retrieve a list of all search jobs that should be performed.
with open(HONAS_QUERY_JOBS_CONF_DIR, 'r') as conf_file:
	confjson = json.loads(conf_file.read())
	entities_file = confjson["entities_file"]

	# Check if the entities file exists.
	if not os.path.exists(entities_file):
		log.debug("The entities file " + entities_file + " does not exist!")
		exit(1)

	# Get the search jobs and iterate over them.
	jobs = confjson["searchjobs"]
	for job in jobs:
		# Get the name, blacklist file path, and results directory.
		name = job["name"]
		blacklist = job["blacklist"]
		result_directory = job["result_directory"]

		# Get a list of search results.
		to_query = []
		result_basenames = []
		resultfiles = glob.iglob(result_directory + "/*.hs.json")
		for resfile in resultfiles:
			basefile = os.path.basename(resfile).replace(".json", "")
			result_basenames.append(basefile)

		# Check which state files must be queried.
		for sf in state_files:
			base_state = os.path.basename(sf)
			if not any(base_state in s for s in result_basenames):
				to_query.append(sf)

		# If the query queue is empty, log that information.
		if not to_query:
			log.debug("No state files have to be queried for the " + name + " search job.")

		# Execute the search job on all state files that have not been queried yet.
		for k in to_query:
			callcommand = HONAS_GENERIC_BLACKLIST_QUERY_SCRIPT + " -b " + blacklist + " -o " + result_directory + " -s " + k + " -e " + entities_file
			calldata = os.popen(callcommand).read()
			log.debug("Performed query on state file " + k + " for the " + name + " search job.")
