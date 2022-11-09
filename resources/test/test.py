'''
Author: starrysky9959 965105951@qq.com
Date: 2022-11-02 17:03:24
LastEditors: starrysky9959 965105951@qq.com
LastEditTime: 2022-11-05 18:58:16
Description:  
'''
import requests
from os.path import dirname, realpath

requests.packages.urllib3.disable_warnings()

test_dir = dirname(realpath(__file__))
ip = "127.0.0.1" # "10.0.0.1"

# http 301
# r = requests.get('http://{0}/index.html'.format(ip), allow_redirects=False)
# print(r.status_code)
# print(r.headers['Location'])
# assert(r.status_code == 301 and r.headers['Location'] == 'https://{0}/index.html'.format(ip))

# https 200 OK
r = requests.get('https://{0}/index.html'.format(ip), verify=False)
assert(r.status_code == 200 and open(test_dir + '/../index.html', 'rb').read() == r.content)

# http 200 OK
r = requests.get('http://{0}/index.html'.format(ip), verify=False)
assert(r.status_code == 200 and open(test_dir + '/../index.html', 'rb').read() == r.content)

# http 404
r = requests.get('http://{0}/notfound.html'.format(ip), verify=False)
assert(r.status_code == 404)

# file in directory
r = requests.get('http://{0}/dir/index.html'.format(ip), verify=False)
assert(r.status_code == 200 and open(test_dir + '/../index.html', 'rb').read() == r.content)

# http 206
headers = { 'Range': 'bytes=100-200' }
r = requests.get('http://{0}/index.html'.format(ip), headers=headers, verify=False)
assert(r.status_code == 206 and open(test_dir + '/../index.html', 'rb').read()[100:201] == r.content)

# http 206
headers = { 'Range': 'bytes=100-' }
r = requests.get('http://{0}/index.html'.format(ip), headers=headers, verify=False)
assert(r.status_code == 206 and open(test_dir + '/../index.html', 'rb').read()[100:] == r.content)
