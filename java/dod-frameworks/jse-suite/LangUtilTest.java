import java.util.*;
import java.nio.*;
import java.nio.charset.StandardCharsets;

/* DoD: java.lang 显式(String/StringBuilder/Math/包装类/Thread/Exception) +
 * java.util 杂项(Random 确定性/Scanner/BitSet/StringTokenizer) + java.nio.ByteBuffer。 */
public class LangUtilTest {
    static int ok = 0, fail = 0;
    static void check(boolean c, String n) { if (c) ok++; else { fail++; System.out.println("FAIL " + n); } }

    public static void main(String[] args) throws Exception {
        // java.lang String / StringBuilder
        String s = "Hello,World";
        check(s.substring(6).equals("World") && s.split(",").length == 2 && s.toUpperCase().equals("HELLO,WORLD"), "string");
        StringBuilder sb = new StringBuilder(); for (int i = 0; i < 5; i++) sb.append(i);
        check(sb.reverse().toString().equals("43210"), "stringbuilder");
        // Math + wrappers
        check(Math.max(3, 7) == 7 && Math.abs(-5) == 5 && (long) Math.pow(2, 10) == 1024, "math");
        check(Integer.parseInt("ff", 16) == 255 && Long.toBinaryString(5).equals("101") && Double.parseDouble("3.14") == 3.14, "wrappers");
        check(Integer.bitCount(7) == 3 && Integer.numberOfLeadingZeros(1) == 31, "integer-bits");
        // Thread (java.lang)
        int[] x = {0}; Thread t = new Thread(() -> x[0] = 42); t.start(); t.join();
        check(x[0] == 42, "thread");
        // Exception chaining
        String msg = "";
        try { try { throw new IllegalStateException("inner"); } catch (Exception e) { throw new RuntimeException("outer", e); } }
        catch (RuntimeException e) { msg = e.getMessage() + "/" + e.getCause().getMessage(); }
        check(msg.equals("outer/inner"), "exception-chain");

        // java.util Random (deterministic with seed)
        Random r1 = new Random(42), r2 = new Random(42);
        check(r1.nextInt(1000) == r2.nextInt(1000), "random-seeded");
        // Scanner over string
        Scanner sc = new Scanner("10 20 hello");
        check(sc.nextInt() == 10 && sc.nextInt() == 20 && sc.next().equals("hello"), "scanner");
        // BitSet
        BitSet bs = new BitSet(); bs.set(1); bs.set(3); bs.set(5);
        check(bs.cardinality() == 3 && bs.get(3) && !bs.get(2), "bitset");
        // StringTokenizer
        StringTokenizer st = new StringTokenizer("a-b-c", "-");
        check(st.countTokens() == 3, "stringtokenizer");

        // java.nio ByteBuffer
        ByteBuffer buf = ByteBuffer.allocate(16);
        buf.putInt(0xCAFEBABE).putLong(42L); buf.flip();
        check(buf.getInt() == 0xCAFEBABE && buf.getLong() == 42L, "bytebuffer");
        ByteBuffer be = ByteBuffer.allocate(4).order(ByteOrder.BIG_ENDIAN); be.putInt(1); be.flip();
        check(be.get(0) == 0 && be.get(3) == 1, "bytebuffer-endian");
        check("héllo".getBytes(StandardCharsets.UTF_8).length == 6, "charset-utf8");

        System.out.println("LANGUTIL_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0) System.out.println("LANGUTIL_DONE");
    }
}
