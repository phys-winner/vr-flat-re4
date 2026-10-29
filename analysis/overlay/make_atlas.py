"""make_atlas.py - build the touch-overlay icon atlas (896x896 RGBA, 7x7 cells of 128x128) and emit
overlay_assets.h with the RLE-packed atlas and the SPIR-V shaders. Run from analysis/overlay/.
RLE: repeated records [u16 transparent_run][u16 literal_count][literal_count * RGBA] over the row-major image."""
import struct
from PIL import Image, ImageDraw, ImageFont

CELL, N, SS = 128, 7, 4            # cell size, cells per row, supersampling factor
FONT = 'C:/Windows/Fonts/arialbd.ttf'
W = H = CELL * N
atlas = Image.new('RGBA', (W, H), (0, 0, 0, 0))

def cell_canvas():
    im = Image.new('RGBA', (CELL * SS, CELL * SS), (0, 0, 0, 0))
    return im, ImageDraw.Draw(im)

def circle(d, r, fill=None, outline=None, width=0):
    c = CELL * SS / 2
    d.ellipse([c - r * SS, c - r * SS, c + r * SS, c + r * SS], fill=fill, outline=outline, width=width * SS)

WHITE = (255, 255, 255, 242)
DISC = (18, 23, 26, 135)

def button():
    im, d = cell_canvas()
    circle(d, 57, fill=DISC)
    circle(d, 57, outline=(255, 255, 255, 205), width=2)
    return im, d

def line(d, points, width=5):
    d.line([(x * SS, y * SS) for x, y in points], fill=WHITE, width=width * SS, joint='curve')

def polygon(d, points, fill=WHITE):
    d.polygon([(x * SS, y * SS) for x, y in points], fill=fill)

def box(d, bounds, radius=3, fill=None, outline=WHITE, width=5):
    d.rounded_rectangle([v * SS for v in bounds], radius=radius * SS, fill=fill, outline=outline, width=width * SS)

def put(idx, im):
    im = im.resize((CELL, CELL), Image.LANCZOS)
    atlas.paste(im, ((idx % N) * CELL, (idx // N) * CELL))

# 0 stick base
im, d = cell_canvas(); circle(d, 57, fill=(18, 23, 26, 70)); circle(d, 57, outline=(255, 255, 255, 185), width=2)
circle(d, 29, outline=(255, 255, 255, 50), width=1); put(0, im)
# 1 stick knob
im, d = cell_canvas(); circle(d, 41, fill=(255, 255, 255, 150)); circle(d, 41, outline=(255, 255, 255, 220), width=2); put(1, im)
# 2 interact: raised hand. Legacy A keeps its index, not its controller artwork.
im, d = button()
line(d, [(37,70),(39,59),(47,59),(50,69),(50,37),(56,31),(62,34),(63,58),
         (67,45),(73,45),(77,59),(79,49),(84,49),(89,64),(89,82),(82,94),
         (61,98),(49,89),(42,78),(37,70)], 5)
line(d, [(84,31),(91,24)], 3); line(d, [(92,40),(101,40)], 3)
put(2, im)
# 3 back
im, d = button(); line(d, [(76,37),(49,64),(76,91)], 7); put(3, im)
# 4 inventory: backpack
im, d = button()
d.arc([49*SS,25*SS,79*SS,54*SS], 180, 360, fill=WHITE, width=5*SS)
box(d, (36,42,92,96), 12); box(d, (46,68,82,87), 5, width=4)
line(d, [(44,50),(44,59)], 4); line(d, [(84,50),(84,59)], 4); put(4, im)
# Reuse unused legacy cells for mobile settings, favorites and contextual gestures.
im, d = button()
circle(d, 25, outline=WHITE, width=6); circle(d, 8, outline=WHITE, width=4)
for x,y,xx,yy in ((64,27,64,38),(64,90,64,101),(27,64,38,64),(90,64,101,64),
                  (38,38,46,46),(82,82,90,90),(38,90,46,82),(82,46,90,38)):
    line(d,[(x,y),(xx,yy)],7)
put(5,im)
im,d=button()
polygon(d,[(25,55),(95,55),(104,49),(104,64),(72,68),(63,88),(50,88),(57,68),(25,68)])
line(d,[(46,50),(87,50)],4);line(d,[(79,44),(90,44)],4);put(7,im)
im,d=button()
d.ellipse([40*SS,47*SS,87*SS,99*SS],fill=WHITE)
box(d,(54,33,73,50),3,fill=WHITE,width=0);line(d,[(67,34),(81,34),(89,46)],5)
d.ellipse([79*SS,25*SS,95*SS,41*SS],outline=WHITE,width=3*SS);put(8,im)
im,d=button()
box(d,(40,40,88,92),9,width=4)
box(d,(48,30,80,42),3,fill=WHITE,width=0)
box(d,(58,52,70,81),1,fill=WHITE,width=0);box(d,(49,61,79,73),1,fill=WHITE,width=0);put(13,im)
im,d=button()
d.arc([29*SS,29*SS,99*SS,99*SS],35,310,fill=WHITE,width=6*SS)
polygon(d,[(100,50),(81,49),(90,30)])
line(d,[(52,68),(63,57),(76,70)],5);put(14,im)
# 6 fire: cartridge
im, d = button()
polygon(d, [(64,28),(76,46),(76,86),(52,86),(52,46)])
box(d, (48,88,80,96), 2, fill=WHITE, width=0)
line(d, [(48,72),(40,72)], 3); line(d, [(88,59),(80,59)], 3); put(6, im)
# 9 pause
im, d = button()
box(d, (44,36,58,92), 3, fill=WHITE, width=0)
box(d, (70,36,84,92), 3, fill=WHITE, width=0)
put(9, im)
# 10 aim: sight
im, d = button(); circle(d, 24, outline=WHITE, width=4)
for points in ([(64,28),(64,46)],[(64,82),(64,100)],[(28,64),(46,64)],[(82,64),(100,64)]):
    line(d, points, 5)
circle(d, 3, fill=WHITE); put(10, im)
# 11 pressed highlight ring
im, d = cell_canvas(); circle(d, 57, fill=(255, 255, 255, 35)); circle(d, 57, outline=(255, 255, 255, 255), width=5); put(11, im)
# 12 dot (tap feedback)
im, d = cell_canvas(); circle(d, 13, fill=(255, 255, 255, 180)); circle(d, 13, outline=WHITE, width=2); put(12, im)
# 15 white square (solid fills, e.g. panels)
atlas.paste(Image.new('RGBA', (CELL - 8, CELL - 8), (255, 255, 255, 255)), ((15 % N) * CELL + 4, (15 // N) * CELL + 4))
# 16 knife, 17 pistol
im, d = button()
polygon(d, [(91,28),(86,54),(58,79),(48,69),(76,37)])
line(d, [(42,74),(61,90)], 5)
polygon(d, [(47,77),(55,85),(40,102),(31,94)]); put(16, im)
im, d = button()
polygon(d, [(30,38),(96,38),(99,45),(95,59),(61,59),(64,66),(54,95),
            (39,95),(46,66),(30,63)])
line(d, [(61,61),(72,63),(71,75),(60,77)], 4)
box(d, (35,44,89,48), 1, fill=DISC, outline=None, width=0); put(17, im)
# 18 reload: circular arrow around a magazine
im, d = button()
d.arc([30*SS,30*SS,98*SS,98*SS], 35, 315, fill=WHITE, width=5*SS)
polygon(d, [(100,49),(81,48),(89,30)])
box(d, (52,44,76,86), 3, width=4)
line(d, [(52,51),(76,51)], 3)
for y in (62,72): line(d, [(61,y),(67,y)], 3)
put(18, im)
# 19 answer phone
im, d = button()
polygon(d, [(36,34),(43,30),(60,48),(51,57),(64,73),(76,79),(84,69),
            (101,86),(92,97),(81,99),(67,94),(52,82),(39,64),(31,47),(31,39)])
put(19, im)
# 20 confirm / play
im, d = button(); polygon(d, [(50,35),(94,64),(50,93)]); put(20, im)
# 21 run
im, d = button()
d.ellipse([67*SS,26*SS,83*SS,42*SS], fill=WHITE)
for points in ([(66,48),(60,70)],[(66,49),(84,62),(96,50)],[(64,53),(46,50),(37,62)],
               [(60,70),(79,77),(91,97)],[(61,71),(44,89),(29,88)]): line(d, points, 5)
put(21, im)
# 22 crouch
im, d = button()
d.ellipse([71*SS,35*SS,87*SS,51*SS], fill=WHITE)
for points in ([(72,56),(59,75)],[(71,59),(86,68),(97,65)],
               [(59,75),(78,84),(91,84)],[(59,75),(49,94),(30,94)]): line(d, points, 5)
put(22, im)
# 23 health: watch frame and LCD glyph; the shader draws the true current/max arc.
im, d = cell_canvas()
circle(d, 58, outline=(44,49,49,240), width=5)
circle(d, 57, outline=(192,203,196,245), width=2)
circle(d, 54, outline=(88,103,98,245), width=2)
circle(d, 44, fill=(13,29,31,240))
circle(d, 42, outline=(80,121,117,195), width=1)
line(d, [(31,66),(45,66),(52,55),(60,78),(67,43),(75,66),(96,66)], 3)
put(23, im)
# 24 contextual kick: bent leg and forward boot, rather than a controller letter.
im, d = button()
polygon(d, [(47,28),(61,28),(67,49),(60,63),(86,67),(97,65),(105,70),(104,80),
            (83,82),(49,74),(42,63),(53,47)])
line(d, [(39,86),(29,86)], 3); line(d, [(36,95),(26,95)], 3)
put(24, im)

def figure(d, x):
    # small standing person centred on x (head, body, legs)
    d.ellipse([(x-9)*SS,24*SS,(x+9)*SS,42*SS], fill=WHITE)
    line(d, [(x,46),(x,74)], 7); line(d, [(x,74),(x-10,98)], 6); line(d, [(x,74),(x+10,98)], 6)
    line(d, [(x-13,56),(x+13,56)], 6)
# 25 Ashley: follow (person + forward chevrons)
im, d = button(); figure(d, 48)
line(d, [(70,48),(84,64),(70,80)], 6); line(d, [(84,48),(98,64),(84,80)], 6); put(25, im)
# 26 Ashley: wait (person + pause bars)
im, d = button(); figure(d, 48)
box(d, (72,46,80,84), 2, fill=WHITE, width=0); box(d, (88,46,96,84), 2, fill=WHITE, width=0); put(26, im)
# 27..36 digits, 37 slash: white glyphs with a dark outline, readable over the scene (ammo counter).
glyph_font = ImageFont.truetype(FONT, 118 * SS // 1)
for k, ch in enumerate('0123456789/'):
    im, d = cell_canvas()
    d.text((CELL * SS / 2, CELL * SS / 2), ch, font=glyph_font, fill=(255, 255, 255, 250), anchor='mm',
           stroke_width=7 * SS, stroke_fill=(10, 14, 16, 225))
    put(27 + k, im)
# 38 crank: circular arrow around a hub with a handle
im, d = button()
d.arc([30*SS,30*SS,98*SS,98*SS], 300, 210, fill=WHITE, width=6*SS)
polygon(d, [(36,42),(52,30),(30,24)])
circle(d, 9, fill=WHITE); line(d, [(64,64),(86,86)], 7)
d.ellipse([(86-8)*SS,(86-8)*SS,(86+8)*SS,(86+8)*SS], fill=WHITE); put(38, im)
# 39 lever: base plate, tilted handle with knob, pull arc
im, d = button()
box(d, (34,84,94,96), 3, fill=WHITE, width=0)
line(d, [(64,86),(84,40)], 7); d.ellipse([(84-10)*SS,(40-10)*SS,(84+10)*SS,(40+10)*SS], fill=WHITE)
d.arc([26*SS,30*SS,86*SS,90*SS], 200, 250, fill=WHITE, width=4*SS); put(39, im)
# 40 throw: ballistic arc ending in an arrow head
im, d = button()
d.arc([22*SS,40*SS,106*SS,124*SS], 200, 330, fill=WHITE, width=6*SS)
polygon(d, [(100,58),(84,46),(102,40)])
d.ellipse([22*SS,70*SS,38*SS,86*SS], fill=WHITE); put(40, im)
# 41 skip: fast-forward
im, d = button()
polygon(d, [(32,38),(62,64),(32,90)]); polygon(d, [(60,38),(90,64),(60,90)])
box(d, (92,38,100,90), 2, fill=WHITE, width=0); put(41, im)

# The RLE decoder emits zero RGBA for transparent runs; remove invisible Lanczos RGB.
atlas.paste((0,0,0,0), (0,0,W,H), atlas.getchannel('A').point(lambda a: 0 if a else 255))
atlas.save('atlas_preview.png')

# A labelled inspection sheet; labels are not part of the HUD atlas.
preview = Image.new('RGB', (N * 160, N * 164 + 4), (35, 42, 43))
pd = ImageDraw.Draw(preview)
labels = {0:'СТИК',1:'СТИК',2:'ДЕЙСТВИЕ',3:'НАЗАД',4:'ИНВЕНТАРЬ',6:'ОГОНЬ',9:'ПАУЗА',
          10:'ПРИЦЕЛ',11:'НАЖАТО',12:'КАСАНИЕ',15:'ЗАЛИВКА',16:'НОЖ',17:'ОРУЖИЕ',
          18:'ПЕРЕЗАРЯДКА',19:'ТЕЛЕФОН',20:'ПРОДОЛЖИТЬ',21:'БЕГ',22:'ПРИСЕСТЬ',
          23:'ЗДОРОВЬЕ',24:'ПИНОК',5:'НАСТРОЙКИ',7:'ОРУЖИЕ 2',8:'ГРАНАТА',13:'ЛЕЧЕНИЕ',14:'ДЕЙСТВИЕ',
          25:'ЭШЛИ: ЗА МНОЙ',26:'ЭШЛИ: ЖДАТЬ',37:'/',38:'ЛЕБЁДКА',39:'РЫЧАГ',40:'БРОСОК',41:'ПРОПУСК'}
labels.update({27 + k: str(k) for k in range(10)})
font = ImageFont.truetype(FONT, 13)
for idx in range(N*N):
    x, y = (idx%N)*160, (idx//N)*164
    pd.rectangle((x+5,y+5,x+154,y+158), fill=(47,56,54), outline=(68,77,72))
    icon = atlas.crop(((idx%N)*CELL,(idx//N)*CELL,(idx%N+1)*CELL,(idx//N+1)*CELL))
    preview.paste(icon, (x+16,y+7), icon)
    pd.text((x+80,y+144), '%d · %s' % (idx,labels.get(idx,'ПУСТО')), font=font,
            fill=(215,221,217), anchor='mm')
preview.save('atlas_icons_preview.png')

# ---- RLE pack ----
px = atlas.tobytes()
out = bytearray()
i, npx = 0, W * H
while i < npx:
    run = 0
    while i < npx and px[i * 4 + 3] == 0 and run < 65535:
        run += 1; i += 1
    lit_start = i; lit = 0
    while i < npx and px[i * 4 + 3] != 0 and lit < 65535:
        lit += 1; i += 1
    out += struct.pack('<HH', run, lit) + px[lit_start * 4:(lit_start + lit) * 4]

# Match the renderer's row-major RLE layout, including every transparent cell.
decoded = bytearray(); pos = 0
while pos < len(out):
    run, lit = struct.unpack_from('<HH', out, pos); pos += 4
    assert run or lit
    decoded += bytes(run*4) + out[pos:pos+lit*4]; pos += lit*4
assert pos == len(out) and decoded == px
assert atlas.size == (896,896) and atlas.mode == 'RGBA'
for idx in (5,7,8,13,14,15,23,24) + tuple(range(25, 42)):
    assert atlas.crop(((idx%N)*CELL,(idx//N)*CELL,(idx%N+1)*CELL,(idx//N+1)*CELL)).getbbox() is not None

def c_bytes(name, data):
    lines = []
    for k in range(0, len(data), 24):
        lines.append(''.join('%d,' % b for b in data[k:k + 24]))
    return 'static const unsigned char %s[%d] = {\n%s\n};\n' % (name, len(data), '\n'.join(lines))

def c_words(name, path):
    b = open(path, 'rb').read()
    w = struct.unpack('<%dI' % (len(b) // 4), b)
    lines = []
    for k in range(0, len(w), 8):
        lines.append(''.join('0x%08x,' % x for x in w[k:k + 8]))
    return 'static const unsigned int %s[%d] = {\n%s\n};\n' % (name, len(w), '\n'.join(lines))

cells = ['// generated by make_atlas.py - do not edit', '#ifndef OVERLAY_CELLS_H', '#define OVERLAY_CELLS_H',
       '#define OVL_ATLAS_W %d' % W, '#define OVL_ATLAS_H %d' % H, '#define OVL_CELLS_PER_ROW %d' % N,
       'enum { OVL_STICK_BASE, OVL_STICK_KNOB, OVL_A, OVL_B, OVL_X, OVL_Y, OVL_FIRE, OVL_GRIP_R, OVL_GRIP_L, OVL_MENU,',
       '       OVL_LT, OVL_RING, OVL_DOT, OVL_L3, OVL_R3, OVL_SOLID, OVL_KNIFE, OVL_GUN, OVL_RELOAD,',
       '       OVL_PHONE, OVL_CONFIRM, OVL_RUN, OVL_CROUCH, OVL_HEALTH, OVL_KICK, OVL_ASHLEY_FOLLOW, OVL_ASHLEY_WAIT,',
       '       OVL_DIGIT0, OVL_DIGIT1, OVL_DIGIT2, OVL_DIGIT3, OVL_DIGIT4, OVL_DIGIT5, OVL_DIGIT6, OVL_DIGIT7,',
       '       OVL_DIGIT8, OVL_DIGIT9, OVL_SLASH, OVL_CRANK, OVL_LEVER, OVL_THROW, OVL_SKIP };',
       '#define OVL_INTERACT OVL_A', '#define OVL_BACK OVL_B', '#define OVL_INVENTORY OVL_X',
       '#define OVL_PAUSE OVL_MENU', '#define OVL_AIM OVL_LT', '#define OVL_WEAPON OVL_GUN',
       '#define OVL_PLAY OVL_CONFIRM', '#endif', '']
cells[-2:-2]=['#define OVL_SETTINGS OVL_Y','#define OVL_WEAPON2 OVL_GRIP_R',
              '#define OVL_GRENADE OVL_GRIP_L','#define OVL_HEAL OVL_L3','#define OVL_ACTION_MOTION OVL_R3']
open('overlay_cells.h', 'w').write(chr(10).join(cells))
hdr = ['// generated by make_atlas.py - do not edit', '#include "overlay_cells.h"',
       c_bytes('ovl_atlas_rle', out), c_words('ovl_vert_spv', 'overlay.vert.spv'), c_words('ovl_frag_spv', 'overlay.frag.spv')]
open('overlay_assets.h', 'w').write('\n'.join(hdr))
print('atlas rle bytes:', len(out), 'of raw', W * H * 4)
