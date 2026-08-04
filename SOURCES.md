# Source attribution for render-assets cache

Video and audio assets originate from bilibili.com uploads (the numeric token in
each original filename is the bilibili content id). Approved for commit WITH
attribution by repo owner @Lfan-ke, who takes responsibility. Files here are only
format-processed (transcode / audio-extract / frame-extract); no lyrics or
copyrighted text is reproduced.

Fonts: HarmonyOS Sans family (Huawei, free commercial license, LICENSE per family
kept in fonts/) and JetBrains Mono (SIL OFL-1.1, OFL fetched from the official
JetBrains/JetBrainsMono repo since the source drop shipped no license file).

Point cloud: Stanford Bunny, Stanford Computer Graphics Laboratory 3D Scanning
Repository (https://graphics.stanford.edu/pub/3Dscanrep/bunny.tar.gz).

Images: Honkai Impact 3rd wallpapers from miHoYo official static hosts
(webstatic.mihoyo.com / act-webstatic.mihoyo.com).

## Slug -> original filename mapping

### video/
| slug | original filename | source |
|------|-------------------|--------|
| badapple.mp4    | 【4K_60FPS】(全站最清晰画质_音频修复)Bad_apple!!!坏苹果!!!.505002580.mp4 | bilibili.com (id 505002580) |
| tashouheng.mp4  | 【三无】他守恒【专辑曲】....468124262.mp4 | bilibili.com (id 468124262) |
| rickroll.mp4    | 【官方_MV】Never_Gonna_Give_You_Up_-_Rick_Astley....137649199.mp4 | bilibili.com (id 137649199) |
| luoqixi.mp4     | 洛琪希AI语音模型+推理平台测试,Roxy可爱捏.40059998004.mp4 | bilibili.com (id 40059998004) |
| luoxiaohei.mp4  | 罗小黑.mp4 | bilibili.com |

### subtitles/ (sidecar, slugs match their video)
| slug | original filename |
|------|-------------------|
| badapple.ass    | 【4K_60FPS】...Bad_apple!!!坏苹果!!!.505002580.ass |
| tashouheng.srt  | 【三无】他守恒【专辑曲】....468124262.srt |
| rickroll.json   | 【官方_MV】Never_Gonna_Give_You_Up_-_Rick_Astley....137649199.json |
| luoqixi.txt     | 洛琪希AI语音模型+推理平台测试,Roxy可爱捏.40059998004.txt |

### audio/ (extracted from 音频测试 mp4s)
| slug | original filename | source |
|------|-------------------|--------|
| tashouheng        | 【三无】他守恒【专辑曲】....468124262.mp4 | bilibili.com (id 468124262) |
| dayuhaitang       | 【十二孔陶笛】大鱼海棠(清吹+伴奏版)....169865246.mp4 | bilibili.com (id 169865246) |
| blackblueroom     | 【泽野弘之】治愈系风景_NHK...Black_&_Blue_Room....29745284541.mp4 | bilibili.com (id 29745284541) |
| fuguang           | 【浮光-The_History】...中央音乐学院学生竹笛....1119413045.mp4 | bilibili.com (id 1119413045) |
| miyazakimountain  | 【纯音乐】Miyazaki_Mountain_-_Philter(无损音质).1112258272.mp4 | bilibili.com (id 1112258272) |
| heroreborn        | 【纯音乐】推荐《Hero_Reborn—Shight》....40325220082.mp4 | bilibili.com (id 40325220082) |
| riverflows        | 世界上最温柔的旋律_River_Flows_in_You...119778056.mp4 | bilibili.com (id 119778056) |
| therightpath      | 被营销号毁掉的神曲01——The_Right_Path....965404316.mp4 | bilibili.com (id 965404316) |
| huiyinruguo       | 黄霄雲《回音如果》...【Hi-Res无损】.35236875919.mp4 | bilibili.com (id 35236875919) |
| flowerdance       | 【Flower_Dance】花之舞来啦___.mp4 (added to tmp mid-session, extracted) | bilibili.com |

### images/
| slug | original / source |
|------|-------------------|
| honkai3_base.png       | 图片测试-崩坏三.png (byte-identical to the known mihoyo URL below) |
| honkai3_wall_home.png  | act-webstatic.mihoyo.com puzzle asset (bh3 official) |
| known mihoyo URL       | webstatic.mihoyo.com/upload/contentweb/2022/12/14/1809b567ee823bc741cd0d3e355c5891_3955607419716857496.png (== honkai3_base.png, duplicate dropped) |

### models/
| slug | original filename |
|------|-------------------|
| suzanne.{obj,glb,stl,mtl} | suzanne.{obj,glb,stl,mtl} |
| benchy.stl / benchy.svg   | #3DBenchy - The jolly 3D printing torture-test.{stl,svg} |
| cube.stl                  | generated primitive (unit cube) for slicer analytic golden |

### docs/
| slug | original filename |
|------|-------------------|
| software-doc-spec.pdf | 计算机软件文档编制规范.pdf |

### pointcloud/
| slug | source |
|------|--------|
| bunny.ply         | Stanford bun_zipper.ply reconstruction, 35947 verts |
| bunny_scan000.ply | Stanford bun000.ply raw range scan |
