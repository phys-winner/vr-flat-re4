"""repack.py - copy an APK replacing selected entries (keeps per-entry compression, drops old signature).
usage: python repack.py in.apk out.apk entry=path [entry=path ...]
"""
import sys, zipfile

src, dst = sys.argv[1], sys.argv[2]
repl = dict(a.split('=', 1) for a in sys.argv[3:])
zin = zipfile.ZipFile(src)
zout = zipfile.ZipFile(dst, 'w', allowZip64=True)
seen = set()
for info in zin.infolist():
    if info.filename.startswith('META-INF/'):
        continue
    if info.filename in repl:
        data = open(repl[info.filename], 'rb').read()
        ni = zipfile.ZipInfo(info.filename, date_time=info.date_time)
        ni.compress_type = info.compress_type
        ni.external_attr = info.external_attr
        zout.writestr(ni, data)
        seen.add(info.filename)
        print('replaced', info.filename, len(data))
    else:
        zout.writestr(info, zin.read(info.filename))
for name, p in repl.items():
    if name not in seen:
        data = open(p, 'rb').read()
        ni = zipfile.ZipInfo(name)
        ni.compress_type = zipfile.ZIP_DEFLATED
        zout.writestr(ni, data)
        print('added', name, len(data))
zout.close()
print('wrote', dst)
