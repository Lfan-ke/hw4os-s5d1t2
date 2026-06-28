import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import java.util.concurrent.locks.*;
import java.util.stream.*;

// P7: deeper concurrency/parallelism stress (beyond basic ConcurrencyTest):
// high thread count + heavy contention + parallel compute + coordination.
// Exercises starry single-core scheduler / futex / park-unpark harder.
public class ConcurrencyDeep {
    static int ok = 0, fail = 0;
    static void chk(boolean c, String m) { if (c) ok++; else { fail++; System.out.println("FAIL " + m); } }

    public static void main(String[] args) throws Exception {
        // 1. 64 threads × 10000 incs on AtomicLong + lock-guarded long → exact sum
        final int T = 64, N = 10000;
        AtomicLong atom = new AtomicLong();
        final long[] guarded = {0};
        ReentrantLock lock = new ReentrantLock();
        CountDownLatch start = new CountDownLatch(1), done = new CountDownLatch(T);
        ExecutorService ex = Executors.newFixedThreadPool(T);
        for (int t = 0; t < T; t++) ex.submit(() -> {
            try { start.await(); } catch (InterruptedException ignored) {}
            for (int i = 0; i < N; i++) {
                atom.incrementAndGet();
                lock.lock(); try { guarded[0]++; } finally { lock.unlock(); }
            }
            done.countDown();
        });
        start.countDown();
        chk(done.await(120, TimeUnit.SECONDS), "64-thread latch completes");
        chk(atom.get() == (long) T * N, "atomic exact=" + atom.get());
        chk(guarded[0] == (long) T * N, "lock-guarded exact=" + guarded[0]);
        ex.shutdown();

        // 2. parallelStream sum over 2,000,000 → Gauss
        long n = 2_000_000L;
        long sum = LongStream.rangeClosed(1, n).parallel().sum();
        chk(sum == n * (n + 1) / 2, "parallelStream sum");

        // 3. ForkJoinPool RecursiveTask parallel sum
        ForkJoinPool fj = new ForkJoinPool(8);
        long fjSum = fj.invoke(new SumTask(1, 1_000_000));
        chk(fjSum == 500000L * 1000001L, "forkjoin sum=" + fjSum);
        fj.shutdown();

        // 4. CompletableFuture chain (compose+combine) across pool
        CompletableFuture<Integer> cf = CompletableFuture.supplyAsync(() -> 10)
                .thenApplyAsync(x -> x * 2)
                .thenComposeAsync(x -> CompletableFuture.supplyAsync(() -> x + 5))
                .thenCombineAsync(CompletableFuture.supplyAsync(() -> 100), Integer::sum);
        chk(cf.get(30, TimeUnit.SECONDS) == 125, "completablefuture chain");

        // 5. Producer/consumer via BlockingQueue, 4 prod × 4 cons, 40000 items
        BlockingQueue<Integer> q = new LinkedBlockingQueue<>(1000);
        AtomicInteger consumed = new AtomicInteger();
        int P = 4, C = 4, items = 10000;
        ExecutorService pc = Executors.newFixedThreadPool(P + C);
        CountDownLatch pdone = new CountDownLatch(P);
        for (int p = 0; p < P; p++) pc.submit(() -> {
            try { for (int i = 0; i < items; i++) q.put(i); } catch (InterruptedException ignored) {}
            pdone.countDown();
        });
        AtomicBoolean run = new AtomicBoolean(true);
        List<Future<?>> cons = new ArrayList<>();
        for (int c = 0; c < C; c++) cons.add(pc.submit(() -> {
            try { while (run.get() || !q.isEmpty()) { Integer v = q.poll(50, TimeUnit.MILLISECONDS); if (v != null) consumed.incrementAndGet(); } }
            catch (InterruptedException ignored) {}
        }));
        pdone.await(120, TimeUnit.SECONDS); run.set(false);
        for (Future<?> f : cons) f.get(60, TimeUnit.SECONDS);
        chk(consumed.get() == P * items, "producer/consumer all consumed=" + consumed.get());
        pc.shutdown();

        // 6. ConcurrentHashMap under contention: 16 threads merge-incrementing 100 keys
        ConcurrentHashMap<Integer, Integer> chm = new ConcurrentHashMap<>();
        ExecutorService ce = Executors.newFixedThreadPool(16);
        CountDownLatch cl = new CountDownLatch(16);
        for (int t = 0; t < 16; t++) ce.submit(() -> {
            for (int i = 0; i < 100 * 50; i++) chm.merge(i % 100, 1, Integer::sum);
            cl.countDown();
        });
        chk(cl.await(120, TimeUnit.SECONDS), "chm latch");
        chk(chm.values().stream().mapToInt(Integer::intValue).sum() == 16 * 100 * 50, "chm merge exact");
        ce.shutdown();

        System.out.println("CONCURRENCY_DEEP ok=" + ok + " fail=" + fail);
        if (fail == 0) System.out.println("CONCURRENCY_DEEP_DONE");
    }

    static class SumTask extends RecursiveTask<Long> {
        final int lo, hi;
        SumTask(int lo, int hi) { this.lo = lo; this.hi = hi; }
        protected Long compute() {
            if (hi - lo <= 10000) { long s = 0; for (int i = lo; i <= hi; i++) s += i; return s; }
            int mid = (lo + hi) >>> 1;
            SumTask l = new SumTask(lo, mid); l.fork();
            SumTask r = new SumTask(mid + 1, hi);
            return r.compute() + l.join();
        }
    }
}
