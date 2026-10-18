#!/usr/bin/env python3
"""Merge official prod/dev OpenAPI with the APK reverse contract, excluding server administration."""
import argparse, copy, datetime, hashlib, json, pathlib, re, urllib.request
ROOT = pathlib.Path(__file__).resolve().parents[1]
METHODS = {'get', 'post', 'put', 'patch', 'delete', 'head', 'options'}
def excluded(path, op):
    text = path + ' ' + ' '.join(op.get('tags', []))
    return bool(re.search(r'admin|webhook|/q/', text, re.I))
def normalized(path):
    return re.sub(r'\{[^}]+\}', '{}', path)
def filter_spec(spec):
    spec = copy.deepcopy(spec)
    spec['paths'] = {p: {m:o for m,o in v.items() if m in METHODS and not excluded(p,o)} for p,v in spec['paths'].items()}
    spec['paths'] = {p:v for p,v in spec['paths'].items() if v}
    # Only keep schemas reachable from client endpoints; never ship admin/webhook models.
    needed, pending = set(), [spec['paths']]
    schemas = spec.get('components', {}).get('schemas', {})
    while pending:
        obj = pending.pop()
        if isinstance(obj, dict):
            ref = obj.get('$ref', '')
            if ref.startswith('#/components/schemas/') and ref.split('/')[-1] not in needed:
                name = ref.split('/')[-1]; needed.add(name); pending.append(schemas.get(name, {}))
            pending.extend(v for k,v in obj.items() if k != '$ref')
        elif isinstance(obj, list): pending.extend(obj)
    spec['components'] = {'schemas': {k:v for k,v in schemas.items() if k in needed}, 'securitySchemes': spec.get('components',{}).get('securitySchemes',{})}
    return spec

def main():
    p=argparse.ArgumentParser(); p.add_argument('--reverse', type=pathlib.Path, default=pathlib.Path.home()/'monnett-api-reverse/docs/openapi.json'); p.add_argument('--prod', type=pathlib.Path); p.add_argument('--dev', type=pathlib.Path); args=p.parse_args()
    sources={}; provenance={}
    for env in ['prod','dev']:
        url=f'https://api{"-dev" if env == "dev" else ""}.monnettsocial.com/openapi.json'
        raw=getattr(args,env).read_bytes() if getattr(args,env) else urllib.request.urlopen(url,timeout=45).read()
        sources[env]=filter_spec(json.loads(raw)); provenance[env]={'url':url,'sha256':hashlib.sha256(raw).hexdigest()}
    reverse=json.loads(args.reverse.read_text()) if args.reverse.exists() else json.loads((ROOT/'resources/reverse-client.json').read_text())
    reverse=filter_spec(reverse)
    (ROOT/'resources/reverse-client.json').write_text(json.dumps(reverse,ensure_ascii=False,indent=2)+'\n')
    operations=[]
    for env,spec in sources.items():
        for path,verbs in spec['paths'].items():
            for method,operation in verbs.items():
                entry=copy.deepcopy(operation); entry.update(method=method.upper(),path=path,environment=env,source='official',schemas=spec['components']['schemas'])
                operations.append(entry)
        known={(m,normalized(path)) for path,v in spec['paths'].items() for m in v}
        for path,verbs in reverse['paths'].items():
            path='/api'+path if not path.startswith('/api/') else path
            for method,operation in verbs.items():
                if (method,normalized(path)) in known: continue
                entry=copy.deepcopy(operation); entry.update(method=method.upper(),path=path,environment=env,source='reverse',schemas=reverse['components']['schemas'])
                operations.append(entry)
        (ROOT/f'resources/openapi-{env}.json').write_text(json.dumps(spec,ensure_ascii=False,indent=2)+'\n')
    # Shared schema tables avoid repeating the full schema map per endpoint.
    catalogue={'operations':[], 'schemas':{env:s['components']['schemas'] for env,s in sources.items()},'reverseSchemas':reverse['components']['schemas'],'provenance':provenance}
    for entry in operations:
        entry.pop('schemas'); catalogue['operations'].append(entry)
    (ROOT/'resources/catalogue.json').write_text(json.dumps(catalogue,ensure_ascii=False,indent=2)+'\n')
    lines=['# Покрытие клиентского API','', 'Автоматически построено `tools/update_api.py`. Официальный контракт имеет приоритет над реверсом. Admin, webhook и JobRunr исключены.','', '| Среда | Метод | Путь | Источник |','| --- | --- | --- | --- |']
    lines += [f'| {o["environment"]} | {o["method"]} | `{o["path"]}` | {o["source"]} |' for o in operations]
    (ROOT/'docs/API_COVERAGE.md').write_text('\n'.join(lines)+'\n')
    print('Generated',len(operations),'environment/operation pairs')
if __name__=='__main__': main()
