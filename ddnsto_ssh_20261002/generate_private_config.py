"""Generate local credentials; generated secrets are never tracked in Git."""
import base64
import json
import secrets
from pathlib import Path

root = Path(__file__).parent
private = root / 'private'
private.mkdir(exist_ok=True)
credentials = private / 'basic-auth.txt'
if credentials.exists():
    login = credentials.read_text(encoding='ascii')
else:
    login = 'f1:' + secrets.token_hex(32)
    credentials.write_text(login, encoding='ascii')
encoded = base64.b64encode(login.encode()).decode()
locations = '''location = /f1-ssh {
    if ($f1_ssh_authorized = 0) { return 401; }
    if ($request_method != GET) { return 405; }
    proxy_pass http://127.0.0.1:18090;
    proxy_http_version 1.1;
    proxy_set_header Upgrade $http_upgrade;
    proxy_set_header Connection "upgrade";
    proxy_set_header Authorization "";
    proxy_set_header Host $host;
    proxy_buffering off;
    proxy_request_buffering off;
    proxy_read_timeout 3600s;
    proxy_send_timeout 3600s;
    access_log off;
    limit_conn f1_ssh_connections 4;
}
'''
config = '''map_hash_bucket_size 128;
map $http_authorization $f1_ssh_authorized {
    default 0;
    "Basic %s" 1;
}
limit_conn_zone $server_name zone=f1_ssh_connections:32k;
server {
    listen 10.0.0.1:18089;
    server_name f1_ssh;
    access_log off;
    location = /health { default_type text/plain; return 200 "f1-ssh-ws-ready\\n"; }
    include /etc/nginx/conf.d/f1-ssh.locations;
    location / { return 404; }
}
''' % encoded
(private / 'f1-ssh.conf').write_text(config, encoding='ascii')
(private / 'f1-ssh.locations').write_text(locations, encoding='ascii')
print('Generated private auth and scoped nginx additions; credential not printed.')
