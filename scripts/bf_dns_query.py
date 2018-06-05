#!/usr/bin/python3
# ----------------------------------------------
# Generates artificial DNS queries randomly of
# lowercase characters and digits between 5 and
# 15 characters in length. This script is used
# to test the Bloom filter prototype in the
# absence of real data.
# ----------------------------------------------

import sys
import time
import random
import string
import dns.resolver
import datetime

# Get target DNS server as input argument.
nameserver = ""
if len(sys.argv) > 1:
	nameserver = sys.argv[1]
else:
	print("Please enter the destination DNS server as first argument!")
	exit()

# A list containing random pre- and suffixes.
rnd_prefixes = [ "www", "mx", "mail", "ftp", "example", "test", "www2", "bierglas", "troll", "admin", "tld", "secure" ]
rnd_suffixes = [ "com", "nl", "org", "me", "be", "de", "es", "tk", "ch", "cn", "jp", "pt", "pl", "ru", "ro", "lu" ]
rnd_records = [ "A", "AAAA", "NS", "MX" ]

# Create DNS resolver object for querying.
resolv = dns.resolver.Resolver()
resolv.nameservers = [ nameserver ]

# Save current time for calculations in the end.
start = datetime.datetime.now()
generated = 0
failed = 0

# Start continuously generating random queries.
print("Random DNS query generator by Gijs Rijnders")
print("Generating random DNS queries to " + nameserver)
try:
	while True:
		# Generate random domain names
		rnd_pref = random.choice(rnd_prefixes)
		rnd_sld = ''.join(random.choices(string.ascii_lowercase + string.digits, k=random.randint(5, 15)))
		rnd_suf = random.choice(rnd_suffixes)
		qname = rnd_pref + "." + rnd_sld + "." + rnd_suf
		rec = random.choice(rnd_records)

		# Execute the generated query.
		try:
			generated += 1
			resolv.query(qname, rec)
		except:
			failed += 1

		# Sleep slightly to delay the query speed and allow interrupting.
		time.sleep(0.001)

except KeyboardInterrupt:
	print("Random DNS query generator was interrupted. Exiting!")

# Calculate and print statistics.
end = datetime.datetime.now()
elapsed = end - start
print("Generated " + str(generated) + " DNS queries (" + str(generated / elapsed.total_seconds()) + " queries per second).")
