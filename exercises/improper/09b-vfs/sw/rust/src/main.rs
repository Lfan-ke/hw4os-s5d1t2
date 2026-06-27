//! VFS（虚拟文件系统）—— Rust。
//!
//! 母题：VFS = 让多种文件系统「共存于同一组接口」之下的抽象层。
//! （Sun 1986 为接入 NFS 发明：一台机器上 ext/nfs/proc 都长成同一张 `read/write` 脸。）
//!
//! VFS 四大对象，本课的极简对应：
//!   superblock = 一个挂好的 FS 实例（这里是 `Box<dyn FileSystem>`）
//!   inode      = FS 内的一个节点（这里用一个 `usize` 节点号代表）
//!   dentry     = 「名字 → 节点」的解析（各 FS 自己的 `lookup`）
//!   file       = 打开后的句柄（这里是 `OpenFile{挂载下标, 节点号}`）
//!
//! 两个 mock 文件系统，背后行为完全不同、对外接口完全一致：
//!   ① RamFs —— 内存里几个真文件（有存储，写进去读得出）。
//!   ② DevFs —— 虚拟设备：/null 吞掉写入、/zero 读出全 0（无存储，纯副作用）。
//!
//! VFS 维护一张挂载表：RamFs 挂在 "/"，DevFs 挂在 "/dev"。
//! open("/hello") 落到 RamFs；open("/dev/zero") 跨过挂载点落到 DevFs。
//!
//! 你只实现两处（标 TODO）：
//!   (1) `Vfs::resolve` —— 按路径找「最长挂载前缀」，路由到对应 FS；
//!   (2) `RamFs::lookup` —— 在本 FS 的文件表里把名字解析成节点号。
//! 下方测试 harness（向量 + 断言 + PASS 打印）勿改。
#![allow(unused_variables, dead_code)]

/// /dev/zero 一次读出的零字节数（够看出「读出全 0」即可）。
const ZERO_READ_LEN: usize = 8;

// ════════════════════════════════════════════════════════════════
// 统一接口：一切文件系统都实现这张「脸」（对应 Linux 的 struct file_operations /
// super_operations，rcore 的 FileSystem/Inode trait）。
// ════════════════════════════════════════════════════════════════
trait FileSystem {
    /// 这个 FS 的名字（用来证明「路由到了哪个 FS」）。
    fn name(&self) -> &str;
    /// 名字解析（dentry→inode）：把 FS 内相对路径映射到节点号。找不到返回 None。
    fn lookup(&self, rel: &str) -> Option<usize>;
    /// 读节点全部内容。
    fn read(&self, node: usize) -> Vec<u8>;
    /// 写节点，返回「写了几字节」（吞写设备返回 len 但不真正存储）。
    fn write(&mut self, node: usize, data: &[u8]) -> usize;
    /// 列目录：本 FS 里所有节点的名字（对应 readdir）。
    fn readdir(&self) -> Vec<String>;
}

// ── FS 实例 ①：RamFs（内存里几个真文件，有存储）──────────────────
struct RamFs {
    files: Vec<(String, Vec<u8>)>, // (相对路径名, 内容)
}
impl FileSystem for RamFs {
    fn name(&self) -> &str {
        "ramfs"
    }
    fn lookup(&self, rel: &str) -> Option<usize> {
        // TODO(2)：名字解析（dentry→inode）。顺序扫 self.files，
        //   找到 name == rel 的那一项，返回它的下标当节点号 Some(i)；扫完没有返回 None。
        // HINT: for (i, (name, _)) in self.files.iter().enumerate() { if name == rel { return Some(i); } }
        None // ← 占位（永远找不到，VFS_PASS 跑不出来）
    }
    fn read(&self, node: usize) -> Vec<u8> {
        self.files[node].1.clone()
    }
    fn write(&mut self, node: usize, data: &[u8]) -> usize {
        // 真存储：把内容存下来，下次 read 读得出。
        self.files[node].1 = data.to_vec();
        data.len()
    }
    fn readdir(&self) -> Vec<String> {
        self.files.iter().map(|(n, _)| n.clone()).collect()
    }
}

// ── FS 实例 ②：DevFs（虚拟设备，无存储，纯副作用）────────────────
// 节点号约定：0 = /null（吞写、读空），1 = /zero（读出全 0）。
struct DevFs;
impl FileSystem for DevFs {
    fn name(&self) -> &str {
        "devfs"
    }
    fn lookup(&self, rel: &str) -> Option<usize> {
        match rel {
            "/null" => Some(0),
            "/zero" => Some(1),
            _ => None,
        }
    }
    fn read(&self, node: usize) -> Vec<u8> {
        match node {
            1 => vec![0u8; ZERO_READ_LEN], // /dev/zero：读出一串 0
            _ => Vec::new(),               // /dev/null：读出空
        }
    }
    fn write(&mut self, _node: usize, data: &[u8]) -> usize {
        // /dev/null 与 /dev/zero 都吞掉写入：报告「写了 data.len() 字节」但不存储。
        data.len()
    }
    fn readdir(&self) -> Vec<String> {
        vec!["/null".to_string(), "/zero".to_string()]
    }
}

// ════════════════════════════════════════════════════════════════
// 已给的路径工具（勿改）：判定 path 是否在挂载点下、剥出 FS 内相对路径。
// ════════════════════════════════════════════════════════════════

/// path 是否落在挂载点 mount 之下？带边界检查，避免 "/dev" 误配 "/device"。
fn is_under(mount: &str, path: &str) -> bool {
    if mount == "/" {
        return path.starts_with('/'); // 根挂载覆盖一切绝对路径
    }
    if !path.starts_with(mount) {
        return false;
    }
    let rest = &path[mount.len()..];
    rest.is_empty() || rest.starts_with('/')
}

/// 把挂载点从 path 上剥掉，得到 FS 内相对路径。
/// 根挂载("/")返回整条路径；其余挂载剥掉前缀（"/dev/zero" 挂 "/dev" → "/zero"）。
fn subpath(mount: &str, path: &str) -> String {
    if mount == "/" {
        return path.to_string();
    }
    let rest = &path[mount.len()..];
    if rest.is_empty() {
        "/".to_string()
    } else {
        rest.to_string()
    }
}

// ════════════════════════════════════════════════════════════════
// VFS 层：挂载表 + 路径路由 + 统一 open/read/write
// ════════════════════════════════════════════════════════════════

/// 打开后的句柄（对应 VFS 的 struct file）：记住落在哪个挂载、哪个节点。
#[derive(Clone, Copy)]
struct OpenFile {
    mi: usize,   // 挂载表下标 = 哪个 FS 实例
    node: usize, // 该 FS 内的节点号
}

struct Vfs {
    mounts: Vec<(String, Box<dyn FileSystem>)>, // (挂载点路径, FS 实例)
}
impl Vfs {
    fn new() -> Self {
        Vfs { mounts: Vec::new() }
    }
    /// 把一个 FS 挂到某路径下（对应 mount(2)）。
    fn mount(&mut self, at: &str, fs: Box<dyn FileSystem>) {
        self.mounts.push((at.to_string(), fs));
    }

    /// 【学生填空 (1)】路径路由：在挂载表里找**最长**匹配的挂载前缀，
    /// 返回 (挂载下标, FS 内相对路径)。一个都不匹配返回 None。
    ///
    /// 为什么要「最长」：路径 "/dev/zero" 同时落在 "/"(长 1) 和 "/dev"(长 4) 之下，
    /// 必须选更长的 "/dev"，才能跨过挂载点进入 DevFs，而不是停在根的 RamFs。
    fn resolve(&self, path: &str) -> Option<(usize, String)> {
        // TODO(1)：遍历 self.mounts，用已给的 is_under(挂载点, path) 判前缀；
        //   在所有命中的挂载里挑**挂载点字符串最长**的那个（best）。
        //   都没命中返回 None；命中则用 subpath(挂载点, path) 剥出相对路径，
        //   返回 Some((best 下标, 相对路径))。
        // HINT:
        //   let mut best: Option<usize> = None;
        //   let mut best_len = 0usize;
        //   for (i, (mp, _)) in self.mounts.iter().enumerate() {
        //       if is_under(mp, path) && (best.is_none() || mp.len() > best_len) {
        //           best = Some(i); best_len = mp.len();
        //       }
        //   }
        //   let i = best?;
        //   Some((i, subpath(&self.mounts[i].0, path)))
        None // ← 占位（永远路由失败，MOUNT/DISPATCH 跑不出来）
    }

    // ── 统一接口：调用者只认 path，不关心背后是哪种 FS ──
    fn open(&self, path: &str) -> Option<OpenFile> {
        let (mi, rel) = self.resolve(path)?; // 先路由到 FS
        let node = self.mounts[mi].1.lookup(&rel)?; // 再让该 FS 解析名字
        Some(OpenFile { mi, node })
    }
    fn read(&self, of: &OpenFile) -> Vec<u8> {
        self.mounts[of.mi].1.read(of.node)
    }
    fn write(&mut self, of: &OpenFile, data: &[u8]) -> usize {
        self.mounts[of.mi].1.write(of.node, data)
    }
    /// 某路径会被路由到哪个 FS 的名字（用于证明路由正确）。
    fn fs_name(&self, path: &str) -> Option<&str> {
        let (mi, _) = self.resolve(path)?;
        Some(self.mounts[mi].1.name())
    }
}

// ════════════════════════════════════════════════════════════════
// 测试 harness（给定，勿改）
// ════════════════════════════════════════════════════════════════

/// 搭一棵标准挂载树：RamFs 挂 "/"（含 /hello、/motd），DevFs 挂 "/dev"。
fn build_vfs() -> Vfs {
    let mut vfs = Vfs::new();
    vfs.mount(
        "/",
        Box::new(RamFs {
            files: vec![
                ("/hello".to_string(), b"hi, vfs\n".to_vec()),
                ("/motd".to_string(), b"welcome\n".to_vec()),
            ],
        }),
    );
    vfs.mount("/dev", Box::new(DevFs));
    vfs
}

/// 信息性打印（非判据）：用 readdir 把挂载树列出来。
fn dump(vfs: &Vfs) {
    for (mp, fs) in &vfs.mounts {
        let names = fs.readdir().join(", ");
        println!("[vfs] {} -> {}({})", mp, fs.name(), names);
    }
}

// 子实验 1：经统一接口 open/read 一个 RamFs 文件。
fn check_vfs(vfs: &Vfs) -> bool {
    let mut ok = true;
    match vfs.open("/hello") {
        Some(of) => {
            let data = vfs.read(&of);
            if data != b"hi, vfs\n" {
                println!("VFS_FAIL /hello 内容不符: {:?}", String::from_utf8_lossy(&data));
                ok = false;
            }
        }
        None => {
            println!("VFS_FAIL open(\"/hello\") 失败（resolve 或 ramfs.lookup 没实现？）");
            ok = false;
        }
    }
    if ok {
        println!("VFS_PASS");
    }
    ok
}

// 子实验 2：挂载第二个 FS；跨过挂载点的路径进入它。
fn check_mount(vfs: &Vfs) -> bool {
    let mut ok = true;
    // "/hello" 停在根挂载 RamFs；"/dev/zero" 必须跨过 "/dev" 进入 DevFs。
    match vfs.resolve("/hello") {
        Some((mi, rel)) => {
            if vfs.mounts[mi].1.name() != "ramfs" || rel != "/hello" {
                println!("MOUNT_BAD /hello 应路由 ramfs rel=/hello，得 {} rel={}", vfs.mounts[mi].1.name(), rel);
                ok = false;
            }
        }
        None => {
            println!("MOUNT_FAIL resolve(\"/hello\") 没找到挂载");
            ok = false;
        }
    }
    match vfs.resolve("/dev/zero") {
        Some((mi, rel)) => {
            // 关键：选了更长的 "/dev" 而非根 "/"，且相对路径剥成 "/zero"。
            if vfs.mounts[mi].1.name() != "devfs" {
                println!("MOUNT_FAIL /dev/zero 没跨过挂载点：路由到 {} 而非 devfs（最长前缀没选对？）", vfs.mounts[mi].1.name());
                ok = false;
            }
            if rel != "/zero" {
                println!("MOUNT_BAD /dev/zero 在 devfs 内相对路径应=/zero，得 {}", rel);
                ok = false;
            }
        }
        None => {
            println!("MOUNT_FAIL resolve(\"/dev/zero\") 没找到挂载");
            ok = false;
        }
    }
    if ok {
        println!("MOUNT_PASS");
    }
    ok
}

// 子实验 3：同一接口背后不同 FS 行为——/dev/null 吞写、/dev/zero 读全 0。
fn check_devfs(vfs: &mut Vfs) -> bool {
    let mut ok = true;

    // /dev/null：写入被吞（返回写了 5），但读出为空（无存储）。
    match vfs.open("/dev/null") {
        Some(of) => {
            let w = vfs.write(&of, b"hello");
            if w != 5 {
                println!("DEVFS_BAD write(/dev/null,\"hello\") 应报告写 5，得 {}", w);
                ok = false;
            }
            let r = vfs.read(&of);
            if !r.is_empty() {
                println!("DEVFS_FAIL /dev/null 应吞掉写入读出空，却读出 {} 字节", r.len());
                ok = false;
            }
        }
        None => {
            println!("DEVFS_FAIL open(\"/dev/null\") 失败");
            ok = false;
        }
    }

    // /dev/zero：读出恰好 ZERO_READ_LEN 个 0 字节。
    match vfs.open("/dev/zero") {
        Some(of) => {
            let r = vfs.read(&of);
            if r.len() != ZERO_READ_LEN || r.iter().any(|&b| b != 0) {
                println!("DEVFS_FAIL /dev/zero 应读出 {} 个全 0，得 {:?}", ZERO_READ_LEN, r);
                ok = false;
            }
        }
        None => {
            println!("DEVFS_FAIL open(\"/dev/zero\") 失败");
            ok = false;
        }
    }

    // 对照：RamFs 的 /hello 是真存储——写进去读得出（与 /dev/null 相反）。
    if let Some(of) = vfs.open("/hello") {
        vfs.write(&of, b"changed");
        if vfs.read(&of) != b"changed" {
            println!("DEVFS_FAIL ramfs /hello 应真存储：写 changed 后应读出 changed");
            ok = false;
        }
    }

    if ok {
        println!("DEVFS_PASS");
    }
    ok
}

// 子实验 4：操作按路径被路由到正确 FS（路由 ≠ 存在性）。
fn check_dispatch(vfs: &Vfs) -> bool {
    let mut ok = true;
    // (路径, 期望落到的 FS 名, 该路径是否真存在可 open)
    let cases: [(&str, &str, bool); 6] = [
        ("/hello", "ramfs", true),
        ("/motd", "ramfs", true),
        ("/dev/null", "devfs", true),
        ("/dev/zero", "devfs", true),
        ("/dev/missing", "devfs", false), // 路由到 devfs，但该设备不存在
        ("/ghost", "ramfs", false),       // 路由到 ramfs，但无此文件
    ];
    for (path, want_fs, want_exist) in cases {
        match vfs.fs_name(path) {
            Some(got) if got == want_fs => {}
            other => {
                println!("DISPATCH_FAIL {} 应路由到 {}，得 {:?}", path, want_fs, other);
                ok = false;
            }
        }
        let exist = vfs.open(path).is_some();
        if exist != want_exist {
            println!("DISPATCH_BAD {} 存在性应={}，得={}", path, want_exist, exist);
            ok = false;
        }
    }
    if ok {
        println!("DISPATCH_PASS");
    }
    ok
}

fn main() {
    let mut vfs = build_vfs();
    dump(&vfs);

    let mut all = true;
    all &= check_vfs(&vfs);
    all &= check_mount(&vfs);
    all &= check_devfs(&mut vfs);
    all &= check_dispatch(&vfs);

    if all {
        println!("ALL_PASS");
    } else {
        std::process::exit(1);
    }
}
