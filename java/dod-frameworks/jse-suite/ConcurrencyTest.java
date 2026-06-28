import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import java.util.concurrent.locks.*;
import java.util.stream.*;

/* DoD: 异步/并发 — ExecutorService/Future, CompletableFuture(异步链), ForkJoinPool,
 * parallelStream, ReentrantLock, atomics, CountDownLatch, Semaphore,
 * ConcurrentHashMap, BlockingQueue 生产者-消费者。验证 starry 调度/futex/线程语义。 */
public class ConcurrencyTest {
    static int ok = 0, fail = 0;
    static synchronized void check(boolean c, String n) { if (c) ok++; else { fail++; System.out.println("FAIL " + n); } }

    public static void main(String[] args) throws Exception {
        // ExecutorService + Future
        ExecutorService ex = Executors.newFixedThreadPool(4);
        List<Future<Integer>> fs = new ArrayList<>();
        for (int i = 1; i <= 8; i++) { final int k = i; fs.add(ex.submit(() -> k * k)); }
        int sum = 0; for (Future<Integer> f : fs) sum += f.get();
        check(sum == 204, "executor-future");

        // CompletableFuture async chain
        int r = CompletableFuture.supplyAsync(() -> 10, ex).thenApply(x -> x + 5).thenCompose(x -> CompletableFuture.supplyAsync(() -> x * 2, ex)).get();
        check(r == 30, "completablefuture");

        // ForkJoinPool parallel sum
        ForkJoinPool fj = new ForkJoinPool();
        long fjsum = fj.submit(() -> IntStream.rangeClosed(1, 1000).parallel().asLongStream().sum()).get();
        check(fjsum == 500500, "forkjoin");

        // parallelStream
        long psum = IntStream.rangeClosed(1, 10000).parallel().mapToLong(x -> x).sum();
        check(psum == 50005000L, "parallelstream");

        // ReentrantLock + shared counter
        ReentrantLock lock = new ReentrantLock();
        int[] counter = {0};
        CountDownLatch latch = new CountDownLatch(10);
        for (int i = 0; i < 10; i++) ex.submit(() -> { for (int j = 0; j < 1000; j++) { lock.lock(); try { counter[0]++; } finally { lock.unlock(); } } latch.countDown(); });
        latch.await(10, TimeUnit.SECONDS);
        check(counter[0] == 10000, "reentrantlock-latch");

        // AtomicInteger
        AtomicInteger ai = new AtomicInteger();
        CountDownLatch l2 = new CountDownLatch(8);
        for (int i = 0; i < 8; i++) ex.submit(() -> { for (int j = 0; j < 1000; j++) ai.incrementAndGet(); l2.countDown(); });
        l2.await(10, TimeUnit.SECONDS);
        check(ai.get() == 8000, "atomic");

        // Semaphore
        Semaphore sem = new Semaphore(2);
        check(sem.tryAcquire(2) && !sem.tryAcquire(), "semaphore");

        // ConcurrentHashMap
        ConcurrentHashMap<String, Integer> chm = new ConcurrentHashMap<>();
        CountDownLatch l3 = new CountDownLatch(4);
        for (int i = 0; i < 4; i++) ex.submit(() -> { for (int j = 0; j < 500; j++) chm.merge("k", 1, Integer::sum); l3.countDown(); });
        l3.await(10, TimeUnit.SECONDS);
        check(chm.get("k") == 2000, "concurrenthashmap");

        // BlockingQueue producer/consumer
        BlockingQueue<Integer> bq = new ArrayBlockingQueue<>(16);
        AtomicInteger consumed = new AtomicInteger();
        Thread prod = new Thread(() -> { try { for (int i = 0; i < 100; i++) bq.put(i); } catch (Exception e) {} });
        Thread cons = new Thread(() -> { try { for (int i = 0; i < 100; i++) { bq.take(); consumed.incrementAndGet(); } } catch (Exception e) {} });
        cons.start(); prod.start(); prod.join(5000); cons.join(5000);
        check(consumed.get() == 100, "blockingqueue");

        ex.shutdown(); fj.shutdown();
        System.out.println("CONC_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0) System.out.println("CONC_DONE");
    }
}
