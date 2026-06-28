package demo;

import java.util.List;

/** Java 17 sealed interface + records + pattern-style dispatch. */
public final class Shapes {
    private Shapes() {}

    public sealed interface Shape permits Circle, Rect, Triangle {}

    public record Circle(double r) implements Shape {}
    public record Rect(double w, double h) implements Shape {}
    public record Triangle(double base, double height) implements Shape {}

    /** Dispatch over sealed type via Java 17 instanceof pattern matching. */
    public static double area(Shape s) {
        if (s instanceof Circle c) {
            return Math.PI * c.r() * c.r();
        } else if (s instanceof Rect r) {
            return r.w() * r.h();
        } else if (s instanceof Triangle t) {
            return 0.5 * t.base() * t.height();
        }
        throw new IllegalStateException("unknown shape: " + s);
    }

    public static double totalArea(List<Shape> shapes) {
        return shapes.stream().mapToDouble(Shapes::area).sum();
    }
}
