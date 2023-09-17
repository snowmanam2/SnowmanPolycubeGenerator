import sys
import os
import time
import requests
import configparser
import argparse
import subprocess
import json

def get_raw_size(length):
	raw_size_bits = 6 + 5 * (length - 2)
	raw_size_bytes = int(raw_size_bits / 8)

	if raw_size_bits % 8 > 0:
		raw_size_bytes += 1
	
	return raw_size_bytes

def bitface_split_file(infile, outfile, start, count):
	data = None
	length = 0
	with open(infile, 'rb') as f:
		length = int.from_bytes(f.read(1), "little")
		raw_size = get_raw_size(length)
		print('Key byte size = '+str(raw_size))
		print('Found '+str(bitface_get_count(infile, raw_size)) + ' total polycubes')
		
		f.seek(raw_size * start, 1)
		data = f.read(raw_size * count)
		
	with open(outfile, 'wb') as f:
		f.write(length.to_bytes(1, byteorder='little'))
		f.write(data)
		print('Wrote ' + str(len(data)) + ' bytes containing '+str(int(len(data)/raw_size))+' polycubes')
	
	if len(data) == 0:
		return -1
	
	return length

def bitface_get_count(infile, rawsize):
	size = os.path.getsize(infile)
	return int((size - 1) / rawsize)

def get_results(resultfile):
	submission = {}
	results = []
	with open(resultfile, 'r') as f:
		while True:
			l = f.readline().strip('\r\n')
			if not l:
				break
			v = f.readline().strip('\r\n')
			results.append(dict(resultlength=int(l), resultvalue=int(v)))
	
	submission['results'] = results
	
	return submission
	

# Command line argument parsing
parser = argparse.ArgumentParser(description='Job processor for polycube generator')
parser.add_argument('-n','--number', help='Number of jobs to run', default=-1)
parser.add_argument('-s', '--seconds', help='Run jobs for amount of seconds', default=-1)
args = parser.parse_args()

configfilename = 'job_processor.cfg'
config = configparser.ConfigParser()
if os.path.exists(configfilename):
	config.read(configfilename)
else:
	print('Building '+configfilename+'...')
	config.add_section('Job')
	config.add_section('General')
	config.set('Job', 'job_server', 'http://<insert job server name>/<job>/job-tickets')
	config.set('Job', 'job_folder', 'jobs')
	config.set('General', 'threads', '16')
	config.set('General', 'cmd', './polycube_generator')
	config.set('General', 'contributor_name', 'anonymous')
	with open(configfilename, 'w') as configfile:
		config.write(configfile) 
	
	print('Default configuration file `'+configfilename+'` created.')
	print('Fill in fields and run this program again.')
	
	sys.exit(0)

start_run = time.time()

count = 0
jobdir = config.get('Job', 'job_folder')
os.makedirs(jobdir, exist_ok=True)

ticketserver = config.get('Job', 'job_server')
cmd = config.get('General', 'cmd')
threads = config.get('General', 'threads')
contributor = config.get('General', 'contributor_name')
while True:
	print('Contacting ticket server at ' + ticketserver)
	try:
		retval = requests.post(ticketserver)
	except requests.ConnectionError:
		print('Could not reach ticket server')
		break
		
	if retval.status_code == requests.codes.created:
		print('Ticket created.')
	else:
		print('Ticket creation failure.')
		print(retval)
		print(retval.json())
		break
	
	ticketdata = retval.json()
	job = ticketdata['job']
	ticketnumber = ticketdata['ticketid']
	
	with open (os.path.join(jobdir, job+'_ticket'+str(ticketnumber)+'.json'), 'w') as f:
		json.dump(ticketdata, f)
		
	targetlength = ticketdata['targetlength']
	start = ticketdata['seedindex']
	chunk = ticketdata['seedchunk']
	infile = os.path.join(jobdir, job+'_ticket'+str(ticketnumber)+'.dat')
	resultfile = os.path.join(jobdir, job+'_ticket'+str(ticketnumber)+'.presult')
	
	print('Received '+job+' ticket #'+str(ticketnumber)+' for seed index '+str(start))
	
	basefile = os.path.join(jobdir, job+'.dat')
	if not os.path.isfile(basefile):
		dlurl = ticketdata['seedurl']
		print('Downloading seed file from `'+dlurl+"`")
		data = requests.get(dlurl)
		with open(basefile, 'wb') as f:
			f.write(data.content)
	
	filelength = bitface_split_file(basefile, infile, start, chunk)
	
	if filelength > targetlength or filelength < 3:
		print ('Invalid input file')
		break
	
	genargs = [cmd, str(targetlength), '-i', infile, '-r', resultfile, '-t', str(threads)]
	tstart = time.perf_counter()
	
	subprocess.run(genargs)
	
	telapsed = int(time.perf_counter() - tstart)
	
	submission = get_results(resultfile)
	submission['contributor'] = contributor
	submission['ticketid'] = ticketnumber
	submission['token'] = ticketdata['token']
	submission['secondselapsed'] = telapsed
	submission['seedindex'] = start
	
	with open (os.path.join(jobdir, job+'_ticket'+str(ticketnumber)+'_submission.json'), 'w') as f:
		json.dump(submission, f)
	
	print('Submitting results to ticketserver at '+ticketserver)
	retval = requests.put(ticketserver, json=submission)
	if retval.status_code == requests.codes.ok:
		print('Submission accepted.')
	else:
		print('Submission failed.')
		print(retval)
		print(retval.json())
		break
	
	print('---------------------------------------')
	
	count += 1
	
	if count >= args.number and args.number > 0:
		break
		
	if time.time() > (start_run + args.seconds) and args.seconds > 0:
		break
	
	
