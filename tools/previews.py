from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
root = Path(__file__).resolve().parents[1]
font = ImageFont.truetype(str(root/'build/NotoSansSC.ttf'), 17)
panels = [('shore','水域侦测'),('bite','咬钩信号'),('fight','目标追踪'),('surge','张力警报'),('caught','捕获报告'),('book','标本档案')]
sheet = Image.new('RGB',(1008,954),'#030a03')
d=ImageDraw.Draw(sheet)
d.text((24,12),'ANGLER / 口袋钓鱼 · 听潮档案 v0.2.0',font=font,fill='#baff89')
for i,(name,label) in enumerate(panels):
    x=24+(i%2)*492;y=50+(i//2)*300
    d.text((x,y),label,font=font,fill='#6ba54d')
    im=Image.open(root/f'build/{name}.ppm').resize((480,270),Image.Resampling.NEAREST)
    sheet.paste(im,(x,y+24))
sheet.save(root/'dist/screens.png')
specimens=Image.new('RGB',(960,648),'#030a03')
for i in range(24):
    im=Image.open(root/f'build/specimen-{i:02}.ppm')
    # The exported rendering is the same source code and 2x sprites used by the device renderer.
    crop=im.crop((0,12,240,120))
    specimens.paste(crop,((i%4)*240,(i//4)*108))
specimens.save(root/'dist/specimens.png')
objects=Image.new('RGB',(960,850),'#030a03')
ImageDraw.Draw(objects).text((18,10),'24 类物品 / 实际绘图代码 · 放大预览',font=font,fill='#baff89')
for i in range(24):objects.paste(Image.open(root/f'build/object-{i:02}.ppm'),((i%4)*240,40+(i//4)*135))
objects.save(root/'dist/objects.png')
lore=Image.new('RGB',(1008,1254),'#030a03')
ld=ImageDraw.Draw(lore)
ld.text((24,12),'废土档案 / 同一固件绘图代码的静态预览',font=font,fill='#baff89')
pages=[f'lore-{i}' for i in range(6)]+['lore-extra-1','lore-extra-2']
for i,name in enumerate(pages):
    x=24+(i%2)*492;y=50+(i//2)*300
    label='鉴定记录' if i<6 else '关联物证' if i==6 else '留存规定'
    ld.text((x,y),label,font=font,fill='#6ba54d')
    lore.paste(Image.open(root/f'build/{name}.ppm').resize((480,270),Image.Resampling.NEAREST),(x,y+24))
lore.save(root/'dist/lore.png')
frames=[Image.open(p).resize((720,405),Image.Resampling.NEAREST) for p in sorted((root/'build').glob('frame-*.ppm'))]
frames[0].save(root/'dist/gameplay.gif',save_all=True,append_images=frames[1:],duration=100,loop=0,optimize=False)
for name in ['screens.png','specimens.png','objects.png','lore.png','gameplay.gif']:
    source=root/'dist'/name
    (root/'dist'/f'{source.stem}-v0.2.0{source.suffix}').write_bytes(source.read_bytes())
print('dist/screens.png, specimens.png, objects.png, lore.png, gameplay.gif generated from firmware renderer')
