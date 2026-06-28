import java.util.*;
import java.util.function.*;

/* DoD: java17 语言高级 + 语法糖 — records/compact ctor, sealed+permits, instanceof 模式,
 * switch 表达式, 文本块, var, lambda/4类方法引用, try-with-resources, 多重catch, varargs,
 * 自动装箱, 增强for, 钻石操作符, enum带方法, 泛型通配符。无 preview 特性。 */
public class SyntaxTest {
    static int ok = 0, fail = 0;
    static void check(boolean c, String n) { if (c) ok++; else { fail++; System.out.println("FAIL " + n); } }

    record Pair<A, B>(A a, B b) { Pair { Objects.requireNonNull(a); } }      // record + compact ctor + generics
    sealed interface Shape permits Circle, Square {}
    record Circle(double r) implements Shape {}
    record Square(double s) implements Shape {}
    static double area(Shape sh) {                                          // instanceof pattern
        if (sh instanceof Circle c) return Math.PI * c.r() * c.r();
        else if (sh instanceof Square sq) return sq.s() * sq.s();
        return 0;
    }
    enum Op { ADD { int ap(int a, int b) { return a + b; } }, MUL { int ap(int a, int b) { return a * b; } }; abstract int ap(int a, int b); }
    static class Res implements AutoCloseable { static boolean closed = false; public void close() { closed = true; } }
    @SafeVarargs static <T> int count(T... xs) { return xs.length; }

    public static void main(String[] args) {
        // record + generics
        Pair<String, Integer> p = new Pair<>("x", 1);
        check(p.a().equals("x") && p.b() == 1 && p.equals(new Pair<>("x", 1)), "record-generics-equals");
        // sealed + instanceof pattern
        check(Math.abs(area(new Square(3)) - 9.0) < 1e-9, "sealed-instanceof-pattern");
        // switch expression with yield
        int d = 5; String s = switch (d) { case 1, 2, 3, 4, 5 -> "low"; default -> { yield "high"; } };
        check(s.equals("low"), "switch-yield");
        // enum with abstract method
        check(Op.ADD.ap(2, 3) == 5 && Op.MUL.ap(2, 3) == 6, "enum-method");
        // lambda + functional interfaces
        Function<Integer, Integer> sq = x -> x * x;
        BiFunction<Integer, Integer, Integer> add = Integer::sum;            // static method ref
        Supplier<List<String>> mk = ArrayList::new;                          // ctor ref
        check(sq.apply(4) == 16 && add.apply(3, 4) == 7 && mk.get().isEmpty(), "lambda-methodref");
        // bound + unbound instance method refs
        String hi = "Hello";
        Supplier<Integer> len = hi::length;                                  // bound
        Function<String, String> up = String::toUpperCase;                   // unbound
        check(len.get() == 5 && up.apply("ab").equals("AB"), "methodref-instance");
        // try-with-resources
        try (Res r = new Res()) { /* use */ }
        check(Res.closed, "try-with-resources");
        // multi-catch
        String mc = "";
        try { if (args.length < 99) throw new NumberFormatException(); }
        catch (NumberFormatException | ArrayIndexOutOfBoundsException e) { mc = "caught"; }
        check(mc.equals("caught"), "multi-catch");
        // varargs + autoboxing + enhanced for + diamond
        check(count("a", "b", "c") == 3, "varargs");
        Map<String, List<Integer>> m = new HashMap<>();                      // diamond
        m.computeIfAbsent("k", k -> new ArrayList<>()).add(1);
        int total = 0; for (int x : m.get("k")) total += x;                  // autobox + enhanced-for
        check(total == 1, "autobox-enhancedfor-diamond");
        // text block
        String tb = """
            {"k":1}""";
        check(tb.equals("{\"k\":1}"), "text-block");
        // generics wildcard
        List<? extends Number> nums = List.of(1, 2.0, 3L);
        double sum = 0; for (Number x : nums) sum += x.doubleValue();
        check(Math.abs(sum - 6.0) < 1e-9, "wildcard-generics");

        System.out.println("SYNTAX_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0) System.out.println("SYNTAX_DONE");
    }
}
