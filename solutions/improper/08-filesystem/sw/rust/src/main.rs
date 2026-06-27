//! 文件管理：从裸指针块设备到 easy-fs 风格的简易文件系统 —— Rust 参考解。
//!
//! 母题：磁盘只是「一大片能按块寻址的格子」，文件 / 目录 / KV 记录都是
//! 软件在这片格子上约定出来的字节排布。三段逐题递进：
//!   (a) 裸指针 / MMIO 块设备：写进去再读出来逐字节相等   → BDEV_PASS
//!   (b) 按 key 扫描 KV 记录（EMM233 头 / EMM666 尾）       → KV_PASS
//!   (c) easy-fs 风格 inode / 目录 / 目录项：mkfs→create→ls→读写 → FS_PASS
//! 三段皆过再打印 ALL_PASS。
#![allow(dead_code)]

const BLOCK_SIZE: usize = 512;
const NUM_BLOCKS: usize = 64;

// ── (a) 裸指针 / MMIO 块设备 ────────────────────────────────────────
// 块设备 = 一大片字节 RAM；块号 × 块大小 = MMIO 数据窗口基址。
// 驱动只认「按块读写」，不关心底层是 virtio 还是内存盘。
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

    /// 把整块从 buf 搬到块设备窗口（volatile 逐字节）。
    fn write_block(&mut self, block_id: usize, buf: &[u8]) {
        let w = self.window(block_id);
        for i in 0..BLOCK_SIZE {
            unsafe { core::ptr::write_volatile(w.add(i), buf[i]); }
        }
    }

    /// 把整块从块设备窗口读回 buf（volatile 逐字节）。
    fn read_block(&mut self, block_id: usize, buf: &mut [u8]) {
        let w = self.window(block_id);
        for i in 0..BLOCK_SIZE {
            unsafe { buf[i] = core::ptr::read_volatile(w.add(i)); }
        }
    }
}

// ── (b) KV 记录扫描 ─────────────────────────────────────────────────
// 记录布局：EMM233 | key[3] | len(u8) | data[len] | EMM666
// head 后 3 字节当 key（短文件名雏形）。head 不匹配即视为终止。
const KV_HEAD: &[u8; 6] = b"EMM233";
const KV_TAIL: &[u8; 6] = b"EMM666";

/// 线性扫描 buf，校验头/尾，收集 key 命中记录。
/// 返回 (命中记录数, 命中记录的数据字节校验和)。
fn find_by_key(buf: &[u8], key: &[u8; 3]) -> (u32, u32) {
    let mut count = 0u32;
    let mut sum = 0u32;
    let mut off = 0usize;
    while off + 16 <= buf.len() {
        if &buf[off..off + 6] != KV_HEAD {
            break; // 头不匹配 = 记录区结束
        }
        let rkey = &buf[off + 6..off + 9];
        let len = buf[off + 9] as usize;
        let tail_off = off + 10 + len;
        if tail_off + 6 > buf.len() || &buf[tail_off..tail_off + 6] != KV_TAIL {
            break; // 尾不匹配 = 记录损坏
        }
        if rkey == key {
            count += 1;
            for &b in &buf[off + 10..off + 10 + len] {
                sum = sum.wrapping_add(b as u32);
            }
        }
        off = tail_off + 6;
    }
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
    let zero = [0u8; BLOCK_SIZE];
    for b in 0..NUM_BLOCKS {
        dev.write_block(b, &zero);
    }
    let mut sb = [0u8; BLOCK_SIZE];
    wr_u32(&mut sb, 0, MAGIC);
    wr_u32(&mut sb, 4, NUM_BLOCKS as u32);
    wr_u32(&mut sb, 8, 1); // inode_start
    wr_u32(&mut sb, 12, 16); // inode_count_max
    wr_u32(&mut sb, 16, 2); // data_start
    wr_u32(&mut sb, 20, 1); // next_inode（0 留给根目录）
    wr_u32(&mut sb, 24, 3); // next_block（块 2 用作根目录数据块）
    dev.write_block(0, &sb);
    // 根 inode：type=1(dir)、size=0、direct[0]=2
    write_inode(dev, 0, 0, 1, &[2, 0, 0, 0, 0, 0]);
}

/// 在根目录创建一个空文件，返回其 inode 号。
fn fs_create(dev: &mut BlockDev, name: &str) -> u32 {
    let mut sb = [0u8; BLOCK_SIZE];
    dev.read_block(0, &mut sb);
    let ino = rd_u32(&sb, 20);
    wr_u32(&mut sb, 20, ino + 1);
    dev.write_block(0, &sb);
    // 新文件 inode：空、type=2(file)
    write_inode(dev, ino as usize, 0, 2, &[0; MAX_DIRECT]);
    // 往根目录追加一条目录项
    let (rsize, _rtyp, rdir) = read_inode(dev, 0);
    let idx = rsize as usize / DIRENT_SIZE;
    let mut blk = [0u8; BLOCK_SIZE];
    dev.read_block(rdir[0] as usize, &mut blk);
    let de = idx * DIRENT_SIZE;
    let nb = name.as_bytes();
    for j in 0..NAME_CAP {
        blk[de + j] = if j < nb.len() { nb[j] } else { 0 };
    }
    wr_u32(&mut blk, de + NAME_CAP, ino);
    dev.write_block(rdir[0] as usize, &blk);
    write_inode(dev, 0, rsize + DIRENT_SIZE as u32, 1, &rdir);
    ino
}

/// 列出根目录下所有文件名。
fn fs_ls(dev: &mut BlockDev) -> Vec<String> {
    let (rsize, _rtyp, rdir) = read_inode(dev, 0);
    let mut blk = [0u8; BLOCK_SIZE];
    dev.read_block(rdir[0] as usize, &mut blk);
    let mut out = Vec::new();
    let n = rsize as usize / DIRENT_SIZE;
    for i in 0..n {
        let de = i * DIRENT_SIZE;
        let name = &blk[de..de + NAME_CAP];
        let end = name.iter().position(|&c| c == 0).unwrap_or(NAME_CAP);
        out.push(String::from_utf8_lossy(&name[..end]).into_owned());
    }
    out
}

/// 把内容写入某 inode（按需分配数据块，仅 direct 映射）。
fn fs_write(dev: &mut BlockDev, ino: u32, data: &[u8]) {
    let nblocks = (data.len() + BLOCK_SIZE - 1) / BLOCK_SIZE;
    let mut sb = [0u8; BLOCK_SIZE];
    dev.read_block(0, &mut sb);
    let mut nextb = rd_u32(&sb, 24);
    let mut directs = [0u32; MAX_DIRECT];
    for k in 0..nblocks {
        directs[k] = nextb;
        nextb += 1;
        let mut blk = [0u8; BLOCK_SIZE];
        let s = k * BLOCK_SIZE;
        let e = core::cmp::min(s + BLOCK_SIZE, data.len());
        blk[..e - s].copy_from_slice(&data[s..e]);
        dev.write_block(directs[k] as usize, &blk);
    }
    wr_u32(&mut sb, 24, nextb);
    dev.write_block(0, &sb);
    write_inode(dev, ino as usize, data.len() as u32, 2, &directs);
}

/// 读回某 inode 的全部内容。
fn fs_read(dev: &mut BlockDev, ino: u32) -> Vec<u8> {
    let (size, _typ, directs) = read_inode(dev, ino as usize);
    let mut out = Vec::new();
    let mut remaining = size as usize;
    let mut k = 0;
    while remaining > 0 {
        let mut blk = [0u8; BLOCK_SIZE];
        dev.read_block(directs[k] as usize, &mut blk);
        let take = core::cmp::min(BLOCK_SIZE, remaining);
        out.extend_from_slice(&blk[..take]);
        remaining -= take;
        k += 1;
    }
    out
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
    // 写入跨多块内容再读回
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
