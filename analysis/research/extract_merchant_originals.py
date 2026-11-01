"""Selected original merchant/treasure evidence only; never changes a game asset."""
from pathlib import Path
import contextlib
import io
import hashlib
import json
import struct

ROOT=Path(__file__).resolve().parents[2]
loader=ROOT/'analysis/research/extract_luis_route_originals.py'
env={'__file__':str(loader),'__name__':'merchant_original_decoder'}
exec(compile(loader.read_text().split('\nroom = ',1)[0],str(loader),'exec'),env)
output=ROOT/'analysis/research/merchant_originals'
output.mkdir(exist_ok=True)
env['output']=output
pak,native_read_original,package=env['pak'],env['read_original'],env['package']
def read_original(name):
    if pak.entries[name]['method']==1:return native_read_original(name)
    first=pak.read_entry(name);assert first==pak.read_entry(name)
    leaf=Path(name).name;(output/leaf).write_bytes(first)
    env['receipt'].append(dict(source=name,output=leaf,sha256=hashlib.sha256(first).hexdigest(),bytes=len(first),
        compressed_sha1=pak.entries[name]['hash'].hex(),repeats=2,
        method=pak.methods[pak.entries[name]['method']] if pak.entries[name]['method'] else 'Stored'))
    return first
stems=('VR4PauseMerchantPlayerPawn_BP','UI_MerchantScreen_BP','UI_MerchantScreen_InventoryFrame_BP',
       'UI_Merchant_GridTable_BP','UI_Merchant_Button_SelectionMenu_BP','UI_Merchant_Button_ContentFrame_BP',
       'UI_MerchantScreen_Button_BP','UI_KeysTreasures_Grab_Button_BP','UI_PauseMenu_Content_KeysTreasures_BP',
       'UI_Treasure_Item_BP','TreasureConfiguration','MerchantStrings','UI_KeysTreasures_Title_BP',
       'UI_KeysTreasures_Handle_BP','MerchantComponent_BP')
parsed={}
for stem in stems:
    matches=[n for n in pak.entries if n.endswith('/'+stem+'.uasset')]
    assert len(matches)==1,(stem,matches)
    asset=read_original(matches[0]);body=read_original(matches[0][:-7]+'.uexp')
    parsed[stem]=package(asset,body)
    (output/(stem+'.package.json')).write_text(json.dumps(parsed[stem],indent=2)+'\n')
pak.f.close();pak.z.close()
import inspect_flat_bp as bp
bp.DIRECTORY=output
class MerchantScript(bp.Script):
    def expression(self,depth=0):
        if self.data[self.at]==0x2d:
            self.take(1)
            return {'literal_token':0x2d}
        # Original execArrayGetByRef +0x6991710/17bc steps array then integer index, with no immediate payload.
        if self.data[self.at]==0x6b:
            self.take(1)
            return {'array_get_by_ref':[self.expression(depth+1),self.expression(depth+1)]}
        return super().expression(depth)
bp.Script=MerchantScript
scripts=[]
for stem,meta in parsed.items():
    body=(output/(stem+'.uexp')).read_bytes()
    for export in meta['exports']:
        if export['class_path']!='/Script/CoreUObject.Function':continue
        data=body[export['uexp_offset']:export['uexp_offset']+export['serial_size']]
        candidates=[(at+4,struct.unpack_from('<I',data,at)[0]) for at in range(4,len(data)-16)
                    if at+4+struct.unpack_from('<I',data,at)[0]+12==len(data) and data[-13]==0x53]
        selected=None
        for offset,count in sorted(candidates,key=lambda v:v[1],reverse=True):
            try:
                with contextlib.redirect_stdout(io.StringIO()):
                    selected=bp.read_script(stem,export['name'],offset)
                break
            except (AssertionError,IndexError,KeyError,struct.error):pass
        assert selected is not None,(stem,export['name'],candidates)
        scripts.append(dict(stem=stem,name=export['name'],offset=selected['script_offset'],
                            bytes=selected['script_bytes'],sha256=export['sha256']))
(output/'receipt.json').write_text(json.dumps(dict(files=env['receipt'],scripts=scripts,
    native_sha256=env['environment']['native_hash'],decoder='original ARM64 Oodle, twice per block',hardware_tested=False),indent=2)+'\n')
print('Selected merchant originals:',len(parsed),'packages,',len(scripts),'exact parsed scripts')
