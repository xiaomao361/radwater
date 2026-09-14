from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
root = Path(__file__).resolve().parents[1]
font = ImageFont.truetype(str(root/'build/NotoSansSC.ttf'), 17)
panels = [('shore','水域侦测'),('bite','咬钩信号'),('fight','短收线'),('surge','张力警报'),('caught','捕获报告'),('book','标本档案')]
sheet = Image.new('RGB',(1008,954),'#1b201d')
d=ImageDraw.Draw(sheet)
d.text((24,12),'RADWATER / 余波 · v0.5.1',font=font,fill='#e9ddbc')
for i,(name,label) in enumerate(panels):
    x=24+(i%2)*492;y=50+(i//2)*300
    d.text((x,y),label,font=font,fill='#a4ab99')
    im=Image.open(root/f'build/{name}.ppm').resize((480,270),Image.Resampling.NEAREST)
    sheet.paste(im,(x,y+24))
sheet.save(root/'dist/screens.png')
specimens=Image.new('RGB',(960,648),'#1b201d')
for i in range(24):
    im=Image.open(root/f'build/specimen-{i:02}.ppm')
    # The exported rendering is the same source code and 2x sprites used by the device renderer.
    crop=im.crop((0,12,240,120))
    specimens.paste(crop,((i%4)*240,(i//4)*108))
specimens.save(root/'dist/specimens.png')
objects=Image.new('RGB',(960,850),'#1b201d')
ImageDraw.Draw(objects).text((18,10),'24 类物品 / 实际绘图代码 · 放大预览',font=font,fill='#e9ddbc')
for i in range(24):objects.paste(Image.open(root/f'build/object-{i:02}.ppm'),((i%4)*240,40+(i//4)*135))
objects.save(root/'dist/objects.png')
lore=Image.new('RGB',(1008,1254),'#1b201d')
ld=ImageDraw.Draw(lore)
ld.text((24,12),'废土档案 / 同一固件绘图代码的静态预览',font=font,fill='#e9ddbc')
pages=[f'lore-{i}' for i in range(6)]+['lore-extra-1','lore-extra-2']
for i,name in enumerate(pages):
    x=24+(i%2)*492;y=50+(i//2)*300
    label='鉴定记录' if i<6 else '关联物证' if i==6 else '留存规定'
    ld.text((x,y),label,font=font,fill='#a4ab99')
    lore.paste(Image.open(root/f'build/{name}.ppm').resize((480,270),Image.Resampling.NEAREST),(x,y+24))
lore.save(root/'dist/lore.png')
features=Image.new('RGB',(1008,1254),'#1b201d')
fd=ImageDraw.Draw(features)
fd.text((24,12),'水边来信 / 事件示意与新玩法 · 同源静态渲染',font=font,fill='#e9ddbc')
feature_pages=[('bay','暮湾：旧码头'),('night','星潭：深水浮标'),('changed-bay','线索改变了岸边'),('event-note','事件留下可回看的记录'),('method-0','苇岸：排水口'),('caught','出水后直接读短文'),('annotation-card-photo','工牌与照片的关联'),('annotation-tape-clock','磁带与怀表的关联')]
for i,(name,label) in enumerate(feature_pages):
    x=24+(i%2)*492;y=50+(i//2)*300
    fd.text((x,y),label,font=font,fill='#a4ab99')
    features.paste(Image.open(root/f'build/{name}.ppm').resize((480,270),Image.Resampling.NEAREST),(x,y+24))
features.save(root/'dist/features.png')
frames=[Image.open(p).resize((720,405),Image.Resampling.NEAREST) for p in sorted((root/'build').glob('frame-*.ppm'))]
frames[0].save(root/'dist/gameplay.gif',save_all=True,append_images=frames[1:],duration=100,loop=0,optimize=False)
fish=Image.new('RGB',(960,580),'#1b201d')
ImageDraw.Draw(fish).text((18,8),'16 种鱼 / 有限的轮廓，各自的故事',font=font,fill='#e9ddbc')
for i in range(16):fish.paste(Image.open(root/f'build/fish-{i:02}.ppm'),((i%4)*240,40+(i//4)*135))
fish.save(root/'dist/fish.png')
for name in ['fish.png','screens.png','specimens.png','objects.png','lore.png','features.png','gameplay.gif']:
    source=root/'dist'/name
    (root/'dist'/f'{source.stem}-v0.5.1{source.suffix}').write_bytes(source.read_bytes())
print('dist/screens.png, specimens.png, objects.png, lore.png, features.png, gameplay.gif generated from firmware renderer')

arrival=[Image.open(root/f'build/arrival-{i:03}.ppm').resize((720,405),Image.Resampling.NEAREST) for i in range(150)]
arrival[0].save(root/'dist/arrival.gif',save_all=True,append_images=arrival[1:],duration=100,loop=0,optimize=False)
opening=Image.new('RGB',(768,944),'#1b201d')
od=ImageDraw.Draw(opening)
od.text((24,10),'坐会儿吧 / v0.5.1 · 240×135 同源渲染',font=font,fill='#e9ddbc')
for i,(frame,label) in enumerate([(0,'岸边的旧帆布椅'),(12,'停一会儿'),(38,'坐在水边')]):
    y=42+i*300
    od.text((24,y),label,font=font,fill='#a4ab99')
    opening.paste(arrival[frame].resize((480,270),Image.Resampling.NEAREST),(144,y+23))
opening.save(root/'dist/arrival.png')
Image.open(root/'build/rest-0.ppm').save(root/'dist/rest-240x135.png')

# Actual renderer outputs, with synthetic saved collections and battery samples.
review=Image.new('RGB',(1008,660),'#1b201d')
rd=ImageDraw.Draw(review);rd.text((24,10),'v0.5.1 / 收藏、阅读与电量 · 同源示例',font=font,fill='#e9ddbc')
for i,(name,label) in enumerate([('book-group','按类收藏 / 同类外观'),('battery-info','电压估算 / 充电说明'),('new-link-event','事件与新批注同时保留'),('size-record','已保存的个人尺寸纪录')]):
    x=24+(i%2)*492;y=48+(i//2)*306
    rd.text((x,y),label,font=font,fill='#a4ab99')
    review.paste(Image.open(root/f'build/{name}.ppm').resize((480,270),Image.Resampling.NEAREST),(x,y+24))
review.save(root/'dist/optimization.png')
