#!/usr/bin/python3


import os
import glob
import gzip
import json


REGS = [
	'flags',
	'ax', 'bx', 'cx', 'dx',
	'si', 'di', 'bp', 'sp', 'ip',
	'cs', 'ds', 'es', 'ss'
]



def process_cpudata(cpufile):

	opcodes = []

	try:
		with gzip.GzipFile(cpufile, 'rb') as cpu:
			cpudata = json.load(cpu)

	except gzip.BadGzipFile:
		with open(cpufile, 'r') as cpu:
			cpudata = json.load(cpu)

	#with open((test[:test.index('.json')] if '.json' in test else test).replace(os.path.sep, '-').lower() + ".txt", 'w') as out:

	for opcode, data in cpudata.items():
		if 'reg' in data:
			for reg, data in data['reg'].items():
				status = data['status']
				flags  = int(data['flags-mask']) if 'flags-mask' in data else 65535
				opcodes.append((f"{opcode}.{reg}", status, flags))
		else:
			status = data['status']
			flags  = int(data['flags-mask']) if 'flags-mask' in data else 65535
			opcodes.append((opcode, status, flags))


	return opcodes



def generate_test(path, opcode, status, flags, output):

	test = f"{path}/{opcode}.json.gz"

	try:
		with gzip.GzipFile(test, 'rb') as tjz:
			tests = json.load(tjz)

	except:
		print(f"Failed to open {test} for opcode {opcode}")
		return

	with open(output, 'w') as out:

		print("",  file=out)
		print("#", file=out)
		print(f"# Automatically generated from {test}", file=out)
		print(f"# Opcode: {opcode}", file=out)
		print(f"# Status: {status}", file=out)
		print("#", file=out)
		print("",  file=out)
		print("",  file=out)

		count = 0

		for test in tests:

			print(f"T {opcode}:{count} {test['name']}", file=out)
			print(f"U{flags:04x}", file=out)
			print("R" + " ".join([ f"{x:04x}" for x in [
				(test['initial']['regs'][reg] if reg in test['initial']['regs'] else 0)
					for reg in REGS
				]
			]), file=out)

			for addr, val in test['initial']['ram']:
				print(f"@{addr:#x} {val:#x}", file=out)

			print("X", file=out)

			print("R" + " ".join([ f"{x:04x}" for x in [
				(test['final']['regs'][reg] if reg in test['final']['regs'] else 0)
					for reg in REGS
				]
			]), file=out)

			for addr, val in test['final']['ram']:
				print(f"@{addr:#x} {val:#x}", file=out)

			print("",  file=out)
			count += 1



path    = "ProcessorTests/8088/v1/"
opcodes = process_cpudata(path + "8088.json")


for opcode, status, flags in opcodes:

	if status != 'normal':
		continue;

	print(f"Processing opcode {opcode}...")
	generate_test(path, opcode, status, flags, f"opcode-{opcode}.dat")


