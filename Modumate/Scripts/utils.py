import argparse
import base64
import configparser
import json
import hashlib
import marshal
import os
import re
import shlex
import subprocess
import sys
from urllib import request, parse

def try_decode(value, encoding='utf-8'):
	if isinstance(value, bytes):
		try:
			return value.decode(encoding)
		except UnicodeDecodeError:
			return value
	else:
		return value

def get_gravatar(user_email):
	clean_email = user_email.strip().lower()
	email_md5 = hashlib.md5(clean_email.encode('utf8')).hexdigest()
	avatar_url = "https://www.gravatar.com/avatar/%s.jpg" % email_md5
	return avatar_url

def run_process_on_server(address, *varargs):
	process_args = list(varargs)
	arg_str = subprocess.list2cmdline(process_args)
	print("Running process on %s: %s" % (address, arg_str))
	arg_str_quoted = parse.quote(arg_str, safe='')
	process_url = '%s/run?args=%s' % (address , arg_str_quoted)
	req = request.Request(process_url)
	response = request.urlopen(req)
	response_str = response.read().decode('utf-8')
	response_data = json.loads(response_str)
	rc, output = response_data['return_code'], response_data['output']
	print("Process exited with code %d, output:\n%s" % (rc, output))
	sys.exit(rc)

def get_project_version():
	cwd = os.path.abspath(os.path.normpath(sys.path[0]))

	config = configparser.ConfigParser(strict=False)
	config_path = os.path.join(cwd, '../Config/DefaultGame.ini')
	encodings = ['utf-16', 'utf-8']
	for encoding_type in encodings:
		try:
			config.read(config_path, encoding=encoding_type)
			break
		except UnicodeError:
			pass

	project_version = config.get('/Script/EngineSettings.GeneralProjectSettings', 'ProjectVersion')
	print(project_version)

# If utils.py is invoked directly, try to call one of its defined functions
if __name__ == '__main__':
	global_vars = globals()
	if (len(sys.argv) > 1):
		func_name = sys.argv[1]
		if func_name in global_vars and callable(global_vars[func_name]):
			result = global_vars[func_name](*sys.argv[2:])
			if result is not None:
				print(str(result))
		else:
			raise TypeError("Invalid function: %s" % func_name)

