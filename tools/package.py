"""Package app-only firmware. No flashing, mount, or SD modification."""
from pathlib import Path
import hashlib
import json
import re
import shutil
import zipfile

root = Path(__file__).resolve().parents[1]
dist = root / 'dist'
dist.mkdir(exist_ok=True)
source = root / '.pio/build/cardputer-adv/firmware.bin'
raw = source.read_bytes()
assert raw[0] == 0xE9, 'not an ESP app image'
assert len(raw) < 2*1024*1024, 'exceeds checked build partition'
assert int.from_bytes(raw[12:14], 'little') == 9, 'image is not ESP32-S3'
name = 'PocketFishing-ADV-v0.3.0.bin'
shutil.copyfile(source, dist/name)
buildlog = (root/'build/firmware-build.log').read_text()
assert '[SUCCESS]' in buildlog, 'build did not pass'
tests = (root/'build/test-results.txt').read_text()
assert all(marker in tests for marker in ['renderer:', 'journal:', 'compatibility:', 'objects: 24', 'events:', 'annotations:', 'method play:', 'journal CLI: C++ mixed']), 'native/CLI tests did not finish'
flash = re.search(r'Flash:.*used (\d+) bytes', buildlog)
ram = re.search(r'RAM:.*used (\d+) bytes', buildlog)
manifest = {'name':'PocketFishing','version':'0.3.0','theme':'phosphor terminal / Water Echoes (no avatar)','target':'M5Stack Cardputer ADV / ESP32-S3',
    'format':'app-only (M5Launcher)', 'firmware':name, 'bytes':len(raw),
    'sha256':hashlib.sha256(raw).hexdigest(), 'static_ram_bytes':int(ram[1]),
    'linked_flash_bytes':int(flash[1]), 'build_partition_bytes':2097152,
    'sd_journal':'/PocketFishing/catches-v1.pfj','record_bytes':32,
    'fish_parameter_combinations':131072, 'object_parameter_combinations':768, 'object_base_types':24,
    'generator_version':2, 'readable_generator_versions':[1,2], 'object_dossiers':48, 'linked_evidence_pages':24,
    'fish_family_dossiers':8, 'dossier_pages':{'base':3,'with_annotation':4}, 'annotation_pairs':12,
    'ambient_event_types':3, 'event_cooldown_casts':3, 'fishing_methods':['shallow','bottom','deep'],
    'method_selection_retention':'current session; reboot defaults to shallow', 'follow_assist':True,
    'verified':['native tests','static renderer outputs','ESP32-S3 build','app image header and SHA256'],
    'unverified':['device installation','physical keyboard/display/audio','SD round-trip on device','battery life','human gameplay feel']}
(dist/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
(dist/'SHA256SUMS.txt').write_text(f"{manifest['sha256']}  {name}\n")
(dist/'安装说明.txt').write_text('''口袋钓鱼 v0.3.0｜水面回声｜Cardputer ADV

把 PocketFishing-ADV-v0.3.0.bin 复制到已有 FAT32 SD 卡，例如 /Games/。
在 M5Launcher 里找到这个 bin 并安装。它只有应用镜像，不包含启动器或分区表。
不需要额外资源文件。若 Launcher 要求改分区，先核对现有固件和数据，不要直接确认。

物品扩到 24 类轮廓、768 种外观组合；鱼仍有 131072 种外观组合。
48 份物品主档案、24 份互相关联的物证页，拼出净水三号的听潮事件。
原创废土黑色幽默，带轻微异常档案与未知深水气氛，不是官方跨界设定。
新增三种偶发异常：双重倒影、仪表显示 25:13、打捞报告提及明日。
这些只出现在游戏画面，真实保存状态不变；触发后至少间隔三竿。
12 组关联物品凑齐并成功保存后，双方档案会多一页“新增批注”。
已有存档中的组合也会解锁；原三页不改写。无 SD 或保存失败不解锁新批注。
自动跟鱼、空格收放线沿用上一版。

F 循环浅水 / 贴底 / 深水；岸边或结果页可用，钓鱼途中不能换。
浅水鱼更多，贴底旧物更多，深水拉扯稍长、双重倒影更容易出现。
钓法本次开机一直沿用，重启默认浅水。

升级前请在电脑备份 /PocketFishing/catches-v1.pfj（若已有存档）。
本版沿用 v0.2.0 的生成器 v2 和每条 32 字节日志，直接读取旧记录，不迁移或清空。
v0.2.0 可读取本版渔获，但没有新批注和新玩法；更早固件不认识版本 2。
回退到 v0.1.x 会在首条新记录处只读，不能显示后面的渔获。
若需回退，请保留完整新档，使用升级前的副本；不要直接用旧固件继续同一份新档。

空格抛竿；浮漂真正下沉并提示咬钩时，再按空格。
上钩后自动跟鱼，按住空格收线；冲刺或张力高时松手。
不必按 A/D；仍可手动左右微调。提竿窗口略放宽。
张力满会断线，长时间不收线会脱钩。
B 图鉴，A/D 选标本；捕获页或图鉴里 R 读档案，空格翻页，R 返回。
F 钓法；H 帮助；P 暂停；1/2/3 切换钓点；M 声音。
Enter 也可代替空格，逗号/斜杠也可左右控制。
Backspace 返回；钓鱼途中按下会放弃本竿。

存档自动放在 /PocketFishing/catches-v1.pfj。
没有 SD 卡仍可玩，但本局不存档。看到“已保存”再确认图鉴；写失败会明确提示。
本版本已通过电脑测试和固件编译，未在你的 ADV 上安装或实测。
screens.png / specimens.png / objects.png / lore.png / features.png / gameplay.gif 是同一绘图代码的电脑预览，不是实拍。
先验证按键、颜色、中文、成功保存和重启保留，再反馈钓鱼难度。
''')
entries=[name,'manifest.json','SHA256SUMS.txt','安装说明.txt','screens.png','specimens.png','objects.png','lore.png','features.png','gameplay.gif']
archive=dist/'PocketFishing-ADV-v0.3.0.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
    for entry in entries:z.write(dist/entry,entry)
    z.write(root/'docs/FONT-LICENSE.txt','FONT-LICENSE.txt')
    z.write(root/'README.md','开发与玩法说明.md')
    z.write(root/'docs/VALIDATION.md','docs/VALIDATION.md')
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    for entry in entries:assert z.read(entry)==(dist/entry).read_bytes()
print(json.dumps(manifest,ensure_ascii=False,indent=2))
print(archive)
