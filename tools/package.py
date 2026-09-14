"""Package the validated app-only image. No flashing or SD operations."""
from pathlib import Path
import hashlib
import json
import re
import zipfile

root = Path(__file__).resolve().parents[1]
dist = root / 'dist'
dist.mkdir(exist_ok=True)
version = '0.4.5'
name = f'Radwater-ADV-v{version}.bin'
raw = (root/'.pio/build/cardputer-adv/firmware.bin').read_bytes()
assert raw[0] == 0xE9 and len(raw) < 2097152 and int.from_bytes(raw[12:14], 'little') == 9
# Validate ESP image segments, XOR checksum, and appended SHA256 (not just the outer file hash).
pos = 24
checksum = 0xEF
for _ in range(raw[1]):
    size = int.from_bytes(raw[pos+4:pos+8], 'little')
    pos += 8
    segment = raw[pos:pos+size]
    assert len(segment) == size
    for byte in segment:
        checksum ^= byte
    pos += size
checksum_pos = (pos // 16) * 16 + 15
assert raw[checksum_pos] == checksum
assert raw[23] == 1 and raw[checksum_pos+1:checksum_pos+33] == hashlib.sha256(raw[:checksum_pos+1]).digest()
assert len(raw) == checksum_pos + 33
buildlog = (root/'build/firmware-build.log').read_text()
tests = (root/'build/test-results.txt').read_text()
assert '[SUCCESS]' in buildlog
assert all(marker in tests for marker in ['sound:', 'quiet arrival:', 'renderer:', 'journal:', 'compatibility:', 'objects: 24', 'events:', 'annotations:', 'method play:', 'story edition:', 'notebook:', 'fish silhouettes: 16', 'journal CLI: C++ mixed'])
flash = re.search(r'Flash:.*used (\d+) bytes', buildlog)
ram = re.search(r'RAM:.*used (\d+) bytes', buildlog)
manifest = {
    'name': 'Radwater', 'version': version, 'theme': 'quiet wasteland waterfront / take a seat (no avatar)',
    'target': 'M5Stack Cardputer ADV / ESP32-S3', 'format': 'app-only (M5Launcher)',
    'firmware': name, 'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest(),
    'static_ram_bytes': int(ram[1]), 'linked_flash_bytes': int(flash[1]), 'build_partition_bytes': 2097152,
    'sd_journal': '/PocketFishing/catches-v1.pfj', 'record_bytes': 32,
    'reading_journal': '/PocketFishing/reading-v1.pfn', 'reading_record_bytes': 96, 'recent_events': 12,
    'fish_species': 16, 'fish_dossiers': 16, 'fish_annotations': 16,
    'legacy_fish_parameter_combinations': 131072, 'legacy_species_mapping': 'body | ((ornament & 1) << 3)',
    'object_parameter_combinations': 768, 'object_base_types': 24, 'object_dossiers': 48,
    'linked_evidence_pages': 24, 'annotation_pairs': 12,
    'generator_version': 3, 'readable_generator_versions': [1, 2, 3],
    'downgrade': 'v0.3.0 and earlier stop at first v3 record; preserve new journal and use pre-upgrade copy',
    'dossier_pages': {'fish': 1, 'fish_with_annotation': 2, 'object': 3, 'object_with_annotation': 4},
    'sound': 'procedural 8kHz mono, four 120ms cues, muted by default',
    'sound_pcm_ram_bytes': 3840,
    'quiet_arrival_seconds': 2.6, 'quiet_idle_timeout': None,
    'ambient_event_types': 12, 'event_cooldown_casts': [3, 5],
    'fishing_methods': ['shallow', 'bottom', 'deep'], 'reel_interaction': 'one short release; no A/D tracking',
    'anti_repeat': 'bounded replayable candidate selection; no guarantee',
    'verified': ['ASan/UBSan native tests', 'legacy v1 fixture byte parity', 'mixed v1/v2/v3 C++ and Python checks', 'reading journal failure/reboot tests', 'static renderer outputs', 'ESP32-S3 build', 'image segment checksum and embedded SHA256', 'ZIP contents'],
    'unverified': ['device installation', 'physical keyboard/display/audio', 'SD round-trip on device', 'battery life', 'human gameplay feel']
}
# Never silently replace an existing release image with different bytes.
target = dist/name
if target.exists():
    assert target.read_bytes() == raw, 'versioned firmware already exists with different bytes; choose a new version'
else:
    target.write_bytes(raw)
(dist/'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n')
(dist/'SHA256SUMS.txt').write_text(f"{manifest['sha256']}  {name}\n")
(dist/'安装说明.txt').write_text(f'''余波 Radwater v{version}｜水边来信｜Cardputer ADV

将 {name} 放入已有 FAT32 SD 卡，在已有 M5Launcher 中选中安装。
这是 app-only 镜像，不包含 bootloader 或分区表；不需要额外资源文件。

改名前后的存档路径保持 /PocketFishing/，v0.4.0与本版互相兼容。
升级前备份 /PocketFishing/catches-v1.pfj。
旧收藏直接读取、原样保留；鱼按16种归类展示，数量变小不等于原始记录被删除。
新渔获使用生成器v3，v0.3.0及更早固件不认识，会在首条v3处停止读取。
如需回退，请保留完整新档并使用升级前副本；不要用旧工具修复新版日志。

开机先看一把摆好的旧帆布椅，随后视角坐低，总共约2.6秒；空格/Enter随时直接抛竿，其他快捷键也立即响应。
“坐会儿吧。”淡掉后可一直坐着，没有倒计时或奖励；H查看操作。
空格抛竿，咬钩再按一次。按住收线，看到挣扎松一下，再按住。
新增落水、咬钩、鱼/旧物上岸短音效。默认静音，M开启或立即静音；无额外素材。
不用A/D追鱼。持续按住仍会断线；来不及提竿会暂停，P继续。
R读档案，B收藏，U找未读，T关联物品，C继续上次阅读。
N看手记与最近12条随机事件；水下敲门后可在结果页E回应，也可忽略。
F切换钓法，1/2/3水域，H帮助，M声音，Backspace返回。

16种鱼及独立档案，物品仍是24类768外观组合，原有物品正文保留。
12类事件中，排水影响后三竿、鱼群交班加快后两竿；按竿数计算。
灰绿水面、沙黄暮色、锈色设施与纸面档案，无角色或小猫。

收藏：/PocketFishing/catches-v1.pfj，每条32字节。
手记：/PocketFishing/reading-v1.pfn，每次96字节快照。
没有SD时可玩但不保留；两个文件独立显示保存失败，不自动截断原档。
C恢复已保存的阅读页，不恢复关机前的收线进度。

已通过电脑逻辑/存储测试、固件编译及镜像校验，尚未安装到你的设备验收。
先试空格手感、小字与颜色，再检查一次保存和重启保留。
附带图片和GIF均为同源电脑渲染，不是真机实拍。
更多说明见开发与玩法说明.md。
''')
entries = [name, 'manifest.json', 'SHA256SUMS.txt', '安装说明.txt', 'screens.png', 'fish.png', 'specimens.png', 'objects.png', 'lore.png', 'features.png', 'gameplay.gif', 'arrival.png', 'arrival.gif', 'rest-240x135.png']
archive = dist/f'Radwater-ADV-v{version}.zip'
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
    for entry in entries:
        z.write(dist/entry, entry)
    z.write(root/'docs/FONT-LICENSE.txt', 'FONT-LICENSE.txt')
    z.write(root/'README.md', '开发与玩法说明.md')
    z.write(root/'docs/VALIDATION.md', 'docs/VALIDATION.md')
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    for entry in entries:
        assert z.read(entry) == (dist/entry).read_bytes()
print(json.dumps(manifest, ensure_ascii=False, indent=2))
print(archive)
