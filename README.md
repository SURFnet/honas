[TOC]

Honas - Host name searching                      {#mainpage}
===========================

Honas is a system for collecting large quantities of hostname lookup requests
in order to check afterwards if certain hostnames have been requested by clients.

It tries to prevent disclosing privacy sensitive details about individual
clients by making use of coarse grained cardinality estimation using multiple
probabilistic data structures in the form of [bloom
filters](https://en.wikipedia.org/wiki/Bloom_filter).

Honas was originally developed by Quarantainenet, but extended by SURFnet to
fit an ISP context. This extended prototype features a [dnstap](http://dnstap.info/)
input for untraceability. Furthermore, it allows network operators to identify
the organization or network segment DNS queries were performed in. With this feature,
the prototype is useful to gain insight into the threat landscape of an organization.

Build instructions                               {#build_instructions}
------------------

Honas is build using the Meson build system, for installation instructions and
more information, see the [Meson website](http://mesonbuild.com/).

### Building the programs:

Install the necessary dependencies (command below is for Debian based systems):

```
apt-get install check libyajl-dev libevent-dev libfstrm-dev libprotobuf-dev protobuf-c-compiler clang-tidy
```

Create the meson build directory:

```
meson build/
```

Enter the meson build directory and build all targets:

```
cd build/
ninja
```

### Project build-time configuration options

Run from inside the meson build directory.

- Change the default `honas-gather.conf` path (default: `/etc/honas/gather.conf`):
	`meson configure -Ddefault_honas_gather_config_path=/path/to/honas-gather.conf`
- Disable the use some hand-coded assembler optimizations:
	`meson configure -Duse_asm=false`

### Additional build options

Run from inside the meson build directory.

- Release mode:
	`meson configure -Dbuildtype=release`
- Strip executables:
	`meson configure -Dstrip=true`
- More warnings:
	`meson configure -Dwarning_level=3`
- Fatal warnings:
	`meson configure -Dwerror=true`
- Address sanitzation:
	`meson configure -Db_sanitize=address`
- For more options see:
	`meson configure`

### Suggested build instructions for release builds

```
meson build/
cd build/
meson configure -Dbuildtype=release -Dwarning_level=1 -Dwerror=false -Dunity=on -Db_lto=true -Db_coverage=false -Db_sanitize=none
ninja
```

Documentation                                    {#documentation}
-------------

The interfaces are documented using [Doxygen](http://www.doxygen.org/). The
main page of the generated documentation will also include this text in a HTML
rendered form.

 To generate the documentation install doxygen and run (from the meson build
directory):

```
ninja doc
```

### Overview                                     {#overview}

The main Honas programming interface is the [honas state](@ref honas_state).
But Honas is mainly designed to be run in the form of a set of programs that
generate and search through the honas state files that are managed through the
said interface.

Below is a diagram that shows the different parts of Honas and how they fit together:

![Honas overview](doc/images/honas_overview.svg)

Each of these parts is discussed in more detail below.

### DNS logging                                  {#dns_logging}

Honas is designed to be run as a separate process that processes DNS server
logging. The extended prototype is designed to accept DNS logs via a [dnstap](http://dnstap.info/)
interface. With that interface, Honas integrates easily into major DNS resolver
packages such as [Unbound](https://www.nlnetlabs.nl/projects/unbound/about/) 
and [Bind](https://www.isc.org/downloads/bind/). Moreover, the DNS logs are transferred from
the resolver process to Honas in an untraceable way.

Honas stores the FQDN and all separate labels from the FQDN in the Bloom filters, including
some combinations. This allows DGA detection to some extent. The example below indicates
which information from a DNS query is stored in the Bloom filters. The example domain name is

```
malicious.site.example.com
```

The following information is stored.

```
malicious.site.example.com
example.com
malicious
site
example
```

If the source address of the DNS query can be matched to a known IPv4 or IPv6 prefix, the following
information is stored additionally. Suppose that the matched prefix belongs to organization A. (No
spaces between the @ sign, this is for markdown compilation)

```
A @ malicious.site.example.com
A @ example.com
A @ malicious
A @ site
A @ example
```

##### Notes

The following notes apply to the collection and storage of DNS logs.

- The timestamp value is ignored and "now" is used when processing the entries in the state file
- All host names are expected to be in ASCII, other Unicode host names should be [punycode](https://en.wikipedia.org/wiki/Punycode) encoded
- A trailing dot after the host name is stripped before being hashed
- The TLD label is not stored because it does not identify a domain name in any way
- Only DNS CLASS 1 (`Internet (IN)`) gets processed
- Only DNS Resource Records of type 1 (`A`), 2 (`NS`), 15 (`MX`) and 28 (`AAAA`) are processed

### Subnet activity mapping using source IP-address	{#subnet_activity}

Honas features a subsystem that tests the source IP-address against a collection
of entity-prefix mappings. As we discussed above, the entity is then prepended
and stored with the domain name or label. The entity-prefix mappings are loaded
using a JSON file of subnet activity. The required configuration entry is called
`subnet_activity_path` as we will discuss later. An example of such file is
descibed below.

```
{
        "subnet_activity" :
        [
                {
                        "entity" : "SURFnet",
                        "prefixes" :
                        [
                                { "192.87.0.0" : 16 },
                                { "145.0.0.0" : 8 },
                                { "192.42.0.0" : 16 }
                        ]
                },
                {
                        "entity" : "netSURF",
                        "prefixes" :
                        [
                                { "192.42.113.0" : 24 },
                                { "145.220.0.0" : 16 },
                                { "2001:67c:6ec::" : 48 },
                                { "2001:67c:6ec:201::" : 64 }
                        ]
                },
        ]
}
```

Manually crafting this JSON file can be time consuming. Therefore, we built a
script that generates this file from a CSV file containing prefixes and entities.
This script is located in the `scripts` directory and is called `subnet_definitions_generator.py`.
The script can be used as follows.

```
usage: subnet_definitions_generator.py [-h] -r INPUT_FILE [-w OUTPUT_FILE]
                                       [-v]

Honas subnet definitions file generator

optional arguments:
  -h, --help      show this help message and exit
  -r INPUT_FILE   Input file containing entity-prefix mappings
  -w OUTPUT_FILE  Output file receiving the JSON mappings
  -v              Verbose output
```

The INPUT_FILE is the entity-prefix mapping file in CSV format. The OUTPUT_FILE receives
the JSON output that can be loaded into `honas-gather`. If no output file is specified,
the JSON is written to stdout. Note that the entity-prefix mappings may change over time.
When the entity-prefix mappings change, the configuration file must be regenerated. We
developed a script that checks for differences between CRM files over time, and regenerates
the JSON configuration file if required. This configuration file is then automatically
reloaded by `honas-gather` once the `period_length` interval passes. The script is called
`crm_diff.py`, and is located in the `scripts` directory. This script can be executed
automatically on a daily basis, using cron. An example configuration is shown below.

```
# Checking for CRM changes, to keep the subnet activity configuration up to date.
0 7 * * * /home/gijs/honas/scripts/crm_diff.py -d ~crm-sync/ -v
```

### The Honas state file                         {#honas_state_file}

The parsed host name lookups are saved to Honas state files. There are multiple
state files, one for each period of time.

The finished state files are named after their period begin timestamp (the time
they get created) in UTC, like `2017-12-06T11:50:21.hs`. This ensures that
simply sorting the state files alphabetically will order them correctly in
time.

The period ends on an exact multiple of the period duration since the unix
epoch time. This ensures that results of multiple servers with the same period
duration can be grouped together based on their period end time.

#### State file header

Each honas state file contains all the relevant meta data in the state file
header and no separate configuration of other knowledge is necessary to
interpret results contained within that file.

This includes:
- Period begin and end timestamps
- The configuration about the bloom filters:
  + Number of bloom filters
  + Number of bits per bloom filter
  + Number of hashes per bloom filter
  + Number of bloom filters per client
- Statistical information:
  + First request
  + Last request
  + Number of requests
  + Estimated number of distinct clients
  + Estimated number of distinct host names

All state files have a version number that should follow [Semantic
Versioning](https://semver.org/) rules.

#### Bloom filters

Each state file contains a number of [bloom
filters](https://en.wikipedia.org/wiki/Bloom_filter). Host name lookups are
registered based on a SHA-256 hash of that host name and for all parent
domains, except for the TLD itself.  For checking if a host name might have
been looked up only the SHA-256 hash of the host name to be checked has to be
provided.

Different clients will update a different subset of the bloom filters in the
state file. The number of bloom filters a host name hash is found in can be
used in combination with the number of bloom filters and the number of bloom
filters per client to make very coarse grained estimations about the number of
different clients that requested that host name.

#### HyperLogLogs

Each state file also contains two
[HyperLogLog](https://en.wikipedia.org/wiki/HyperLogLog) data structures. These
are used for providing estimations about the number of distinct clients being
active and the number of distinct host name hashes that are registered across
all bloom filters.

These values can be used as a basis for optimally configuring the bloom filters
based on historical data.

The HyperLogLog datastructures are embedded in the state file to allow state
files to be saved and loaded multiple times while they're being filled.  When
the state files are written to disk the estimates are written to the state file
header.

### Search job                                   {#search_job}

Search jobs are JSON encoded data structures that can be used to get search
results from a state file using `honas-search`.

#### Format

```json
{
	"groups": [
		{
			"id": <integer>,
			"hostnames": {
				"<key>": "<hex encoded SHA-256 host name hash>",
				...
			}
		},
		...
	]
}
```

#### Notes

- The most minimal valid search job specification is: `{}`
- The group "id" is optional and (currently) must be numeric if present
- The host name "key" can be anything, but it's recommended to keep it unique
  within that "hostname" object (though the current implementation doesn't
  care).

#### Example

```json
{
	"groups": [
		{
			"id":1,
			"hostnames": {
				"www.java.com": "dfefecff1f2e77bfef84ef74920e77c23c811dd70df0b3b281521814e85c00ee"
			}
		}
	]
}
```

### Search result                                {#search_result}

The search results are JSON encoded data structures generated based upon a
search job by `honas-search`.

#### Format

```json
{
	"node_version": "<version string>",
	"state_file_version": "<version string>",
	"period_begin": <integer>,
	"first_request": <integer>,
	"last_request": <integer>,
	"period_end": <integer>,
	"estimated_number_of_clients": <integer>,
	"estimated_number_of_host_names": <integer>,
	"number_of_requests": <integer>,
	"number_of_filters": <integer>,
	"number_of_filters_per_user": <integer>,
	"number_of_hashes": <integer>,
	"number_of_bits_per_filter": <integer>,
	"flatten_threshold": <integer>,
	"filters" : [
		{
			"number_of_bits_set": <integer>,
			"estimated_number_of_host_names": <integer>
			"actual_false_positive_rate": <string>,
		},
		...
	],
	"flattened_results": <boolean>,
	"groups" : [
		{
			"id": <integer>,
			"hostnames": {
				"www.java.com": <integer>,
				...
			},
			"hits_by_all_hostnames": <integer>
		},
		...
	]
}
```

#### Notes

- The fields `period_begin`, `first_request`, `last_request` and `period_end`
  are in number of seconds since unix epoch.
- All integer values can be represented as unsigned integers, no negative
  values are to be expected.
- If the search job contained no `groups` item then the results will also have
  no `groups` and no `flattened_results` items. Conversely if the search job
  had a `groups` item, then the result will also always have `groups` and
  `flattened_results` items, even if there were no hits for any of the
  host name hashes.
- If the search job group had no `id` item then the results will also have no
  `id` item.
- Host name hashes that didn't have enough hits (at least
  `number_of_filters_per_user`) will not be present in the result. The number
  reported is the number of filters the host name was probably present in.
- Groups for which none of the host names had enough hits will not be present
  in the result.
- The `hits_by_all_hostnames` field tells the number of filters that probably
  contained all the host name hashes in that group.
- All the other information fields are always included in the result, even if
  none of the queried host name hashes was found.
- If it was deemed that not enough distinct clients were active in this period
  (`estimated_number_of_clients` is less then the `flatten_threshold`) then the
  value of `flattened_results` will be true and all the hit counts will be at
  most 1.
- The `actual_false_positive_rate` provides an estimation of the actual false
  positive rate. This estimation depends on the fill rate (number_of_bits_set)
  of the Bloom filter, its current state. The value is stored as string, but
  contains a double value.

#### Example

```json
{
   "number_of_filters" : 1,
   "number_of_filters_per_user" : 1,
   "flattened_results" : false,
   "number_of_requests" : 175727076,
   "estimated_number_of_clients" : 14287,
   "period_end" : 1532127600,
   "number_of_hashes" : 10,
   "node_version" : "1.0.0",
   "flatten_threshold" : 1,
   "first_request" : 1532041245,
   "estimated_number_of_host_names" : 162956933,
   "groups" : [
      {
         "hits_by_all_hostnames" : 1,
         "id" : 1,
         "hostnames" : {
            "www.java.com" : 1
         }
      }
   ],
   "last_request" : 1532127645,
   "number_of_bits_per_filter" : 491040000,
   "filters" : [
      {
         "actual_false_positive_rate" : "0.0000079791",
         "estimated_number_of_host_names" : 18161591,
         "number_of_bits_set" : 151814229
      }
   ],
   "state_file_version" : "1.0",
   "period_begin" : 1532041245
}
```

#### Scripts for testing DNS blacklists		{#search_automation}

Manually crafting a JSON file as discussed above takes time. Therefore, we
introduced a script for testing a generic domain name blacklist against a 
Honas state file. The script is located in the `scripts` directory and is 
called `query_generic_blacklist.py`. Usage information is as follows.

```
usage: query_generic_blacklist.py [-h] -b BLACKLIST_FILE -s STATE_FILE -e
                                  ENTITY_FILE [-v]

Query tool for generic domain blacklists

optional arguments:
  -h, --help         show this help message and exit
  -b BLACKLIST_FILE  Input blacklist file containing domain names
  -s STATE_FILE      Honas state file to query
  -e ENTITY_FILE     Input file containing all stored entities
  -v                 Verbose output

```

In which the domain name blacklist is BLACKLIST_FILE, the state file to test
against is STATE_FILE, and ENTITY_FILE is a list of all possible entities that
could be stored in the Bloom filters. This information is used to generate all
queries of the form `<possible_entity>@<blacklisted_domain_name>`. Note that 
the entity file must be updated when organizations are added or removed. The 
number of queries that the script generates and tests is the Cartesian product 
of the input blacklist and the entity file. The script will output the JSON file 
with results from the Honas searching application. An example is written below.

```
~/honas/scripts/query_generic_blacklist.py -b example_blacklist.txt -s /data/mm-dd-yyyy/yyyy-mm-dd.hs -e possible_entities.csv
```

The example query script above resulted in the JSON file `yyyy-mm-dd.hs.json`.
This JSON file contains all domain names that were positively tested against the 
Bloom filter in the specified state file. Suppose we want to get an overview of
which unique entities are responsible for the DNS queries that were performed.
To do so, we use the `jq` command first, to remove all irrevalant JSON content.
We mangle the output next (with example way to do so), to get output in CSV,
containing all unique organizations including how many unique DNS queries came
from their network.

```
jq .groups[0].hostnames 2018-09-13.hs.json | grep @ | sed 's/  \"//' | sed 's/}//g' | sed 's/{//g' | sed 's/@.*//' | sed 's/\": 1//' | sed 's/,//' | grep -Ev "^$" | sort | uniq -c | sed 's/  //g' | sed -r 's/\s+/,/'
```

We now need to aggregate the information we retrieved using the entity types.
We assume that the entity types are available in a CSV file, used as input for
the `entities_to_sector.py` script. An example command is described below.

```
~/honas/scripts/entities_to_sector.py -r <output_from_jq_command> -m <csv_file_with_entity_types> -w results_sectorized.csv
```

The script above results in a CSV output file containing the cumulative number
of occurrences in the Bloom filter, aggregated per entity type. An example is
given below.

```
sector,count
Type_A,9
Type_B,46
Type_C,31
Type_D,1
Type_E,15
.....
```

More examples are given in the real-world use cases, where actual real-world
problems are discussed in a large network operator.

### The `honas-gather` process                   {#honas_gather}

The `honas-gather` program is responsible for parsing all the host name
lookup logging, collecting the information in a honas state and writing these
states to separate files, one for each time period.

#### Usage

```
Usage: honas-gather [--help] [--config <file>]

  -h|--help           Show this message
  -c|--config <file>  Load config from file instead of /etc/honas/gather.conf
  -q|--quiet          Be more quiet (can be used multiple times)
  -s|--syslog         Log messages to syslog
  -v|--verbose        Be more verbose (can be used multiple times)
  -f|--fork           Fork the process as daemon (syslog must be enabled)
  -a|--aggregate      Aggregates queries by subnet per filter (predefined subnets)
  -d|--dry-run        Performs measurements and gives advice about Bloom filter configuration
```

#### Configuration

The Honas gather process needs a number of configuration options to be
specified before it can start processing host name lookups and write honas
state files.

The default location of the configuration file, for when none is supplied on
the command line, can be changed through a build configuration option.

The supported configuration items, which are all required, are:

- `bloomfilter_path`: The directory where honas state files will be written
- `subnet_activity_path`: The input file used to perform entity-prefix mappings
- `period_length`: The maximum period length for each state file in seconds
- `number_of_bits_per_filter`: How many bits each bloom filter should have
- `number_of_filters`: How many bloom filters should there be per state file. This number should be `1`, as this is how the prototype was extended
- `number_of_hashes`: How many bits should be set per filter per looked up host name
- `number_of_filters_per_user`: How many bloom filters to update for each host name lookup per client

Note: the configuration file is reloaded every `period_length` seconds. Therefore, the Honas gather
process does not have to be restarted to change the Bloom filter parameters.

#### Example configuration

```txt
bloomfilter_path /var/spool/honas/
subnet_activity_path /etc/honas/subnet_definitions.json
period_length 3600
number_of_bits_per_filter 491040000
number_of_filters 1
number_of_filters_per_user 1
number_of_hashes 10
flatten_threshold 1
```

#### The Dry-Run

The Honas prototype uses Bloom filters, which have a fixed size. This size must be
estimated before storing DNS queries. This way, the false positive rate of the
Bloom filters can be controlled. To do so, the extended prototype features a
dry-run mode. This dry-run mode can be enabled using the '-d' switch, and it
will provide a daily advice on suggested Bloom filter parameters, given the
current query stream. It is strongly suggested to perform a dry-run for at
least a week, using a query stream that is representative of the actual use
of the network. An example advice is given below.

```
------------------------------------ Advice ------------------------------------
[10-08-2018 13:32] The numbers are rounded up to the nearest hundred-thousand, and a tolerance of 10 percent is added.
-------------------------------- Hourly Filters --------------------------------
[10-08-2018 13:32] For a false positive rate of 1 / 1000, BF size (m) should be 40810000, based on 2578756 unique domain names
[10-08-2018 13:32] The number of hash functions (k) should be 10
[10-08-2018 13:32] For a false positive rate of 1 / 10000, BF size (m) should be 54450000, based on 2578756 unique domain names
[10-08-2018 13:32] The number of hash functions (k) should be 14
[10-08-2018 13:32] For a false positive rate of 1 / 100000, BF size (m) should be 67980000, based on 2578756 unique domain names
[10-08-2018 13:32] The number of hash functions (k) should be 16
-------------------------------- Daily Filters ---------------------------------
[10-08-2018 13:32] For a false positive rate of 1 / 1000, BF size (m) should be 305250000, based on 19300734 unique domain names
[10-08-2018 13:32] The number of hash functions (k) should be 10
[10-08-2018 13:32] For a false positive rate of 1 / 10000, BF size (m) should be 407000000, based on 19300734 unique domain names
[10-08-2018 13:32] The number of hash functions (k) should be 14
[10-08-2018 13:32] For a false positive rate of 1 / 100000, BF size (m) should be 508750000, based on 19300734 unique domain names
[10-08-2018 13:32] The number of hash functions (k) should be 16
-------------------------------------- End -------------------------------------
```

#### Honas state rotation and merging

The Bloom filters are stored in state files by Honas in the `bloomfilter_path` at
the end of every `period_length`. A set of scripts is shipped with Honas to rotate
and merge these state files, located in the `scripts` directory. The `honas_state_rotate.py`
script looks for state files in the `bloomfilter_path` to rotate, and moves them
to a more structured directory. Furthermore, the `honas_daily_state_combine.py` script
then aggregates (merges) the Bloom filters in the state files. For example, states
that contain DNS queries for one hour can be combined into a state that contains
all queries for that day. It is possible to automate this process using cron. An
example configuration is depicted below.

```
# Honas state rotation and merging scripts. Note the 'cd /data' command in
# front of the 'honas_daily_state_combine' call. This command is required
# because the combine script needs to be executed with the data archive
# directory as working directory.
0 4 * * * ~/honas/scripts/honas_state_rotate.py -v
0 6 * * * cd /data && ~/honas/scripts/honas_daily_state_combine.py
```

Note: Honas state timestamps are handled in UTC. Take a possible time difference into
account when installing the rotation scripts using cron. Furthermore, the scripts are
fairly verbose in their output. This output is written to Syslog by default. Finally,
the daily state combination script requires the working directory to be the data
archive directory, because the `honas-combine` does so.

### The `honas-search` process                   {#honas_search}

The `honas-search` program is responsible for generating a search result based
on a search job and a honas state file.

#### Usage

```
Usage: honas-search [<options>] <state-file>

Options:
  -h|--help           Show this message
  -j|--job <file>     File containing the search job (default: stdin)
  -r|--result <file>  File to which the results will be saved (default: stdout)
  -f|--flatten-threshold <clients>
                      If fewer than this amount of clients have been seen then
                      flatten the results (default: never flatten)
  -q|--quiet          Be more quiet (can be used multiple times)
  -s|--syslog         Log messages to syslog
  -v|--verbose        Be more verbose (can be used multiple times)
```

#### Example

```
$ echo '{"groups":[{"id":1,"hostnames":{"www.java.com":"dfefecff1f2e77bfef84ef74920e77c23c811dd70df0b3b281521814e85c00ee"}}]}' | honas-search '/data/20-07-2018/2018-07-20.hs' | json_pp
{
   "number_of_filters" : 1,
   "number_of_filters_per_user" : 1,
   "flattened_results" : false,
   "number_of_requests" : 175727076,
   "estimated_number_of_clients" : 14287,
   "period_end" : 1532127600,
   "number_of_hashes" : 10,
   "node_version" : "1.0.0",
   "flatten_threshold" : 1,
   "first_request" : 1532041245,
   "estimated_number_of_host_names" : 162956933,
   "groups" : [
      {
         "hits_by_all_hostnames" : 1,
         "id" : 1,
         "hostnames" : {
            "www.java.com" : 1
         }
      }
   ],
   "last_request" : 1532127645,
   "number_of_bits_per_filter" : 491040000,
   "filters" : [
      {
         "actual_false_positive_rate" : "0.0000079791",
         "estimated_number_of_host_names" : 18161591,
         "number_of_bits_set" : 151814229
      }
   ],
   "state_file_version" : "1.0",
   "period_begin" : 1532041245
}
```

Note that the host name key 'www.java.com' was purely chosen for this example
(to show which host name hash was being queried), the actual key values aren't
interpreted by the search process and are simply used verbatim for reporting
possible results. One could also use database id's or simple incremental
indexes, depending on what is easiest to process.

### The `honas-info` program                     {#honas_info}

The `honas-info` program is mainly intended to get some basis information in a
human readable format.

#### Usage

```
Usage: honas-info [<options>] <state-file>

Options:
  -h|--help           Show this message
  -q|--quiet          Be more quiet (can be used multiple times)
  -v|--verbose        Be more verbose (can be used multiple times)
  -p|--plotmode       Output timestamp and number of hostnames as CSV
```

#### Example

```
$ honas-info '2018-07-01.hs'

## Version information ##

Node version      : 1.0.0
State file version: 1.0

## Period information ##

Period begin                  : 2018-07-01T01:00:38
First request                 : 2018-07-01T01:00:38
Last request                  : 2018-07-02T01:00:38
Period end                    : 2018-07-02T01:00:00
Estimated number of clients   : 10163
Estimated number of host names: 128019870 
Number of requests            : 155622232

## Filter configuration ##

Number of filters         : 1
Number of filters per user: 1
Number of hashes          : 10
Number of bits per filter : 491040000
Flatten threshold         : 1

## Filter information ##

 1. Number of bits set:  118252441 (Estimated number of host names:   13528981)
    Fill Rate:        0.2408203833 (False positive probability:   0.0000006560)
```

### The `honas-combine` program                     {#honas_combine}

The `honas-combine` program aggregates the Bloom filters and other parameters
and structures in two Honas state files. Note that the first parameter is also
the destination state for the aggregation.

#### Usage

```
Usage: honas-combine [<options>] <dst-state-file> <src-state-file>

Options:
  -h|--help           Show this message
  -q|--quiet          Be more quiet (can be used multiple times)
  -v|--verbose        Be more verbose (can be used multiple times)
```

Validations                                      {#validations}
-----------

### Unittests                                    {#unit_tests}

Honas comes with unittests implemented using
[check](https://github.com/libcheck/check/). All unittests reside under the
`tests/` directory.

To build and run the unittests, run from inside the meson build directory:

```
ninja test
```

### Static code analysis

There are currently two ways of performing static code analysis.

The first is through the meson supplied 'scan-build' functionality. Run from
inside the meson build directory:

```
ninja scan-build
```

The second is through an additional target that calls 'clang-tidy' on all
sources. Run from inside the meson build directory:

```
ninja clang-tidy-all
```

Possible problems and suggested solutions        {#problems_and_suggestions}
-----------------------------------------

### CPU cache trashing by the bloomfilter        {#cache_trashing}

Updates on large bloom filters probably have very bad locality of reference and
thus lead to many CPU cache misses and consequent invalidations as the relevant
cachelines are loaded into the cache. This not only impacts the performance of
Honas itself, but might also adversely impact the performance of other
programs.

#### Possible solution: Bypassing the CPU cache

If the performance of this program is "good enough" and there is a CPU core to
spare then it might make sense to disable the CPU cache for accesses to the
bloom filter memory. Unfortunately these options, while supported by the Linux
kernel, are not available to user space programs. So this would either need a
patched kernel, a custom kernel driver or some other dirty trickery (possibly
using /dev/mem or similar).

#### Possible solution: Reducing bloom filter updates

Another option might be to add some kind of LRU cache in front of the bloom
filter update process. This should reduce the number of bloom filter update for
frequently requested host names. We'd probably want some form of double LRU
cache to better handle "bursts" of random host name lookup.

### Honas performance                            {#performance}

First indications are that on an i5-4570, with 4 filters of 134217728 bits each
and 2 filter updates per user, Honas can processes more than 30.000 host name
lookups per second. This is expected to be adequate for our use case.

Should it turn out that this is not fast enough then there are some possible
options to consider.

#### Possible solution: Changing the host name hash function

The current algorithm for the host name hashing, SHA-256, is a safe choice.
Using a cryptographic hash function guarantees good spread of hash values,
collision resistance and irreversability.

But, depending on the hardware, calculating SHA-256 might not be very
efficient. Furthermore, the bloom filter offset determination makes multiple
passes over all those bits to make as optimal a spread as possible.

By using another hashing algorithm the hashing itself might be a lot faster.
Any collision resistant hash with a wide spread should be a reasonable choice.

By using a smaller hash value (or perhaps by limiting ourselves to a part of
the hash value) the transformations can be made cheaper.

Before considering these radical steps it's very important to verify that these
parts are the biggest bottlenecks as it's very likely that cache trashing and
main memory access is the culprit. The easiest way to verify is to run
`honas-gather` using `perf record` and check the generated `perf.data` using
`perf report`.

#### Possible solution: Reducing bloom filter updates

Should the cpu usage be due to cache misses, then see above.

Real-world Use Cases				{#real_world_usecases}
-----------------------------------------

### National Detection Network			{#ndnusage}

The National Detection Network is a community run by the NCSC (National Cyber 
Security Centre) in the Netherlands. Security intelligence is shared in this 
community, in the form of Indicators of Compromise (IoC). These IoCs often 
contain domain names, and hence can be tested against the Bloom filters. The
IoCs are shared in a MISP (Malware Information Sharing Platform), which enables
automated blacklist downloading and IoC lookup. However, this requires some 
scripting for Honas as well. Therefore, we built a set of scripts to help with these tasks.

The script `ndn_download_blacklist.py` downloads a domain name blacklist containing 
all domain name attributes in IoCs. This blacklist can then be tested against the 
Bloom filters using the `query_generic_blacklist.py` script we discussed above. The
script `ndn_misp_lookup.py` takes a domain name that was found in a Bloom filter, and 
looks up the associated IoC(s) in the MISP. It provides the threat level and TLP tags 
in the output, to allow security officials to decide more accurately on a follow-up.

Automation access to the MISP requires an authentication key. The MISP URL and required
authentication key are supposed to be present in a Python file called `keys.py`. An example
keys file can be found in `keys.py.example`. Furthermore, the `scripts` directory contains
a series of undocumented scripts. These scripts were part of the development and validation 
process, and are not necessarily required in practice. They solely serve reproducibility of
the research results.

We consider an example. Suppose that we want to obtain an overview of all unique domain names
that were found to be hit in the Bloom filters. These domain names can then be looked up in
the MISP using the lookup script we discussed above. The exampe command below obtains the
overview we want.

```
jq .groups[0].hostnames yyyy-mm-dd.hs.json | grep @ | sed 's/.*@//g' | sed 's/,//g' | sed 's/\": 1//g' | sort | uniq
```

Suppose that we now want to look up all domain names in the MISP, to determine which hits
are possibly serious, and require targeted investigation, or some other follow-up. We use
the following script (for example).

```
for dn in `grep @ yyyy-mm-dd.hs.json | sed 's/ //g' | sed 's/\"//g' | sed 's/:1//g' | sed 's/.*@//g' | sed 's/,//g' | sort | uniq`; do ./ndn_misp_lookup.py -s $dn; done
```

If the lookup was succesful, the output should look like the following. In this example,
we looked up the domain name `malicious.site.example.com`.

```
{
    "search_value": "malicious.site.example.com",
    "event_id": "1337",
    "info": "example IoC description",
    "threat_level": "Low",
    "tlp": "white"
}
```

Based on the IoC information, threat level or TLP (Traffic Light Protocol, https://first.org/tlp/),
we could distinguish which threats are serious enough for a follow-up. The MISP features more
information that is not yet included in the script output. However, this information can easily
be added.

### Booters					{#bootersusage}

Booters are websites where one can buy a DDoS attack. They are also called `web-stressers`.

TODO...

Future Work					{#future_work}
-----------------------------------------

TODO...
