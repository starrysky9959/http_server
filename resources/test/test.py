'''
Author: starrysky9959 965105951@qq.com
Date: 2022-11-02 17:03:24
LastEditors: starrysky9959 965105951@qq.com
LastEditTime: 2022-11-02 17:07:01
Description: 
'''
from ipaddress import ip_address
import requests
from os.path import dirname, realpath

requests.packages.urllib3.disable_warnings()

test_dir = dirname(realpath(__file__))
ip = "http://localhost/"#"http://10.0.0.1/"


# http 301
# r = requests.get(ip+'index.html', allow_redirects=False)
# assert(r.status_code == 301 and r.headers['Location'] == 'https://10.0.0.1/index.html')

# # https 200 OK
# # r = requests.get('https://10.0.0.1/index.html', verify=False)
# r = requests.get(ip+'index.html', verify=False)
# assert(r.status_code == 200 and open(test_dir + '/../index.html', 'rb').read() == r.content)

# http 200 OK
r = requests.get(ip+'index.html', verify=False)
assert(r.status_code == 200 and open(test_dir + '/../index.html', 'rb').read() == r.content)

# http 404
r = requests.get(ip+'notfound.html', verify=False)
assert(r.status_code == 404)

# file in directory
r = requests.get(ip+'dir/index.html', verify=False)
assert(r.status_code == 200 and open(test_dir + '/../index.html', 'rb').read() == r.content)

# http 206
headers = { 'Range': 'bytes=100-200' }
r = requests.get(ip+'index.html', headers=headers, verify=False)
assert(r.status_code == 206 and open(test_dir + '/../index.html', 'rb').read()[100:201] == r.content)

# http 206
headers = { 'Range': 'bytes=100-' }
r = requests.get(ip+'index.html', headers=headers, verify=False)
assert(r.status_code == 206 and open(test_dir + '/../index.html', 'rb').read()[100:] == r.content)
