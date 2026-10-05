"""Authenticated file transfer regression; removes only its own uploaded fixture."""
import argparse
import base64
import hashlib
import json
import urllib.error
import urllib.request
parser=argparse.ArgumentParser()
parser.add_argument('url');parser.add_argument('password');args=parser.parse_args()
auth='Basic '+base64.b64encode(('admin:'+args.password).encode()).decode()
def request(path,body=None,method='GET',authenticated=True,upload=True):
    headers={}
    if authenticated:headers['Authorization']=auth
    if upload:headers['X-F1-Upload']='1'
    r=urllib.request.Request(args.url+path,data=body,method=method,headers=headers)
    try:
        with urllib.request.urlopen(r,timeout=20) as response:return response.status,response.read()
    except urllib.error.HTTPError as e:return e.code,e.read()
assert request('/',authenticated=False)[0]==401
assert request('/')[0]==200
payload=bytes(range(256))*4096
name='f1-web-regression-中文.bin'
path=urllib.parse.quote(name)
assert request('/api/upload/'+path,payload,'PUT')[0]==201
assert request('/api/upload/'+path,payload,'PUT')[0]==409
status,received=request('/files/'+path)
assert status==200 and hashlib.sha256(received).digest()==hashlib.sha256(payload).digest()
assert any(f['name']==name and f['size']==len(payload) for f in json.loads(request('/api/files')[1]))
assert request('/api/upload/%2e%2e%2fsystem',b'x','PUT')[0]==400
assert request('/api/upload/no-header',b'x','PUT',upload=False)[0]==403
print('WEB authentication/1MiB UTF8 upload/hash/download/list/overwrite/traversal/header PASS')
