#!/usr/bin/python3


import sys
import re



def get_filters():

	ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')

	def process(line):
		line = ansi_escape.sub('', line).strip()

		if   line[0:6] == "[PASS]": line = line[6:].lstrip()
		elif line[0:6] == "[FAIL]": line = line[6:].lstrip()
		elif line[0:6] == "[WARN]": return ""

		if not ' ' in line:
			return ""

		line = line[0:line.index(' ')].rstrip()

		if not ':' in line:
			return ""

		try:
			int(line[line.index(':')+1:])

		except:
			return ""

		return line

	return set( x for x in [ process(x) for x in sys.stdin.readlines() ] if x != "")



def dump_test(filters, filename):

	valid = False

	with open(filename, 'r') as test:
		for line in test:
			line = line.strip()

			if line == "":
				if valid:
					print()
				valid = False
				continue

			if line[0:2] == "T ":
				if line.split()[1] not in filters:
					continue
				valid = True

			if valid:
				print(line)




filters = get_filters()


for filename in sys.argv[1:]:
	dump_test(filters, filename)


