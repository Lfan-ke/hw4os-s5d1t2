import java.util.*;
import java.util.stream.*;
import java.lang.annotation.*;
import java.lang.reflect.*;

/* DoD: 非 GUI 标准库 — streams/泛型(模板)/注解+反射/Optional/switch表达式/文本块/var。纯 stdlib 自校验。 */
public class StdlibTest {
    static int ok = 0, fail = 0;
    static void check(boolean c, String n) { if (c) ok++; else { fail++; System.out.println("FAIL " + n); } }

    @Retention(RetentionPolicy.RUNTIME) @interface Tag { String value(); }
    @Tag("demo") static class Annotated { }
    static <T extends Comparable<T>> T max(List<T> xs) { return xs.stream().max(Comparator.naturalOrder()).orElseThrow(); }

    public static void main(String[] args) {
        // streams: map/filter/reduce/collect/flatMap/groupingBy/IntStream
        int sumSq = IntStream.rangeClosed(1, 5).map(x -> x * x).sum();
        check(sumSq == 55, "intstream");
        List<Integer> evens = Stream.of(1, 2, 3, 4, 5, 6).filter(x -> x % 2 == 0).collect(Collectors.toList());
        check(evens.equals(List.of(2, 4, 6)), "filter-collect");
        Map<Boolean, List<Integer>> parts = Stream.iterate(1, x -> x + 1).limit(10).collect(Collectors.partitioningBy(x -> x % 2 == 0));
        check(parts.get(true).size() == 5, "partitioning");
        Map<Integer, List<String>> byLen = Stream.of("a", "bb", "cc", "ddd").collect(Collectors.groupingBy(String::length));
        check(byLen.get(2).equals(List.of("bb", "cc")), "groupingBy");
        List<Integer> flat = Stream.of(List.of(1, 2), List.of(3, 4)).flatMap(List::stream).collect(Collectors.toList());
        check(flat.equals(List.of(1, 2, 3, 4)), "flatMap");
        check(Stream.of("x", "y", "z").reduce("", String::concat).equals("xyz"), "reduce");

        // generics (模板)
        check(max(List.of(3, 7, 2)) == 7, "generics-max");

        // annotations + reflection
        Tag t = Annotated.class.getAnnotation(Tag.class);
        check(t != null && t.value().equals("demo"), "annotation-reflect");
        check(StdlibTest.class.getDeclaredMethods().length > 0, "reflect-methods");

        // Optional
        check(Optional.of(5).map(x -> x * 2).filter(x -> x > 5).get() == 10, "optional");
        check(Optional.empty().orElse("def").equals("def"), "optional-empty");

        // switch expression (Java 14+) + records-ish
        int day = 3;
        String kind = switch (day) { case 1, 7 -> "weekend"; default -> "weekday"; };
        check(kind.equals("weekday"), "switch-expr");

        // text block (Java 15+)
        String tb = """
            line1
            line2""";
        check(tb.lines().count() == 2, "text-block");

        // var + collectors joining
        var joined = Stream.of("a", "b", "c").collect(Collectors.joining("-", "[", "]"));
        check(joined.equals("[a-b-c]"), "var-joining");

        System.out.println("STDLIB_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0) System.out.println("STDLIB_DONE");
    }
}
