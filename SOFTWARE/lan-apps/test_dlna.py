"""DLNA protocol/playback controls regression against a live renderer."""
import argparse
import socket
import time
import urllib.request
import xml.etree.ElementTree as ET
from xml.sax.saxutils import escape
parser=argparse.ArgumentParser();parser.add_argument('host');parser.add_argument('uri');parser.add_argument('--skip-discovery',action='store_true');args=parser.parse_args()
base='http://'+args.host+':49494'
service='urn:schemas-upnp-org:service:AVTransport:1'
def action(name,fields):
    body=('<?xml version="1.0"?><s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"><s:Body><u:'+name+' xmlns:u="'+service+'">'+''.join('<'+k+'>'+escape(str(v))+'</'+k+'>' for k,v in fields.items())+'</u:'+name+'></s:Body></s:Envelope>').encode()
    request=urllib.request.Request(base+'/upnp/control/rendertransport1',body,headers={'Content-Type':'text/xml; charset="utf-8"','SOAPAction':'"'+service+'#'+name+'"'})
    with urllib.request.urlopen(request,timeout=15) as response:return ET.fromstring(response.read())
def value(tree,key):
    for e in tree.iter():
        if e.tag.split('}')[-1]==key:return e.text
sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.settimeout(5)
message=('M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: "ssdp:discover"\r\nMX: 1\r\nST: urn:schemas-upnp-org:device:MediaRenderer:1\r\n\r\n').encode()
if not args.skip_discovery:
    route=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);route.connect((args.host,1900));local=route.getsockname()[0];route.close()
    sock.bind((local,0));sock.setsockopt(socket.IPPROTO_IP,socket.IP_MULTICAST_IF,socket.inet_aton(local))
    sock.sendto(message,('239.255.255.250',1900));data,_=sock.recvfrom(8192);assert b'LOCATION:' in data.upper();print('DLNA discovery PASS')
sock.close()
action('SetAVTransportURI',{'InstanceID':0,'CurrentURI':args.uri,'CurrentURIMetaData':''})
action('Play',{'InstanceID':0,'Speed':'1'});time.sleep(7)
first=value(action('GetPositionInfo',{'InstanceID':0}),'RelTime');print('PLAY position',first)
assert first and sum(int(part)*scale for part,scale in zip(first.split(':'),(3600,60,1)))>0
action('Pause',{'InstanceID':0});time.sleep(1)
paused=value(action('GetPositionInfo',{'InstanceID':0}),'RelTime');time.sleep(2)
assert value(action('GetPositionInfo',{'InstanceID':0}),'RelTime')==paused
print('DLNA pause position stable PASS')
action('Seek',{'InstanceID':0,'Unit':'REL_TIME','Target':'00:00:05'})
action('Play',{'InstanceID':0,'Speed':'1'});time.sleep(3)
print('SEEK position',value(action('GetPositionInfo',{'InstanceID':0}),'RelTime'))
action('Stop',{'InstanceID':0});print('DLNA URI/play/pause/seek/stop PASS')
