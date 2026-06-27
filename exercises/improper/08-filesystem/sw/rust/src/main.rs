//! 文件管理：从裸指针块设备到 easy-fs 风格的简易文件系统 —— Rust。
//!
//! 母题：磁盘只是「一大片能按块寻址的格子」，文件 / 目录 / KV 记录都是
//! 软件在这片格子上约定出来的字节排布。三段逐题递进，你只填 TODO：
//!   (a) 裸指针 / MMIO 块设备：写进去再读出来逐字节相等   → BDEV_PASS
//!   (b) 按 key 扫描 KV 记录（EMM233 头 / EMM666 尾）       → KV_PASS
//!   (c) easy-fs 风格 inode / 目录 / 目录项                → FS_PASS
//! 三段皆过再打印 ALL_PASS。下方测试 harness 勿改。
#![allow(dead_code, unused_variables, unused_mut)]

const BLOCK_SIZE: usize = 512;
const NUM_BLOCKS: usize = 64;

// ── (a) 裸指针 / MMIO 块设备 ────────────────────────────────────────
// 块设备 = 一大片字节 RAM；块号 × 块大小 = MMIO 数据窗口基址。
struct BlockDev {
    mem: Vec<u8>,
}

impl BlockDev {
    fn new() -> Self {
        Self { mem: vec![0u8; BLOCK_SIZE * NUM_BLOCKS] }
    }

    /// 把块号换算成 MMIO 窗口基址（裸指针）。
    fn window(&mut self, block_id: usize) -> *mut u8 {
        unsafe { self.mem.as_mut_ptr().add(block_id * BLOCK_SIZE) }
    }

    /// 把整块从 buf 搬到块设备窗口。
    fn write_block(&mut self, block_id: usize, buf: &[u8]) {
        let w = self.window(block_id);
        // TODO: 用裸指针把 buf[0..BLOCK_SIZE] 写到窗口 w。
        //   // TODO[a] 按字节循环：for i in 0..BLOCK_SIZE { write_volatile(w.add(i), buf[i]) }
        //   // ELSE[b] 整块 memcpy：copy_nonoverlapping(buf.as_ptr(), w, BLOCK_SIZE)
        // HINT: unsafe { core::ptr::write_volatile(w.add(i), buf[i]); }
        let _ = (w, buf); // ← 占位：什么都没写 → 读回不相等 → BDEV_FAIL
    }

    /// 把整块从块设备窗口读回 buf。
    fn read_block(&mut self, block_id: usize, buf: &mut [u8]) {
        let w = self.window(block_id);
        // TODO: 用裸指针把窗口 w 的 BLOCK_SIZE 字节读进 buf。
        // HINT: unsafe { buf[i] = core::ptr::read_volatile(w.add(i)); }
        let _ = (w, &buf); // ← 占位：没有读出 → BDEV_FAIL
    }
}

// ── (b) KV 记录扫描 ─────────────────────────────────────────────────
// 记录布局：EMM233 | key[3] | len(u8) | data[len] | EMM666
const KV_HEAD: &[u8; 6] = b"EMM233";
const KV_TAIL: &[u8; 6] = b"EMM666";

/// 线性扫描 buf，校验头/尾，收集 key 命中记录。
/// 返回 (命中记录数, 命中记录的数据字节校验和)。
fn find_by_key(buf: &[u8], key: &[u8; 3]) -> (u32, u32) {
    let mut count = 0u32;
    let mut sum = 0u32;
    let mut off = 0usize;
    // TODO: while off + 16 <= buf.len():
    //   1) 校验 buf[off..off+6] == KV_HEAD，否则 break（记录区结束）
    //   2) rkey = buf[off+6..off+9]; len = buf[off+9]; tail_off = off+10+len
    //   3) 校验 buf[tail_off..tail_off+6] == KV_TAIL，否则 break
    //   4) 若 rkey == key：count += 1；把 data 字节累加进 sum（wrapping_add）
    //   5) off = tail_off + 6
    //   // TODO[a] 线性扫描（如上）   // ELSE[b] 先建 key→offset 索引表再查
    // HINT: &buf[off..off+6] != KV_HEAD 即可比较切片。
    let _ = (buf, key, &mut off); // ← 占位：返回 (0,0) → KV_FAIL
    (count, sum)
}

// ── (c) easy-fs 风格 inode / 目录 / 目录项 ──────────────────────────
// 磁盘布局：
//   block 0      : SuperBlock
//   block 1      : inode 区（16 个 DiskInode，每个 32 字节）
//   block 2..    : 数据区（块 2 预留给根目录的目录项）
const MAGIC: u32 = 0x7366_7A65; // "ezfs"
const INODE_SIZE: usize = 32; // size(4) + type(4) + direct[6]*4
const MAX_DIRECT: usize = 6;
const DIRENT_SIZE: usize = 32; // name[28] + inode_no(4)
const NAME_CAP: usize = 28;

fn rd_u32(b: &[u8], off: usize) -> u32 {
    u32::from_le_bytes([b[off], b[off + 1], b[off + 2], b[off + 3]])
}
fn wr_u32(b: &mut [u8], off: usize, v: u32) {
    b[off..off + 4].copy_from_slice(&v.to_le_bytes());
}

// 这两个 inode 低层读写已给好，直接用。
fn write_inode(dev: &mut BlockDev, ino: usize, size: u32, typ: u32, directs: &[u32; MAX_DIRECT]) {
    let mut blk = [0u8; BLOCK_SIZE];
    dev.read_block(1, &mut blk);
    let base = ino * INODE_SIZE;
    wr_u32(&mut blk, base, size);
    wr_u32(&mut blk, base + 4, typ);
    for k in 0..MAX_DIRECT {
        wr_u32(&mut blk, base + 8 + 4 * k, directs[k]);
    }
    dev.write_block(1, &blk);
}

fn read_inode(dev: &mut BlockDev, ino: usize) -> (u32, u32, [u32; MAX_DIRECT]) {
    let mut blk = [0u8; BLOCK_SIZE];
    dev.read_block(1, &mut blk);
    let base = ino * INODE_SIZE;
    let size = rd_u32(&blk, base);
    let typ = rd_u32(&blk, base + 4);
    let mut directs = [0u32; MAX_DIRECT];
    for k in 0..MAX_DIRECT {
        directs[k] = rd_u32(&blk, base + 8 + 4 * k);
    }
    (size, typ, directs)
}

/// 格式化：清零块设备 + 写 SuperBlock + 建空根目录（inode 0）。
fn fs_mkfs(dev: &mut BlockDev) {
    // TODO:
    //   1) 把全部 NUM_BLOCKS 个块清零（write_block 写全 0 缓冲）
    //   2) 在 block 0 写 SuperBlock：off0=MAGIC, off4=NUM_BLOCKS, off8=1(inode_start),
    //      off12=16(inode_count), off16=2(data_start), off20=1(next_inode), off24=3(next_block)
    //   3) 建根 inode：write_inode(dev, 0, 0, 1, &[2,0,0,0,0,0])  // type=1(dir)，direct[0]=2
    // HINT: let mut sb=[0u8;BLOCK_SIZE]; wr_u32(&mut sb, 0, MAGIC); ...; dev.write_block(0,&sb);
    let _ = dev; // ← 占位：未格式化 → 后续全乱 → FS_FAIL
}

/// 在根目录创建一个空文件，返回其 inode 号。
fn fs_create(dev: &mut BlockDev, name: &str) -> u32 {
    // TODO:
    //   1) 读 block 0；ino = next_inode(off20)；把 off20 写回 ino+1
    //   2) write_inode(dev, ino, 0, 2, &[0;MAX_DIRECT])  // 空文件 type=2
    //   3) 读根 inode (read_inode(dev,0)) 拿到 rsize 与 rdir；idx = rsize/DIRENT_SIZE
    //   4) 读根目录数据块 rdir[0]，在第 idx 条目处写 name（前 NAME_CAP 字节）+ wr_u32(ino) 到 +NAME_CAP
    //   5) 写回数据块；write_inode(dev, 0, rsize+DIRENT_SIZE, 1, &rdir)
    // HINT: name.as_bytes()；不足 NAME_CAP 补 0。
    let _ = (dev, name); // ← 占位：返回 0 且未建目录项 → FS_FAIL
    0
}

/// 列出根目录下所有文件名。
fn fs_ls(dev: &mut BlockDev) -> Vec<String> {
    // TODO: 读根 inode 拿 rsize/rdir；读 rdir[0] 数据块；
    //   对 i in 0..(rsize/DIRENT_SIZE)：取 name[28]（到第一个 0 截断）收进结果。
    // HINT: String::from_utf8_lossy(&name[..end]).into_owned()
    let _ = dev; // ← 占位：返回空表 → FS_FAIL
    Vec::new()
}

/// 把内容写入某 inode（按需分配数据块，仅 direct 映射）。
fn fs_write(dev: &mut BlockDev, ino: u32, data: &[u8]) {
    // TODO:
    //   1) nblocks = ceil(data.len()/BLOCK_SIZE)
    //   2) 读 block 0 拿 next_block(off24)；为每块分配一个号填进 directs[k]，写入该块内容
    //   3) 把 next_block 写回 block 0；write_inode(dev, ino, data.len(), 2, &directs)
    // HINT: 末块不足 BLOCK_SIZE 时只拷 data 剩余字节，其余补 0。
    let _ = (dev, ino, data); // ← 占位：没写 → 读回不一致 → FS_FAIL
}

/// 读回某 inode 的全部内容。
fn fs_read(dev: &mut BlockDev, ino: u32) -> Vec<u8> {
    // TODO: read_inode 拿 size 与 directs；按 size 逐块读回拼成 Vec（末块只取剩余字节）。
    let _ = (dev, ino); // ← 占位：返回空 → FS_FAIL
    Vec::new()
}

// ───────────────────────── 测试 harness（勿改）─────────────────────────

fn check_bdev() -> bool {
    let mut dev = BlockDev::new();
    let blocks = [3usize, 7, 40, 63];
    for &b in &blocks {
        let mut buf = [0u8; BLOCK_SIZE];
        for i in 0..BLOCK_SIZE {
            buf[i] = ((b * 7 + i) & 0xff) as u8;
        }
        dev.write_block(b, &buf);
    }
    for &b in &blocks {
        let mut buf = [0u8; BLOCK_SIZE];
        dev.read_block(b, &mut buf);
        for i in 0..BLOCK_SIZE {
            let want = ((b * 7 + i) & 0xff) as u8;
            if buf[i] != want {
                println!("BDEV_FAIL blk={} i={} got={} want={}", b, i, buf[i], want);
                return false;
            }
        }
    }
    println!("BDEV_PASS");
    true
}

fn push_rec(buf: &mut Vec<u8>, key: &[u8; 3], data: &[u8]) {
    buf.extend_from_slice(KV_HEAD);
    buf.extend_from_slice(key);
    buf.push(data.len() as u8);
    buf.extend_from_slice(data);
    buf.extend_from_slice(KV_TAIL);
}

fn check_kv() -> bool {
    let mut buf = Vec::new();
    push_rec(&mut buf, b"log", &[1, 2, 3, 4]);
    push_rec(&mut buf, b"cfg", &[10, 20, 30]);
    push_rec(&mut buf, b"log", &[5, 6, 7, 8, 9]);
    push_rec(&mut buf, b"usr", &[100]);
    buf.extend_from_slice(&[0u8; 8]); // 终止哨兵
    let (c1, s1) = find_by_key(&buf, b"log");
    if c1 != 2 || s1 != 45 {
        println!("KV_FAIL key=log got=({},{}) want=(2,45)", c1, s1);
        return false;
    }
    let (c2, s2) = find_by_key(&buf, b"cfg");
    if c2 != 1 || s2 != 60 {
        println!("KV_FAIL key=cfg got=({},{}) want=(1,60)", c2, s2);
        return false;
    }
    println!("KV_PASS");
    true
}

fn check_fs() -> bool {
    let mut dev = BlockDev::new();
    fs_mkfs(&mut dev);
    let i1 = fs_create(&mut dev, "alpha.txt");
    let _i2 = fs_create(&mut dev, "beta.bin");
    let _i3 = fs_create(&mut dev, "gamma");
    let names = fs_ls(&mut dev);
    for want in ["alpha.txt", "beta.bin", "gamma"] {
        if !names.iter().any(|n| n == want) {
            println!("FS_FAIL ls 缺少 {}", want);
            return false;
        }
    }
    if names.len() != 3 {
        println!("FS_FAIL ls 应有 3 项，实得 {}", names.len());
        return false;
    }
    let mut content = Vec::new();
    for i in 0..700u32 {
        content.push((i.wrapping_mul(31).wrapping_add(7)) as u8);
    }
    fs_write(&mut dev, i1, &content);
    let got = fs_read(&mut dev, i1);
    if got != content {
        println!("FS_FAIL 读回不一致 got_len={} want_len={}", got.len(), content.len());
        return false;
    }
    println!("FS_PASS");
    true
}

fn main() {
    let mut all = true;
    all &= check_bdev();
    all &= check_kv();
    all &= check_fs();
    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
