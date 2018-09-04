#!/usr/bin/python3
# ----------------------
# Automated MISP lookup
# from Honas results.
# ----------------------

import requests
import argparse
import json

# Create file 'keys.py' according to PyMISP structure. We can leave out the verify statement.
from keys import misp_key,misp_url

# Parse input arguments.
parser = argparse.ArgumentParser(description='Automated MISP IoC lookup tool for Honas')
parser.add_argument('-s', action='store', dest='search_value', help='The value to search the MISP for', required=True)
parser.add_argument('-v', action='store_true', dest='verbose', help='Verbose output')
results = parser.parse_args()

searchurl = misp_url + 'attributes/restSearch'

if results.verbose:
	print("Performing search job in MISP at " + searchurl + " for " + results.search_value)

# Create MISP query for this hit.
searchheaders = { 'accept' : 'application/json', 'content-type' : 'application/json', 'Authorization' : misp_key }
payload = '{ "value" : "' + results.search_value + '" }'
r = requests.post(searchurl, data=payload, headers=searchheaders)

# Load the JSON result from the search job.
jsonresult = json.loads(r.text)
attributes = jsonresult["response"]["Attribute"]

# Threat level dictionary.
threatlevels = { 1 : "High", 2 : "Medium", 3 : "Low", 4 : "Undefined" }

# Loop through attributes.
for att in attributes:
	# Only use 'domain' attributes.
	if att["type"] == "domain":
		eventid = att["event_id"]

		# We have the event ID, now lets look up the event and take some information about it.
		event = requests.get(misp_url + "/events/" + str(eventid), headers=searchheaders)
		jsonevent = json.loads(event.text)
		eventresp = jsonevent["Event"]

		# Print some information about the event.
		print("----------------------------------")
		print("Info: " + eventresp["info"])
		print("Threat Level: " + threatlevels[int(eventresp["threat_level_id"])])
