package demo;

import java.lang.reflect.Method;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Duration;
import java.time.LocalDate;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicLong;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.stream.Collectors;
import java.util.stream.IntStream;

import demo.Shapes.Circle;
import demo.Shapes.Rect;
import demo.Shapes.Shape;
import demo.Shapes.Triangle;

/**
 * A dependency-free Java 17 workload that exercises a broad slice of the JVM:
 * sealed types + records, streams/collectors, java.util.concurrent
 * (ExecutorService + CompletableFuture + atomics), reflection, regex,
 * java.time, and NIO file IO. Builds offline (no external deps) so it can be
 * driven by both Maven and Gradle without network access.
 */
public class App {

    public static void main(String[] args) throws Exception {
        System.out.println("CDEMO_START java=" + System.getProperty("java.version")
                + " arch=" + System.getProperty("os.arch"));

        sealedAndStreams();
        concurrency();
        reflection();
        regexAndTime();
        nioRoundTrip();

        System.out.println("CDEMO_DONE");
    }

    /** Sealed interface dispatch + records + stream collectors. */
    static void sealedAndStreams() {
        List<Shape> shapes = List.of(
                new Circle(2.0), new Rect(3.0, 4.0), new Triangle(6.0, 2.0),
                new Circle(1.0), new Rect(5.0, 5.0));
        double total = Shapes.totalArea(shapes);

        Map<String, Long> byKind = shapes.stream()
                .collect(Collectors.groupingBy(s -> s.getClass().getSimpleName(),
                        Collectors.counting()));

        System.out.printf("CDEMO_AREA total=%.3f kinds=%s%n", total, byKind);
    }

    /** ExecutorService + CompletableFuture + atomic accumulation. */
    static void concurrency() throws Exception {
        ExecutorService pool = Executors.newFixedThreadPool(4);
        AtomicLong acc = new AtomicLong();
        List<CompletableFuture<Long>> futures = new ArrayList<>();
        for (int t = 0; t < 8; t++) {
            final int base = t * 1000;
            futures.add(CompletableFuture.supplyAsync(() -> {
                long local = IntStream.range(base, base + 1000).asLongStream().sum();
                acc.addAndGet(local);
                return local;
            }, pool));
        }
        long combined = futures.stream().map(CompletableFuture::join)
                .mapToLong(Long::longValue).sum();
        pool.shutdown();
        System.out.println("CDEMO_CONC combined=" + combined + " atomic=" + acc.get()
                + " match=" + (combined == acc.get()));
    }

    /** Reflection: enumerate this class's static methods. */
    static void reflection() {
        long staticMethods = java.util.Arrays.stream(App.class.getDeclaredMethods())
                .filter(m -> java.lang.reflect.Modifier.isStatic(m.getModifiers()))
                .map(Method::getName)
                .distinct()
                .count();
        System.out.println("CDEMO_REFLECT staticMethods=" + staticMethods);
    }

    /** Regex extraction + java.time arithmetic. */
    static void regexAndTime() {
        String log = "2026-01-01 INFO start; 2026-03-15 WARN spike; 2026-05-20 INFO end";
        Pattern p = Pattern.compile("(\\d{4}-\\d{2}-\\d{2})\\s+(\\w+)");
        Matcher m = p.matcher(log);
        List<LocalDate> dates = new ArrayList<>();
        while (m.find()) {
            dates.add(LocalDate.parse(m.group(1)));
        }
        Duration span = Duration.between(
                dates.get(0).atStartOfDay(),
                dates.get(dates.size() - 1).atStartOfDay());
        System.out.println("CDEMO_REGEX_TIME events=" + dates.size()
                + " spanDays=" + span.toDays());
    }

    /** NIO write + read round trip. */
    static void nioRoundTrip() throws Exception {
        Path p = Path.of(System.getProperty("java.io.tmpdir", "/tmp"), "cdemo_io.txt");
        String payload = IntStream.rangeClosed(1, 50)
                .mapToObj(Integer::toString)
                .collect(Collectors.joining(","));
        Files.writeString(p, payload);
        String back = Files.readString(p);
        System.out.println("CDEMO_NIO bytes=" + back.length() + " ok=" + payload.equals(back));
    }
}
