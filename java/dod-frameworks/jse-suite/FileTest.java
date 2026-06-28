import java.io.*;
import java.nio.file.*;
import java.nio.file.attribute.*;
import java.util.*;

/* DoD: 文件/IO — NIO Files/Path, 读写/追加, 临时文件/目录, list/walk, RandomAccessFile,
 * stat/属性, BufferedReader/Writer 行 IO, 删除。验证 starry 文件系统 syscall(open/openat/
 * read/write/stat/mkdir/unlink/lseek...)。全在 /tmp 下。 */
public class FileTest {
    static int ok = 0, fail = 0;
    static void check(boolean c, String n) { if (c) ok++; else { fail++; System.out.println("FAIL " + n); } }

    public static void main(String[] args) throws Exception {
        Path dir = Files.createTempDirectory("dodfs");
        // write + read string (NIO)
        Path f = dir.resolve("a.txt");
        Files.writeString(f, "hello\n");
        check(Files.readString(f).equals("hello\n"), "nio-write-read-string");
        // append
        Files.writeString(f, "world\n", StandardOpenOption.APPEND);
        check(Files.readString(f).equals("hello\nworld\n"), "append");
        // readAllLines
        check(Files.readAllLines(f).equals(List.of("hello", "world")), "readalllines");
        // bytes
        Files.write(dir.resolve("b.bin"), new byte[]{1, 2, 3, 4});
        check(Files.readAllBytes(dir.resolve("b.bin")).length == 4, "readallbytes");
        // size + exists + attributes (stat)
        check(Files.size(f) == 12 && Files.exists(f), "size-exists");
        BasicFileAttributes attr = Files.readAttributes(f, BasicFileAttributes.class);
        check(attr.isRegularFile() && !attr.isDirectory(), "stat-attributes");
        // directory create + list
        Files.createDirectories(dir.resolve("sub/deep"));
        check(Files.isDirectory(dir.resolve("sub/deep")), "mkdirs");
        long n = Files.list(dir).count();
        check(n >= 3, "list-dir");
        // walk
        Files.writeString(dir.resolve("sub/c.txt"), "x");
        long walked = Files.walk(dir).filter(Files::isRegularFile).count();
        check(walked >= 3, "walk");
        // RandomAccessFile seek
        File raf = dir.resolve("raf.dat").toFile();
        try (RandomAccessFile r = new RandomAccessFile(raf, "rw")) {
            r.writeInt(0xCAFE); r.writeInt(0xBABE); r.seek(4);
            check(r.readInt() == 0xBABE, "randomaccessfile-seek");
        }
        // BufferedReader/Writer line IO
        Path lf = dir.resolve("lines.txt");
        try (BufferedWriter w = Files.newBufferedWriter(lf)) { for (int i = 0; i < 100; i++) { w.write("line" + i); w.newLine(); } }
        try (BufferedReader br = Files.newBufferedReader(lf)) { check(br.lines().count() == 100, "buffered-line-io"); }
        // copy + move
        Files.copy(f, dir.resolve("a-copy.txt"), StandardCopyOption.REPLACE_EXISTING);
        Files.move(dir.resolve("a-copy.txt"), dir.resolve("a-moved.txt"), StandardCopyOption.REPLACE_EXISTING);
        check(Files.exists(dir.resolve("a-moved.txt")) && !Files.exists(dir.resolve("a-copy.txt")), "copy-move");
        // delete
        Files.delete(dir.resolve("b.bin"));
        check(!Files.exists(dir.resolve("b.bin")), "delete");

        System.out.println("FILE_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0) System.out.println("FILE_DONE");
    }
}
