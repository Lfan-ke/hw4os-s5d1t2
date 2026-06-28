import java.io.*;
import java.util.*;
import java.util.concurrent.*;

/* DoD: java.lang.ProcessBuilder / Runtime.exec — 子进程 fork/exec/wait + stdout 捕获 +
 * 退出码 + 管道 + 环境变量。强力 exercise starry 的 fork/execve/waitpid/pipe syscall。
 * 用 busybox 内建命令(rootfs 必有)。 */
public class ProcessTest {
    static int ok = 0, fail = 0;
    static void check(boolean c, String n) { if (c) ok++; else { fail++; System.out.println("FAIL " + n); } }
    static String run(ProcessBuilder pb) throws Exception {
        pb.redirectErrorStream(true);
        Process p = pb.start();
        String out = new String(p.getInputStream().readAllBytes()).trim();
        p.waitFor(10, TimeUnit.SECONDS);
        return out;
    }

    public static void main(String[] args) throws Exception {
        // echo + stdout capture + exit code
        Process p = new ProcessBuilder("/bin/echo", "hello", "world").start();
        String out = new String(p.getInputStream().readAllBytes()).trim();
        int code = p.waitFor();
        check(out.equals("hello world") && code == 0, "echo-exitcode");

        // exit code propagation (false → 1, true → 0)
        check(new ProcessBuilder("/bin/false").start().waitFor() == 1, "false-exit1");
        check(new ProcessBuilder("/bin/true").start().waitFor() == 0, "true-exit0");

        // stdin → stdout pipe (cat)
        Process cat = new ProcessBuilder("/bin/cat").start();
        try (OutputStream os = cat.getOutputStream()) { os.write("piped-data\n".getBytes()); }
        String catOut = new String(cat.getInputStream().readAllBytes()).trim();
        cat.waitFor(10, TimeUnit.SECONDS);
        check(catOut.equals("piped-data"), "stdin-stdout-pipe");

        // environment variable passing
        ProcessBuilder pb = new ProcessBuilder("/bin/sh", "-c", "echo $DOD_VAR");
        pb.environment().put("DOD_VAR", "starry42");
        check(run(pb).equals("starry42"), "env-var");

        // multiple sequential spawns (fork/exec stress)
        int sum = 0;
        for (int i = 1; i <= 10; i++) {
            String r = run(new ProcessBuilder("/bin/sh", "-c", "echo " + i));
            sum += Integer.parseInt(r);
        }
        check(sum == 55, "sequential-spawns");

        System.out.println("PROCESS_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0) System.out.println("PROCESS_DONE");
    }
}
