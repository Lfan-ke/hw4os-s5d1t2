# render-assets golden MANIFEST

Host-side multimedia test-asset cache for StarryOS render/codec/multimedia carpets. 
Generated with ffmpeg 6.1.1 (Ubuntu). All sha256 are over file bytes unless noted `pcm_sha256`/`rgb24` (decoded raw).

## Encoders available
libx264, libx265 (HEVC), libvpx-vp9, ffv1 (lossless), aac, flac, libopus, libwebp, librsvg (SVG raster in ffmpeg).

## fonts/ (54 ttf flattened, dir-prefixed)

HarmonyOS Sans (48 across 8 subfamilies) + 3 loose full-CJK SC + JetBrains Mono 3 = 54. Licenses: 9 (8 HarmonyOS identical + 1 JetBrains OFL).

<details><summary>font sha256</summary>

- `fonts/HarmonyOS_Sans_Condensed_Italic__HarmonyOS_Sans_Condensed_Black_Italic.ttf`  4227b1c8c72286f17d2b05312b21d1c882bf51e330ced81143a148f2ba9a1de9
- `fonts/HarmonyOS_Sans_Condensed_Italic__HarmonyOS_Sans_Condensed_Bold_Italic.ttf`  286307c47de5bbc28b33002bfe5c2d24af3c476d228e84037fd66d4cfc700e3c
- `fonts/HarmonyOS_Sans_Condensed_Italic__HarmonyOS_Sans_Condensed_Light_Italic.ttf`  85aace7b39dbd2a2f26ba86fa8a9959dd56f874e75bd70f2f2645b891b2df0c6
- `fonts/HarmonyOS_Sans_Condensed_Italic__HarmonyOS_Sans_Condensed_Medium_Italic.ttf`  5913a16b155bb42cde9c88bc96639ce43f843eb4322d386c911863627aedef70
- `fonts/HarmonyOS_Sans_Condensed_Italic__HarmonyOS_Sans_Condensed_Regular_Italic.ttf`  ec61739bd2e3e0ed233c4aed98ac604584018017a5f85dc116b8cf6e064c16d4
- `fonts/HarmonyOS_Sans_Condensed_Italic__HarmonyOS_Sans_Condensed_Thin_Italic.ttf`  5a700626d25162999defd99008ca7164433c8f0992b519f315bb9a94de05bd7f
- `fonts/HarmonyOS_Sans_Condensed__HarmonyOS_Sans_Condensed_Black.ttf`  0e2c83fc6b9731470cda9b132f769da5c5459545b63c573385c70637300baceb
- `fonts/HarmonyOS_Sans_Condensed__HarmonyOS_Sans_Condensed_Bold.ttf`  f4cab6822128e2fde6ea91751b9af4407001f01ca6235451ee6c968aa47ff022
- `fonts/HarmonyOS_Sans_Condensed__HarmonyOS_Sans_Condensed_Light.ttf`  f1e77102835485a444589d9ad73c248ba9d11e9240fd007e26771912d8341eeb
- `fonts/HarmonyOS_Sans_Condensed__HarmonyOS_Sans_Condensed_Medium.ttf`  49c3695ffa2924175a9bfe995691559aecd15697587c109b35c2ca66183e93ad
- `fonts/HarmonyOS_Sans_Condensed__HarmonyOS_Sans_Condensed_Regular.ttf`  ef6accddbc7c494c87f5d264cfbf1b4d7def914d115b94528ffbe71f5cc7bf80
- `fonts/HarmonyOS_Sans_Condensed__HarmonyOS_Sans_Condensed_Thin.ttf`  f123028c19c96c5e59a5075e4095aec60077b5ee00b7060c4bc0c2274aa12eac
- `fonts/HarmonyOS_Sans_Italic__HarmonyOS_Sans_Black_Italic.ttf`  8559e332a72b7fe68496056b36584d75560585505b1ce69d7d2ae78f4ca99a3b
- `fonts/HarmonyOS_Sans_Italic__HarmonyOS_Sans_Bold_Italic.ttf`  05eccf749d6602d1832a5214d1b704c0b3010497580cdbe7fc2a153558856153
- `fonts/HarmonyOS_Sans_Italic__HarmonyOS_Sans_Light_Italic.ttf`  032dcc9c42ab19f4e4eeaef06fe10ee7fab552138a7bf16d607dabda78850d7e
- `fonts/HarmonyOS_Sans_Italic__HarmonyOS_Sans_Medium_Italic.ttf`  8035933cb913dd714de3cdcb74044477f9c2ddc315ccea9f81ec1e7f593ae095
- `fonts/HarmonyOS_Sans_Italic__HarmonyOS_Sans_Regular_Italic.ttf`  3c90775b60f11328c3a84d7ba75443a667dee7458cef765ceb0fa10badad3c7e
- `fonts/HarmonyOS_Sans_Italic__HarmonyOS_Sans_Thin_Italic.ttf`  156338dc0ca9d908a95f58cd9eb489314a0dfc669f618f5005a5b4044101eae5
- `fonts/HarmonyOS_Sans_Naskh_Arabic_UI__HarmonyOS_Sans_Naskh_Arabic_UI_Black.ttf`  a5bae9cf6a8b970945a71a54c3b4ef557309e262a74ca9cf10306eaa5cee2072
- `fonts/HarmonyOS_Sans_Naskh_Arabic_UI__HarmonyOS_Sans_Naskh_Arabic_UI_Bold.ttf`  d46bd2d709d514c1538a3ce60902854406314f0877ba3d044a051d81ff85ace7
- `fonts/HarmonyOS_Sans_Naskh_Arabic_UI__HarmonyOS_Sans_Naskh_Arabic_UI_Light.ttf`  505a2ef366ce4c7ebd67b79ea273fb46ed3fe4bdbdf43c7e362730f0bf94803e
- `fonts/HarmonyOS_Sans_Naskh_Arabic_UI__HarmonyOS_Sans_Naskh_Arabic_UI_Medium.ttf`  71d65225d41b829830d0c7a7d7299c831765a18cf83d68320966fa1528a55669
- `fonts/HarmonyOS_Sans_Naskh_Arabic_UI__HarmonyOS_Sans_Naskh_Arabic_UI_Regular.ttf`  f21d33cd0a49fac9c37eba7adc43ca9a8bb5f9191a8d7ba2a954663d6fba4da9
- `fonts/HarmonyOS_Sans_Naskh_Arabic_UI__HarmonyOS_Sans_Naskh_Arabic_UI_Thin.ttf`  a53bfcb57c1441b8519b1d06109db8ec400d66246a49c1925c9432bb1806aeb2
- `fonts/HarmonyOS_Sans_Naskh_Arabic__HarmonyOS_Sans_Naskh_Arabic_Black.ttf`  eaf20098b8dbb617e07e067f216c84eee7e6ba6ec6b31e42895a75e4d0b25c63
- `fonts/HarmonyOS_Sans_Naskh_Arabic__HarmonyOS_Sans_Naskh_Arabic_Bold.ttf`  5b97ebd4d73dc3c0c23685340d4b96332ea4d942137515f5e0bfb542b75c139e
- `fonts/HarmonyOS_Sans_Naskh_Arabic__HarmonyOS_Sans_Naskh_Arabic_Light.ttf`  98274dc8d6014de9c203faf64c7332cf7b7b4cce16626dfc813707fc0fb90304
- `fonts/HarmonyOS_Sans_Naskh_Arabic__HarmonyOS_Sans_Naskh_Arabic_Medium.ttf`  a8c9a03590c817b535d97c0b8c4557d17fa7961ac39c8c57f9238308190a92e1
- `fonts/HarmonyOS_Sans_Naskh_Arabic__HarmonyOS_Sans_Naskh_Arabic_Regular.ttf`  227af807198673c235e91c28ff16bd995cad6d94af0e9099b6ae0b3462f9fcdc
- `fonts/HarmonyOS_Sans_Naskh_Arabic__HarmonyOS_Sans_Naskh_Arabic_Thin.ttf`  38f0c1e9cc76f03b0da1cdfb646150ab9e934a5331e907322d1730a89d0a3d52
- `fonts/HarmonyOS_Sans_SC__HarmonyOS_Sans_SC_Black.ttf`  5de9560908e88820df2e0a5ed9465bc44644ae2ce1cd6c194b76f2ed8e8f186e
- `fonts/HarmonyOS_Sans_SC__HarmonyOS_Sans_SC_Bold.ttf`  43a424b85e47fb53a17b3b32026a71801f86f8e022ca6798d186b47d39fa5f01
- `fonts/HarmonyOS_Sans_SC__HarmonyOS_Sans_SC_Light.ttf`  dd366290b40861bc6ced85801e850ab66d6fe4c5b33bc43095a9747fa29288d8
- `fonts/HarmonyOS_Sans_SC__HarmonyOS_Sans_SC_Medium.ttf`  6ed1553edccddc48eb27ff25d134a4a715cf54211238d4840b3038576cba1944
- `fonts/HarmonyOS_Sans_SC__HarmonyOS_Sans_SC_Regular.ttf`  297b088424be212207df2ce8b98e335468b782aa6b96832af0b8b773d711e2b1
- `fonts/HarmonyOS_Sans_SC__HarmonyOS_Sans_SC_Thin.ttf`  a88d9065dc144bf462e6553e308e9d5256bfe1a068abb47a3ee1081d636ea91a
- `fonts/HarmonyOS_Sans_TC__HarmonyOS_Sans_TC_Black.ttf`  7d9a922077812a1b6ad39116e80ac283219d27ddafcc1da245e7679a2463c621
- `fonts/HarmonyOS_Sans_TC__HarmonyOS_Sans_TC_Bold.ttf`  6382485ee421f54d87d0cdbd63e7028b5b1a2988fbdc7a6a6d233158c511994e
- `fonts/HarmonyOS_Sans_TC__HarmonyOS_Sans_TC_Light.ttf`  9b3501b7b5ad676852efc81a2833ca2293f708f54c8028d4d3267e9e4c3d06d7
- `fonts/HarmonyOS_Sans_TC__HarmonyOS_Sans_TC_Medium.ttf`  ef094d04ee84d6c3711204eaf053d00a38a0de60f4ca35ee909aed80bd8baeb9
- `fonts/HarmonyOS_Sans_TC__HarmonyOS_Sans_TC_Regular.ttf`  b63084477c1b5cd0db3b88fdba581ae0f05c947445f47d03810f1189544cd5a0
- `fonts/HarmonyOS_Sans_TC__HarmonyOS_Sans_TC_Thin.ttf`  97e61257b9e7700ca160b06d67ce769187c63b48eb34aa0b7bb0b18028d3f3a5
- `fonts/HarmonyOS_Sans__HarmonyOS_Sans_Black.ttf`  ef643b37b20c21d01edb5722934cff45ae885f0b03ca1864386463e12fb03e22
- `fonts/HarmonyOS_Sans__HarmonyOS_Sans_Bold.ttf`  7f973862c42353c9cc372dc2ae891d12c9ea5fe2a01b449adaf1eade9b469b47
- `fonts/HarmonyOS_Sans__HarmonyOS_Sans_Light.ttf`  e63124785efe56484d5ff09eb03ff77db940dcc55cf1c8e98da1aa6dbdf03147
- `fonts/HarmonyOS_Sans__HarmonyOS_Sans_Medium.ttf`  f6b009d07d8d894d55eadeb7080b4916c3a2c83ff3ee60bbe851e6698d73bafd
- `fonts/HarmonyOS_Sans__HarmonyOS_Sans_Regular.ttf`  4f00c7e80329238d0b6fc58e5c829c4086432ba9fa1a8c5ca3da9a0442ce0452
- `fonts/HarmonyOS_Sans__HarmonyOS_Sans_Thin.ttf`  d282f31c5c160915cdb10d228119c35d6eb640f6fdac9960e9891272bdb4adf4
- `fonts/root__HarmonyOS_Sans_SC_Bold.ttf`  9d12e16828320b8aac3102d8a2040800994a1efb504f2312d129c7ea9d1c68ef
- `fonts/root__HarmonyOS_Sans_SC_Light.ttf`  dd366290b40861bc6ced85801e850ab66d6fe4c5b33bc43095a9747fa29288d8
- `fonts/root__HarmonyOS_Sans_SC_Regular.ttf`  b8485d72ec40b9030ee19b10a5d24a14df446e9b4d9b7ebd54bfd5f374161f79
- `fonts/root__JetBrainsMono-Bold.ttf`  d22c4f3821d725eb01210d278d95dfcfcaadc34699a06658d47c8a5cc5830ada
- `fonts/root__JetBrainsMono-Medium.ttf`  d16e6dc99672734698d629705f617c79f6eb6040f5113efe3a145204dc988109
- `fonts/root__JetBrainsMono-Regular.ttf`  e6fd0d7e91550b3ed2b735d4312474362c4716edc4fc0577a0f61ed782d5aed1

Licenses:
- `fonts/LICENSE__HarmonyOS_Sans.txt`  b2ffec0e6269ee41c3b5fc0345ab37600b46d66ebea6c9c58ff37f517bdfa164
- `fonts/LICENSE__HarmonyOS_Sans_Condensed.txt`  b2ffec0e6269ee41c3b5fc0345ab37600b46d66ebea6c9c58ff37f517bdfa164
- `fonts/LICENSE__HarmonyOS_Sans_Condensed_Italic.txt`  b2ffec0e6269ee41c3b5fc0345ab37600b46d66ebea6c9c58ff37f517bdfa164
- `fonts/LICENSE__HarmonyOS_Sans_Italic.txt`  b2ffec0e6269ee41c3b5fc0345ab37600b46d66ebea6c9c58ff37f517bdfa164
- `fonts/LICENSE__HarmonyOS_Sans_Naskh_Arabic.txt`  b2ffec0e6269ee41c3b5fc0345ab37600b46d66ebea6c9c58ff37f517bdfa164
- `fonts/LICENSE__HarmonyOS_Sans_Naskh_Arabic_UI.txt`  b2ffec0e6269ee41c3b5fc0345ab37600b46d66ebea6c9c58ff37f517bdfa164
- `fonts/LICENSE__HarmonyOS_Sans_SC.txt`  b2ffec0e6269ee41c3b5fc0345ab37600b46d66ebea6c9c58ff37f517bdfa164
- `fonts/LICENSE__HarmonyOS_Sans_TC.txt`  b2ffec0e6269ee41c3b5fc0345ab37600b46d66ebea6c9c58ff37f517bdfa164
- `fonts/LICENSE__JetBrainsMono-OFL.txt`  a76abf002c49097d146e86740a3105a5d00450b1592e820a1109a8c5680cd697
</details>

## images/ (raster + vector zoo)
Base `honkai3_base.png` 1920x1080 RGBA; `fmt_ref.png` 640x360 conversion source.

| file | format | sha256 |
|---|---|---|
| images/honkai3_base.png | PNG 1920x1080 | 4d90312ae7154f0dccb87a59533b130e1bd3c16853290579e8e487589b570d0e |
| images/honkai3_wall_home.png | PNG 1024x1024 mihoyo | b5565c62521b77327905aea15f5cacac94975c9555dab5571608a6a8206afb1e |
| images/fmt_ref.png | PNG 640x360 src | 8a31e7da297ef77578b8f69c622d8bb807af5b157e04d7fe85f827324c46693d |
| images/fmt.bmp | BMP | 62f337b86af68efce0f0a6e98e8a3a49e950858bcdaa85244180c57557d85be1 |
| images/fmt.tga | TGA | 4680683767d4050096628acd08f2201fcebb8d0c1baf0ba4e5bc8621a4ce9568 |
| images/fmt.ppm | PPM | a5e66aa7997e0b73d04f2ca14859e2c219c01c41f60cc39dcaa923f487c4b910 |
| images/fmt.pgm | PGM gray | b1146cb19e125d484d9e046b0abba6642124becdf858be04db7e77edcaf343fd |
| images/fmt_reencode.png | PNG reencode | 8a31e7da297ef77578b8f69c622d8bb807af5b157e04d7fe85f827324c46693d |
| images/fmt.jpg | JPEG q2 | f469e66f6e4f89a3dd7e0a381de3b382c0ca0a5f84ed645b8750911979d70417 |
| images/fmt.webp | WebP | 564c83308aa2dce8943e34fb97a47a4fc6a939d6425dbe167efd58a965c99edf |
| images/benchy_svg_raster.png | SVG->PNG 512 vector golden | e3df20f7892c99e2e53bb43c5a25695cdc2e4c71a8ccada989ab5194bcd7aa81 |

## video/
| file | note | sha256 |
|---|---|---|
| video/badapple.mp4 | source | bbcc179cad387a4a9e8b6cd1b34512f307079e83ec4397f582fcc9a1200c4348 |
| video/tashouheng.mp4 | source | 156f3664808bf5c4aea4305e50e4e1e3a0373c5b56e58851d300131021bccb52 |
| video/rickroll.mp4 | source | 4c8e889bf499e2be3501fb579423e5b44018ac46a16726352ee8572301805916 |
| video/luoqixi.mp4 | source | 13335c6755c990ee7517bc5c566369a40d09d835d31c6972eab4a9bf4a7211a6 |
| video/luoxiaohei.mp4 | source | 0790d53ae169ab99842672b9bd3fbb65c5ba5704de7ab41cba8ff68f39aba4fa |

### Bad Apple golden - 16 evenly-spaced frames (1920x1080 rgb24)
sha256 over decoded rgb24; 8x8 luma sig in `golden/badapple_frames.tsv`.

| frame | sha256(rgb24) |
|---|---|
| frame_00.png | c3e500c0631b8ae80e1a5a319cf22d0cddb5bbebd39ba3da45fb6cf31316d033 |
| frame_01.png | dd8a48bc365ba99cb5b3a47d350f2c735bb4fff5a65f4e6954fec5de4a3a4f35 |
| frame_02.png | 0420e0e6c1ea7ef1913b69bd8d6d50cd5ba901f606f68cb92f5dc8c8578e2b5f |
| frame_03.png | c9ba232205409ef24b70d2df973f4f52cc4ab2155a94767d19887f91ad2a9eef |
| frame_04.png | d26937ace25197b4ff3e4540eac3c8b998f8155fd2ff83d08d6a8019e1a1ddd8 |
| frame_05.png | 1b59b67e24d09931ffff2ed8508c4c6a5fee472020e9cbc3489de3e7dcb8c937 |
| frame_06.png | 333c5a723320a241745c11eed5ea9f6f565f745b7bc3919d21d79cf94da58cf7 |
| frame_07.png | 58b81138710e90b8647f2a032ecff6c14fb80d6ad2e5ff793e7fd9b60306665b |
| frame_08.png | 4ab9035c5d77e0cc9c78d91b61330cc534094473cfab81a6915a02647082376d |
| frame_09.png | 16960f6f6f86d1ec7cf4b61d9145e89680fc0ae0ae500c08b06e7de17fd7fcc1 |
| frame_10.png | 750e977efcc9a29385af01b58cd7e5358f502aa8bd2ac14f532cf5fc04ac298a |
| frame_11.png | 0cdf4df2dea92cc13fb2499a4da28927120da20f541cabf9a138980d0338a316 |
| frame_12.png | 715bd8d18e1bac24f80bd735d7ac528ad98d8bf183431e563c48cbfb3b471434 |
| frame_13.png | ac471caf22fb6c1b172614c4347be745f8ddb7782f0eea47f86388fb7a54b340 |
| frame_14.png | db943540649cdbaa473e994bdb0257aa96ead52807f5ac6e2f6d9532749a3f0b |
| frame_15.png | 5ae2057debb91c716c971cfb38a9fb2ada0b2463b70743a4f9f4d8d9456c6c88 |

### Bad Apple transcodes (first 5s, 640x480)
frame0 pure black => identical sha across codecs; t=2.0 diverges, ffv1 lossless ref.

| clip | codec | file sha256 | frame0 rgb24 | t2.0 rgb24 |
|---|---|---|---|---|
| video/badapple_clips/badapple_ffv1.mkv | ffv1 | 53fa2658bbd2524e9e0d0cff253d590a25c71a2e05e014f9fcaebc8c55c512ea | 0b150fd32588b1daca5569992ebe559c0102c837306b1af4c44d35128ec58366 | de077c14d028a646359f6bfe2659d7f883b8add9821e5bc58aedaebdb72f3130 |
| video/badapple_clips/badapple_h264.mp4 | h264 | 7330b4ad78e19e7f1afd6b58ac11bde11fe0e283e2f03d1e86fc5ee84e0c827d | 0b150fd32588b1daca5569992ebe559c0102c837306b1af4c44d35128ec58366 | ab11133749016c3060fac2046d285155e84c8352799e9e5a5ad8048238f7dea7 |
| video/badapple_clips/badapple_h265.mp4 | hevc | 49efd5a852bd0dec1e0367e6b7d32c32776cbcf9feb69502ee247cf280e5fc1f | 0b150fd32588b1daca5569992ebe559c0102c837306b1af4c44d35128ec58366 | ea30be0ee5896e3b4638827e0851d859e9dea3c11347eac0236739050d5356f3 |
| video/badapple_clips/badapple_vp9.webm | vp9 | 8aa7f89b666e659e1929ae4780f075d11b24ed8098ca924d7826b46ed8ef7c5e | 0b150fd32588b1daca5569992ebe559c0102c837306b1af4c44d35128ec58366 | dfecebeaa7523348e7650020ec00fd1ea2de3460e93d905455493e4851ee880e |

## audio/ (10 extracted, all AAC stereo source)
.m4a (aac copy) + .wav (pcm_s16le canonical) each; dayuhaitang+miyazakimountain also .flac+.opus.
wav_roundtrip_exact = decoded WAV PCM byte-equals decoded source AAC PCM (clean-decode proof, no speakers).

| slug | sr | ch | sample_count | dur_s | rms | roundtrip | pcm_sha256 |
|---|---|---|---|---|---|---|---|
| dayuhaitang | 44100 | 2 | 8767488 | 198.809 | 0.178639 | True | f948efa454b08a218c84053996974f998606307531572cb237da42679851d5f0 |
| tashouheng | 48000 | 2 | 10728448 | 223.509 | 0.26373 | True | 0c457d5f217670328228ae25a615889489247cf105fb401b9e4f2ac80cfcffbd |
| therightpath | 44100 | 2 | 6530048 | 148.074 | 0.107859 | True | 4d9ea77ead61085d65a25b7556418d866fb403753e1a2a68c59f39c9a00a8075 |
| huiyinruguo | 48000 | 2 | 14259200 | 297.067 | 0.266001 | True | 24dabb6ef82ec2dac990a4419d4ee59ad9cf8fd87dfd8257922b4190788514bd |
| fuguang | 48000 | 2 | 12124160 | 252.587 | 0.202412 | True | 989bfeee9c4a9e8501a811e9e54d13b3f69a4b959c9010d1c616b8b23123f24f |
| miyazakimountain | 44100 | 2 | 9617408 | 218.082 | 0.174488 | True | 17b467a78eb988528f458af1e6afed8464b316ac524b5aa809049865b01163b6 |
| heroreborn | 44100 | 2 | 10805248 | 245.017 | 0.197483 | True | 1cd27d51b42d7ccae17721eb5533c199bb612416efaa143529584003c85d0bd2 |
| riverflows | 48000 | 2 | 10808320 | 225.173 | 0.028555 | True | 01b57747bc9d9332188070a50a06ac371e95591827f13c38aa5524a200701e99 |
| blackblueroom | 44100 | 2 | 16340992 | 370.544 | 0.158122 | True | 0c49e0c5426e11ba3302af5c68afb9d9900a6d1537df94f15af27be9e5ca7f25 |
| flowerdance | 44100 | 2 | 2142208 | 48.576 | 0.173614 | True | 19b8f5af6f4fa7612997c90149ef98e25d87f852dc62446a6da2aaa61f690627 |

<details><summary>audio file sha256</summary>

- `audio/blackblueroom.m4a`  849926eef2cf01498f2c2a87b721872d80e9f221ee2eb52f63c5392cb4f565c9
- `audio/blackblueroom.wav`  00af4c100381f04529debeaa1ee00771ed4cffd289b95461f11ce8821578ee3a
- `audio/dayuhaitang.flac`  f66a692832f8f1a424c5958682f540ae0d3fa4e129e1dc187e715a40f2e8a473
- `audio/dayuhaitang.m4a`  ec08de0e4e35f94b9e34f56277fecff881ac2f8042e211afcb8b4948540b1487
- `audio/dayuhaitang.opus`  00c2b89c7f7d4c22676b3e4b36d27c44f535217894dc55a7cb48a822cb9717bc
- `audio/dayuhaitang.wav`  01107e64b72869dcd80eada6209ed04e08c887dc8224f69c15108a3503d16e8c
- `audio/flowerdance.m4a`  b673e6901ba295151c451722cf41e7a66e6f477cfd12895b4177ad3e9130c8b1
- `audio/flowerdance.wav`  958e24e9e7d13e9f6aafe146b8bebaa97fb0e4b2d0e7adf1e2c0ab8f278c234c
- `audio/fuguang.m4a`  6b32d8dd212d9e7f70fc8dbb0d94a50cbc0ab617b851839abb1a6f00e08d1302
- `audio/fuguang.wav`  7330a6ba7c8a5ef78996c7c49a1b24134168c12803f2fb20d941a3b1d4439e75
- `audio/heroreborn.m4a`  344e3613eb5c3bf0079498a5d0b6630bf711ccadeddee15f03000fe66ee601dd
- `audio/heroreborn.wav`  69acc4d48607b2f2fe27c996df8d3a16426b971089ad4157d19ba302f2dc1db8
- `audio/huiyinruguo.m4a`  124ee5c114733b73b364e9e6a3402b2479bb36f38eebc93d918edc1de0baa0e7
- `audio/huiyinruguo.wav`  f9d6b33e216991de0a8f94f0522422baba07d227efd864848b725219f34b1017
- `audio/miyazakimountain.flac`  43172a193365c92eedbe2ebad075c801a7931c4278dbe6c0ff92634eab80d189
- `audio/miyazakimountain.m4a`  9eebd0bce4cf2c50b293ed892d5888a5be92ecfef22ae7fdd0c5fae072eb6cfc
- `audio/miyazakimountain.opus`  0798d1ee5800c2985006c695e3ae0117662cb75146e7f92a58868685bf246bab
- `audio/miyazakimountain.wav`  470cc65d9ca4ea42ed093339746f6f6dbe399d67a3e3954af41de5028f64aaca
- `audio/riverflows.m4a`  7cc9ae32ea3f149e33859a3d9255c28c86b5147f680d02a206b477491741808d
- `audio/riverflows.wav`  63003126b03c72bc77a96d6cbe10e34196577bdf73a83169f389286275460b19
- `audio/tashouheng.m4a`  1435436d4e29338fa800332f9971eacaa9da80ef1e4024adb232b13f608eaf8e
- `audio/tashouheng.wav`  4380045c6fdd137dae87faa960dca18db75f135b73893ad7599b2e015e9048fc
- `audio/therightpath.m4a`  548116368a49d18fe15277db22842110b8f1f4e253120c2342fd0a98723e7b0e
- `audio/therightpath.wav`  22e3854bffa3252f9a83693c830791d0c9b5cfce6e86e91cec62dea7d3b4a09d
</details>

Note: .flac encoded s32 (ffmpeg default), so flac->s16le != s16 wav bytes though RMS identical; wav is canonical exact PCM ref. .opus lossy.

## models/ + slicer golden
| file | sha256 |
|---|---|
| models/benchy.stl | 56269106d833ad841c19c6184b4003b832ce27cd48d79d3679c59a6dd66ac7b4 |
| models/benchy.svg | 2afc4727f5135505e2826de6ceb1d913a6ec0f14ffcfc938ff3db02350555b63 |
| models/cube.stl | 9b3719534e9b0b8fb1a94f783eba0dddc1b19e34467df043da1054f245839af2 |
| models/suzanne.glb | bc14168d6ea081571683e8a23f51623f80638e29cdb042e67b566bb1d3866d97 |
| models/suzanne.mtl | 6272bc44e24f1ecd395864d81f015a199b2fdab6209c06d0e01e1dbff93924ca |
| models/suzanne.obj | 2dc2c71dfd65e6fa5582178fa275f47e00b18f03fb4da69da869b346f53b2ab5 |
| models/suzanne.stl | 53ccb6f6aa8ad9dbfe98d8aa52d3de5cea1fd9e5d9e42d38299e473afa739393 |

Slicer golden `golden/slice_golden.json` (gen `models/slice_golden.py`), mesh-plane intersection.
- **cube.stl** (generated unit cube): every interior Z slice per=4.0 area=1.0 (analytic square, measured exact).
- **suzanne.stl** (968 tris): 5 layers (z,n_segments,perimeter,area); e.g. z=-0.65625:per=1.784272,area=0.017366; z=-0.328125:per=3.022516,area=0.09767; z=0.0:per=7.762914,area=0.569877 ...
- **benchy.stl** (16186 tris): 5 layers (z,n_segments,perimeter,area); e.g. z=8.0:per=168.945253,area=205.729875; z=16.0:per=196.403478,area=33.376142; z=24.0:per=170.917877,area=62.669466 ...

## pointcloud/
- `pointcloud/bunny.ply`  b1acc63bece78444aa2e15bdcc72371a201279b98c6f5d4b74c993d02f0566fe
- `pointcloud/bunny_scan000.ply`  7d48f9fdf917311de680d074edce8aff25a4b9bfd87be9301822dace811209fb
Source: Stanford 3D Scanning Repo bunny.tar.gz. bunny.ply=bun_zipper (35947 verts/69451 faces); bunny_scan000.ply=raw scan.

## docs/
- `docs/software-doc-spec.pdf`  a318598cc1c3193a62d7e2b73d79315d91878343f6942e5a6eefac6f925b8181

## subtitles/
- `subtitles/badapple.ass`  608badba0d800c2260d58aabb4f3fe4f311bc6d631de3d64ba6b951a44befa70
- `subtitles/luoqixi.txt`  aa84acbe83ddfd35434c49bffe2fe93051ac76bc0154d40ce8d77d8c9f07f5c7
- `subtitles/rickroll.json`  9cd6d72b87c73846ac7da8cf377a7719a4683eb91942d8914b48ec2fc042ece4
- `subtitles/tashouheng.srt`  fdfc620507102c64cf59333746db8b6f5d6cdbd3e013c2a7f3848b2cbee22a01

