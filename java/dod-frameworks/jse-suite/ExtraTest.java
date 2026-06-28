import java.math.*;
import java.text.*;
import java.util.*;
import java.util.regex.*;
import java.util.zip.*;
import java.io.*;

/* DoD: 更多 stdlib — java.math(BigInteger/BigDecimal) + java.text(DecimalFormat/NumberFormat)
 * + java.util.regex(Pattern/Matcher) + java.util.zip(GZIP/Deflater/CRC32) + java.io 序列化. */
public class ExtraTest {
    static int ok = 0, fail = 0;
    static void check(boolean c, String n) { if (c) ok++; else { fail++; System.out.println("FAIL " + n); } }

    public static void main(String[] args) throws Exception {
        // java.math BigInteger / BigDecimal
        BigInteger f = BigInteger.ONE;
        for (int i = 1; i <= 20; i++) f = f.multiply(BigInteger.valueOf(i));
        check(f.toString().equals("2432902008176640000"), "biginteger-factorial");
        check(BigInteger.valueOf(2).pow(64).toString().equals("18446744073709551616"), "biginteger-pow");
        BigDecimal a = new BigDecimal("1.10"), b = new BigDecimal("2.20");
        check(a.add(b).compareTo(new BigDecimal("3.30")) == 0, "bigdecimal-add");
        check(BigDecimal.ONE.divide(new BigDecimal("3"), 5, RoundingMode.HALF_UP).toString().equals("0.33333"), "bigdecimal-div");

        // java.text
        DecimalFormat df = new DecimalFormat("#,##0.00");
        check(df.format(1234567.891).equals("1,234,567.89"), "decimalformat");
        check(NumberFormat.getPercentInstance(Locale.US).format(0.25).equals("25%"), "numberformat-pct");
        check(MessageFormat.format("{0}+{1}={2}", 1, 2, 3).equals("1+2=3"), "messageformat");

        // java.util.regex
        Matcher m = Pattern.compile("(\\d{4})-(\\d{2})-(\\d{2})").matcher("date 2026-05-21 end");
        check(m.find() && m.group(1).equals("2026") && m.group(3).equals("21"), "regex-groups");
        check("a1b2c3".replaceAll("\\d", "#").equals("a#b#c#"), "regex-replace");
        check(Pattern.compile(",").split("x,y,z").length == 3, "regex-split");

        // java.util.zip GZIP + CRC32 + Deflater
        byte[] data = "the quick brown fox ".repeat(50).getBytes();
        ByteArrayOutputStream bo = new ByteArrayOutputStream();
        try (GZIPOutputStream gz = new GZIPOutputStream(bo)) { gz.write(data); }
        byte[] comp = bo.toByteArray();
        check(comp.length < data.length, "gzip-compress");
        ByteArrayInputStream bi = new ByteArrayInputStream(comp);
        byte[] decomp = new GZIPInputStream(bi).readAllBytes();
        check(Arrays.equals(decomp, data), "gzip-roundtrip");
        CRC32 crc = new CRC32(); crc.update(data);
        check(crc.getValue() != 0, "crc32");

        // java.io serialization
        ByteArrayOutputStream so = new ByteArrayOutputStream();
        try (ObjectOutputStream oos = new ObjectOutputStream(so)) {
            oos.writeObject(new HashMap<>(Map.of("k", 42, "x", 7)));
        }
        try (ObjectInputStream ois = new ObjectInputStream(new ByteArrayInputStream(so.toByteArray()))) {
            @SuppressWarnings("unchecked") Map<String, Integer> r = (Map<String, Integer>) ois.readObject();
            check(r.get("k") == 42 && r.get("x") == 7, "serialization");
        }

        System.out.println("EXTRA_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0) System.out.println("EXTRA_DONE");
    }
}
