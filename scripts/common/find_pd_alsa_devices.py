#!/usr/bin/env python3
from sys import argv
from subprocess import check_output, STDOUT
from re import search as re_search

def parse_audio_devices(listdev):
	audio_inputs = {}
	audio_outputs = {}

	target = None

	for line in listdev.split('\n'):
		if line == 'audio input devices:':
			target = audio_inputs
			continue
		elif line == 'audio output devices:':
			target = audio_outputs
			continue

		if target != None:
			result = re_search(r'^(\d+)\. (.*)$', line)
			if result:
				target[result.group(2)] = result.group(1)
			else:
				target = None

	return audio_inputs, audio_outputs

listdev_output = check_output(['puredata', '-nogui', '-alsa', '-listdev', '-send', '; pd quit 0'], stderr = STDOUT).decode()

inputs, outputs = parse_audio_devices(listdev_output)

print('-audioindev {0} -audiooutdev {1}'.format(inputs[argv[1]], outputs[argv[1]]))
