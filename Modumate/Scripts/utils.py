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
import win32con
import win32gui
import winreg


def set_env_value(name, value):
	reg_env_key = winreg.OpenKey(winreg.HKEY_CURRENT_USER, 'Environment', 0, winreg.KEY_SET_VALUE)
	winreg.SetValueEx(reg_env_key, name, 0, winreg.REG_SZ, value)
	win32gui.SendMessageTimeout(win32con.HWND_BROADCAST, win32con.WM_SETTINGCHANGE, 0, 'Environment', win32con.SMTO_ABORTIFHUNG, 1000)

def try_decode(value, encoding='utf-8'):
	if isinstance(value, bytes):
		try:
			return value.decode(encoding)
		except UnicodeDecodeError:
			return value
	else:
		return value

def run_p4(*varargs):
	p4_args = list(varargs)

	# Whether to parse the results as straight "k=v" string pairs, rather than marshaled values
	parse_kvp = 'set' in p4_args

	cmd_args = ["p4", "-G"] + p4_args
	p4_pipe = subprocess.Popen(cmd_args, shell=True, stdout=subprocess.PIPE)
	p4_records = []
	p4_kvp_record = {}
	try:
		while True:
			if parse_kvp:
				p4_record_b = p4_pipe.stdout.readline()
				p4_record_s = try_decode(p4_record_b).strip()
				kvp_list = p4_record_s.split('=')
				if len(kvp_list) == 2:
					p4_kvp_record[kvp_list[0]] = kvp_list[1]
				else:
					break
			else:
				p4_record_b = marshal.load(p4_pipe.stdout)
				p4_record_s = {try_decode(k): try_decode(v) for k, v in p4_record_b.items()}
				p4_records.append(p4_record_s)
	except EOFError:
		pass
	p4_pipe.stdout.close()
	rc = p4_pipe.wait()

	if parse_kvp:
		p4_records.append(p4_kvp_record)

	if rc == 0:
		return p4_records
	else:
		raise subprocess.CalledProcessError(rc, " ".join(p4_args), "", str(p4_records))

def update_p4_client():
	print("Detecting Perforce settings...")

	cwd = os.path.abspath(os.path.normpath(sys.path[0]))
	expected_hostname = os.getenv('COMPUTERNAME')

	user_key = 'P4USER'
	p4_user = run_p4('set', '-q', user_key)[0][user_key]

	cur_client_name = None
	cur_client_root = None
	clients = run_p4("clients", "-u", p4_user)
	for client in clients:
		name = client['client']
		root = os.path.normpath(client['Root'])
		host = client['Host']
		try:
			if (os.path.commonpath([cwd, root]) == root) and (host == expected_hostname):
				cur_client_name = name
				cur_client_root = root
				print("Detected current Perforce client: \"%s\", root: \"%s\"" % (name, root))
				break
		except ValueError:
			pass
	
	if cur_client_name and cur_client_root:
		run_p4('set', 'P4CLIENT=%s' % cur_client_name)
	else:
		print("Couldn't find any matching p4 clients.")
		sys.exit(1)

def update_uebp_client():
	client_key = 'P4CLIENT'
	uebp_env_key = "uebp_CLIENT"
	cur_client_name = run_p4('set', '-q', client_key)[0][client_key]
	set_env_value(uebp_env_key, cur_client_name)

def get_client_root(p4_client_name=None):
	p4_args = ['client', '-o']
	if p4_client_name is not None:
		p4_args.append(p4_client_name)
	return os.path.normpath(run_p4(*p4_args)[0]['Root'])

def get_gravatar(user_email):
	clean_email = user_email.strip().lower()
	email_md5 = hashlib.md5(clean_email.encode('utf8')).hexdigest()
	avatar_url = "https://www.gravatar.com/avatar/%s.jpg" % email_md5
	return avatar_url

def get_p4_user_info(p4_user, p4_ticket):
	results = run_p4('-P', p4_ticket, 'user', '-o', p4_user)
	return results[0]

def get_user_shortname(user_fullname):
	names = user_fullname.split(" ")
	shortname = names[0]
	if len(names) > 1:
		shortname += " %s." % names[1][0]
	return shortname

def get_p4_cl_info(p4_cl, p4_ticket):
	results = run_p4('-P', p4_ticket, 'change', '-o', str(p4_cl))
	return results[0]

def get_p4_review_info(review_id, p4_user, p4_ticket):
	review_id = int(review_id)
	auth_pair = "%s:%s" % (p4_user, p4_ticket)
	swarm_auth = base64.encodebytes(auth_pair.encode('ascii')).decode('ascii').replace('\n', '')
	swarm_url_prefix = "https://modumate-swarm.assembla.com"

	review_url = "%s/api/v9/reviews/%d" % (swarm_url_prefix, review_id)
	req = request.Request(review_url, headers={'Authorization': "Basic %s" % swarm_auth})
	resp = request.urlopen(req)
	review_data = json.loads(resp.read())
	return review_data['review']

def send_post(url, data):
	data_str = json.dumps(data)
	data_bin = bytes(data_str, encoding='utf-8')
	req = request.Request(url, data=data_bin)
	req.add_header('Content-Type', 'application/json')
	resp = request.urlopen(req)
	return resp

def send_commit_message(url, p4_cl, p4_ticket):
	p4_cl = int(p4_cl)
	cl_info = get_p4_cl_info(p4_cl, p4_ticket)
	user_id = cl_info['User']
	user_info = get_p4_user_info(user_id, p4_ticket)
	user_fullname = user_info['FullName']
	user_shortname = get_user_shortname(user_fullname)
	swarm_url_prefix = "https://modumate-swarm.assembla.com"

	commit_message_data = {'attachments': [{
		'fallback': "%s submitted change %d" % (user_shortname, p4_cl),
		'color': '#36a64f',
		'author_name': user_fullname,
		'author_link': "%s/users/%s" % (swarm_url_prefix, user_id),
		'author_icon': get_gravatar(user_info['Email']),
		'title': "Submitted change %d" % p4_cl,
		'title_link': "%s/changes/%d" % (swarm_url_prefix, p4_cl),
		'fields': [{
			'title': "Description",
			'value': cl_info['Description'],
			'short': False
		}]
	}]}

	send_post(url, commit_message_data)

def send_review_message(url, review_id, p4_user, p4_ticket):
	review_id = int(review_id)
	review_data = get_p4_review_info(review_id, p4_user, p4_ticket)

	user_id = review_data['author']
	user_info = get_p4_user_info(user_id, p4_ticket)
	user_fullname = user_info['FullName']
	user_shortname = get_user_shortname(user_fullname)
	swarm_url_prefix = "https://modumate-swarm.assembla.com"

	review_message_data = {'attachments': [{
		'fallback': "%s updated Swarm review %d" % (user_shortname, review_id),
		'color': '#36a64f',
		'author_name': user_fullname,
		'author_link': "%s/users/%s" % (swarm_url_prefix, user_id),
		'author_icon': get_gravatar(user_info['Email']),
		'title': "Updated Swarm review %d" % review_id,
		'title_link': "%s/reviews/%d" % (swarm_url_prefix, review_id),
		'fields': [{
			'title': "Description",
			'value': review_data['description'],
			'short': False
		}]
	}]}

	send_post(url, review_message_data)

def send_build_fail_message(url, build_name, build_url, changelog_path, build_cl, review_id, status, p4_user, p4_ticket):
	build_cl = int(build_cl)
	review_id = int(review_id)
	swarm_url_prefix = "https://modumate-swarm.assembla.com"

	# Determine whether the relevant changelist came from the last submitted changelist, or a changelist up for review
	p4_cl = build_cl
	user_id = ""
	change_title = "Last Change: %d" % build_cl
	change_link = "%s/changes/%d" % (swarm_url_prefix, p4_cl)
	description_title = "Change Description"
	description_value = ""

	if status == "shelved":
		review_data = get_p4_review_info(review_id, p4_user, p4_ticket)
		p4_cl = review_data['changes'][0]
		user_id = review_data['author']
		change_title = "Review ID: %d" % review_id
		change_link = "%s/reviews/%d" % (swarm_url_prefix, review_id)
		description_title = "Review Description"
		description_value = review_data['description']

	cl_info = get_p4_cl_info(p4_cl, p4_ticket)

	if status != "shelved":
		user_id = cl_info['User']
		description_value = cl_info['Description']

	user_info = get_p4_user_info(user_id, p4_ticket)
	user_fullname = user_info['FullName']
	user_shortname = get_user_shortname(user_fullname)

	error_summary = "Unknown"
	try:
		log_dir, changelog_name = os.path.split(changelog_path)
		log_path = os.path.join(log_dir, "log")
		error_pat = re.compile(r'error.*:', re.IGNORECASE)
		error_lines = []
		with open(log_path, 'r') as log_file:
			line_num = 0
			for line_str in log_file:
				if error_pat.search(line_str) is not None:
					error_lines.append("Line %d: %s" % (line_num, line_str.strip()))
				line_num += 1

			num_display_errors = 4
			error_summary = '\n'.join(error_lines[:num_display_errors])
	except:
		pass

	fail_message_data = {'attachments': [{
		'fallback': "%s's build %s failed!" % (user_shortname, build_name),
		'color': '#ff0000',
		'author_name': user_fullname,
		'author_link': "%s/users/%s" % (swarm_url_prefix, user_id),
		'author_icon': get_gravatar(user_info['Email']),
		'title': "Build %s failed!" % build_name,
		'title_link': "%s/console" % build_url,
		'fields': [{
			'title': change_title,
			'value': change_link,
			'short': False
		}, {
			'title': description_title,
			'value': description_value,
			'short': False
		}, {
			'title': "Possible Build Errors",
			'value': error_summary,
			'short': False
		}]
	}]}

	send_post(url, fail_message_data)

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

