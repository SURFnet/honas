#!/usr/bin/python3
# -------------------------
# Automated MISP blacklist
# downloader for Honas.
# -------------------------

import requests
import argparse
import datetime

# Create file 'keys.py' according to PyMISP structure. We can leave out the verify statement.
from keys import misp_key,misp_url

# Parse input arguments.
parser = argparse.ArgumentParser(description='Automated MISP IoC blacklist downloader for Honas')
parser.add_argument('-v', action='store_true', dest='verbose', help='Verbose output')
results = parser.parse_args()

downloadurl = misp_url + 'attributes/text/download/domain'

if results.verbose:
	print("Downloading IoC blacklist at " + downloadurl)

# Create MISP query for this hit.
downloadheaders = { 'Authorization' : misp_key }
r = requests.post(downloadurl, headers=downloadheaders)

# Write out the blacklist to a file.
datetoday = datetime.datetime.now()
fn = 'misp_blacklist-' + datetime.datetime.strftime(datetoday, '%Y-%m-%d') + '.txt'
with open(fn, 'w') as outfile:
	outfile.write(r.text)

if results.verbose:
	print("Wrote IoC blacklist to " + fn)
