"""Independent physical construction checks, including shorts at wire endpoints."""
from pathlib import Path
import argparse,hashlib
from read_lm import Reader
from collections import defaultdict,Counter
import json,math,subprocess
ROOT=Path(__file__).resolve().parents[1]/'examples/perfboard'
def check(name):
 d=json.loads((ROOT/(name+'-Netzliste.json')).read_text(encoding='utf-8'));p=json.loads((ROOT/(name+'.openloch')).read_text(encoding='utf-8'));g=json.loads((ROOT/(name+'-Geometriepruefung.json')).read_text(encoding='utf-8'))
 errors=[]
 for suffix,key in [('.openloch','nativeSHA256'),('.LM4','lm4SHA256')]:
  if hashlib.sha256((ROOT/(name+suffix)).read_bytes()).hexdigest()!=g[key]:errors.append(['geometry report is stale',suffix])
 legacy=Reader((ROOT/(name+'.LM4')).read_bytes()).document(True)
 if legacy['size']!=[p['width'],p['height']]:errors.append(['LM4 board dimensions changed'])
 def physicalPins(n):
  if 'children' in n:return [xy for child in n['children'] for xy in physicalPins(child)]
  if n['type'] in ('TDraht','TDrahtFest','TLeiterbahn') and n['kind'] in (1,9,11,18,19):
   return [n['path'][0]]+([n['path'][-1]] if n['kind']==1 else [])
  return []
 g['lm4Components']=[dict(id=n['id'],feet=physicalPins(n)) for n in legacy['objects'] if 'children' in n and n.get('id')]
 def cacheCheck(n):
  if 'children' in n:count=sum(cacheCheck(c) for c in n['children'])
  elif n['type'] in ('TDraht','TDrahtFest','TLeiterbahn'):count=2 if n['kind']==1 else int(n['kind'] in (9,11,18,19))
  else:count=0
  if len(n['points'])!=count:errors.append(['incomplete legacy terminal cache',n['type'],n.get('id'),count,len(n['points'])])
  if 'inner' in n:cacheCheck(n['inner'])
  return count
 for doc in (legacy,legacy['board']):
  for n in doc['objects']:cacheCheck(n)
  cacheCheck(doc['metadata'])
 touches=defaultdict(set);graph=defaultdict(set);endpads=defaultdict(set);pinmap={};componentNets=defaultdict(list);topends=set()
 for part in d['parts']:
  pts=[]
  for pin,xy in part['pins'].items():
   xy=tuple(map(round,xy));pts.append(tuple(v*254 for v in xy));net=part['nets'][pin]
   if xy in pinmap:errors.append(['duplicate component hole',xy,part['ref']])
   pinmap[xy]=(part['ref'],pin,net)
   if net:componentNets[net].append(xy);endpads[xy].add(net)
  for key in ['components','lm4Components']:
   records=[v for v in g[key] if v['id']==part['ref']]
   if len(records)!=1 or Counter(tuple(v) for v in records[0]['feet'])!=Counter(pts):errors.append(['terminal transform mismatch',key,part['ref']])
 for i,(a,l,t,r,b) in enumerate(d['bodyboxes']):
  if l<1 or t<1 or r*254>p['width'] or b*254>p['height']:errors.append(['body outside board',a])
  for bb,ll,tt,rr,bot in d['bodyboxes'][i+1:]:
   if min(r,rr)>max(l,ll)+.01 and min(b,bot)>max(t,tt)+.01:errors.append(['body collision',a,bb])
 native=[n for n in p['additions'] if n['type']=='wire']
 if len(native)!=len(d['wires']):errors.append(['wire count changed'])
 expectedWires=Counter((tuple((v[0]*254,v[1]*254) for v in (w['start'],w['end'])),not bool(w['start'][2])) for w in d['wires'])
 actualWires=Counter((tuple(tuple(v) for v in w['path']),w['back']) for w in legacy['objects'] if w['type']=='TDraht' and w['kind']==1)
 if expectedWires!=actualWires:errors.append(['LM4 wire coordinates or sides changed'])
 for i,w in enumerate(d['wires']):
  a,b=map(tuple,(w['start'],w['end']));net=w['net'];endpads[a[:2]].add(net);endpads[b[:2]].add(net)
  if a[2]:topends.update((a[:2],b[:2]))
  if a[2]!=b[2] or a[0]!=b[0] and a[1]!=b[1]:errors.append(['non-orthogonal wire',i])
  if i<len(native):
   n=native[i];expected=(a[0]*254,a[1]*254,b[0]*254,b[1]*254,not bool(a[2]))
   if tuple(n[k] for k in ('x','y','x2','y2','back'))!=expected:errors.append(['native wire mismatch',i])
  length=abs(a[0]-b[0])+abs(a[1]-b[1]);previous=None
  for k in range(length+1):
   at=(a[0]+(b[0]-a[0])*k//length,a[1]+(b[1]-a[1])*k//length,a[2]);touches[at].add(net)
   if previous is not None:graph[at].add(previous);graph[previous].add(at)
   previous=at
 for at,nets in touches.items():
  if len(nets)>1:errors.append(['same-side short',at,sorted(nets)])
  if at[:2] in pinmap:
   ref,pin,net=pinmap[at[:2]]
   if nets!={net}:errors.append(['wire touches other component pin',at,ref,pin,sorted(nets),net])
 for at,nets in endpads.items():
  both=set(nets)
  both.update(touches.get(at+(0,),set()))
  if at in topends:both.update(touches.get(at+(1,),set()))
  if len(both)>1:errors.append(['short at soldered wire endpoint',at,sorted(both)])
  elif at in topends:
   a,b=at+(0,),at+(1,);graph[a].add(b);graph[b].add(a)
 for net,terminals in componentNets.items():
  start=terminals[0]+(0,);visited={start};pending=[start]
  while pending:
   for q in graph[pending.pop()]:
    if q not in visited:visited.add(q);pending.append(q)
  missing=[xy for xy in terminals if xy+(0,) not in visited]
  if missing:errors.append(['disconnected net',net,missing])
 report=dict(name=name,components=len(d['parts']),terminals=sum(len(v['pins']) for v in d['parts']),nets=len(componentNets),straightSegments=len(d['wires']),topSegments=sum(w['start'][2]==1 for w in d['wires']),maximumPadDeviationMM=g['maximumPadDeviationMM'],errors=errors,passed=not errors)
 print(json.dumps(report))
 return not errors
if __name__=='__main__':
 import sys
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--directory',type=Path,default=ROOT);args=ap.parse_args();ROOT=args.directory
 sys.exit(0 if all([check('01-Zweitransistor-Blinklicht'),check('02-Zehnkanal-Lauflicht')]) else 1)
