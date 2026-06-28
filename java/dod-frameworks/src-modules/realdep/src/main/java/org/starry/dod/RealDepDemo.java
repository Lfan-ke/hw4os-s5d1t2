package org.starry.dod;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.JsonNode;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ArrayListMultimap;
import com.google.common.collect.Multimap;
import com.google.common.base.Joiner;
import org.apache.commons.lang3.StringUtils;

import java.util.List;

/* DoD: a project that USES real third-party packages (#764 "使用一定的包"):
 * Jackson (JSON ser/de), Guava (immutable collections / multimap / joiner),
 * commons-lang3 (string utils). Built as a shaded fat jar so it runs on starry
 * with just `java -jar`, no classpath. */
public class RealDepDemo {
    record Point(int x, int y) {}

    public static void main(String[] args) throws Exception {
        int ok = 0, fail = 0;

        // Jackson: serialize a record to JSON and parse it back.
        ObjectMapper om = new ObjectMapper();
        String json = om.writeValueAsString(new Point(3, 4));
        JsonNode node = om.readTree(json);
        if (node.get("x").asInt() == 3 && node.get("y").asInt() == 4) { ok++; System.out.println("JACKSON ok " + json); }
        else { fail++; System.out.println("JACKSON FAIL " + json); }

        // Guava: immutable list + multimap + joiner.
        List<String> xs = ImmutableList.of("a", "b", "c");
        Multimap<String, Integer> mm = ArrayListMultimap.create();
        mm.put("k", 1); mm.put("k", 2);
        String joined = Joiner.on(",").join(xs);
        if ("a,b,c".equals(joined) && mm.get("k").size() == 2) { ok++; System.out.println("GUAVA ok " + joined + " " + mm.get("k")); }
        else { fail++; System.out.println("GUAVA FAIL"); }

        // commons-lang3: string utilities.
        if (StringUtils.isAllUpperCase("ABC") && "yzab".equals(StringUtils.rotate("abyz", 2))) { ok++; System.out.println("COMMONS ok"); }
        else { fail++; System.out.println("COMMONS FAIL " + StringUtils.rotate("abyz", 2)); }

        System.out.println("REALDEP_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0) System.out.println("REALDEP_DONE");
    }
}
