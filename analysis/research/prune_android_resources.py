"""Remove resources outside the manifest and audited runtime lookup closure; default verifies only."""
from pathlib import Path
import json,re,sys,xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[2]
RES=ROOT/'apk_decoded/res'
LOOKUPS={'anim/noaction':'GameActivity.onCreate getIdentifier',
         'style/UE4SplashTheme':'GameActivity splash getIdentifier',
         'drawable/icon':'LocalNotificationReceiver icon fallback and manifest'}
REFERENCE=re.compile(r'[@?]\+?(?:(?P<package>[\w.]+):)?(?P<type>[a-z-]+)/(?P<name>[\w.]+)')
def references(text):
    return {m['type']+'/'+m['name'] for m in REFERENCE.finditer(text) if m['package'] not in ('android',)}
assert references('@drawable/icon @android:color/black ?android:attr/colorAccent @+id/foo')=={'drawable/icon','id/foo'}
def inventory():
    nodes={};files={}
    for p in RES.rglob('*'):
        if not p.is_file():continue
        if p.parent.name.startswith('values'):
            tree=ET.parse(p);files[p]=tree
            if p.name=='public.xml':continue
            for e in tree.getroot():
                typ=e.get('type',e.tag);name=e.get('name')
                assert name,(p,e.tag)
                key=typ+'/'+name
                refs=references(ET.tostring(e,encoding='unicode'))
                if typ=='style':
                    parent=e.get('parent')
                    if parent and not parent.startswith(('@','android:')):refs.add('style/'+parent)
                    elif parent is None and '.' in name:refs.add('style/'+name.rsplit('.',1)[0])
                nodes.setdefault(key,[]).append((p,e,refs))
        else:
            key=p.parent.name.split('-')[0]+'/'+p.name.split('.')[0]
            refs=references(p.read_text(encoding='utf-8-sig')) if p.suffix=='.xml' else set()
            nodes.setdefault(key,[]).append((p,None,refs))
    return nodes,files
nodes,files=inventory()
roots=references((ROOT/'apk_decoded/AndroidManifest.xml').read_text())|set(LOOKUPS)
assert not any(re.findall(r'0x7f[0-9a-f]{6}',p.read_text()) for p in (ROOT/'apk_decoded/smali').rglob('*.smali')),'New numeric resource reference needs audit'
reached=set();todo=list(roots)
while todo:
    key=todo.pop()
    if key in reached:continue
    assert key in nodes,'Unresolved resource '+key
    reached.add(key)
    for _,_,deps in nodes[key]:todo.extend(deps)
removed=sorted(set(nodes)-reached)
if '--apply' in sys.argv:
    removed_files=[];before=sum(p.stat().st_size for p in RES.rglob('*') if p.is_file())
    for key in removed:
        for p,e,_ in nodes[key]:
            if e is None:p.unlink();removed_files.append(p.relative_to(ROOT).as_posix())
            else:files[p].getroot().remove(e)
    for p,tree in files.items():
        if p.name=='public.xml':
            for e in list(tree.getroot()):
                if e.get('type')+'/'+e.get('name') not in reached:tree.getroot().remove(e)
        if not len(tree.getroot()):p.unlink();removed_files.append(p.relative_to(ROOT).as_posix())
        else:ET.indent(tree,space='    ');tree.write(p,encoding='utf-8',xml_declaration=True)
    # Remove only directories made empty by this exact resource cleanup.
    for p in sorted((p for p in RES.rglob('*') if p.is_dir()),key=lambda p:len(p.parts),reverse=True):
        assert p.resolve().is_relative_to(RES.resolve())
        if not any(p.iterdir()):p.rmdir()
    after=sum(p.stat().st_size for p in RES.rglob('*') if p.is_file())
    (ROOT/'analysis/research/resource_cleanup.json').write_text(json.dumps({'roots':sorted(roots),'dynamic_lookup_evidence':LOOKUPS,'retained':sorted(reached),'removed':removed,'removed_files':sorted(set(removed_files)),'bytes_before':before,'bytes_after':after,'bytes_removed':before-after,'scope':'apk_decoded/res; game assets/native libraries/OBBs unchanged'},indent=2)+'\n')
    assert set(inventory()[0])==reached
else:assert not removed,'Unused resources remain: '+', '.join(removed)
print('PASS:',len(reached),'resources in manifest/runtime closure;',len(removed),'unused resources removed' if '--apply' in sys.argv else 'unused resources')
