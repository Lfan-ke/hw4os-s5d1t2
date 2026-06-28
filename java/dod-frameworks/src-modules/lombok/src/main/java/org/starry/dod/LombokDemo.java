package org.starry.dod;

import lombok.AllArgsConstructor;
import lombok.Builder;
import lombok.Data;
import lombok.EqualsAndHashCode;
import lombok.Getter;
import lombok.NoArgsConstructor;
import lombok.Setter;
import lombok.ToString;
import lombok.extern.java.Log;

import java.util.Arrays;
import java.util.List;

/* DoD-C: Project Lombok (编译期注解处理器). 验证 @Data/@Builder/@Getter/@Setter/
 * @ToString/@EqualsAndHashCode/@AllArgsConstructor/@NoArgsConstructor/@Log 生成的代码.
 * 编译期 only, 无运行时 native → 任何 arch (javac/JVM 通) 即通. LOMBOK_DONE on pass. */
@Log
public class LombokDemo {

    @Data
    @Builder
    @AllArgsConstructor
    @NoArgsConstructor
    static class Person {
        private String name;
        private int age;
        private List<String> tags;
    }

    @Getter
    @Setter
    @ToString
    @EqualsAndHashCode
    static class Box {
        private int x;
        private int y;
    }

    public static void main(String[] args) {
        int ok = 0, fail = 0;
        Person p = Person.builder().name("Alice").age(30).tags(Arrays.asList("a", "b")).build();
        if ("Alice".equals(p.getName()) && p.getAge() == 30) ok++; else { fail++; System.out.println("FAIL builder/getter"); }
        p.setAge(31);
        if (p.getAge() == 31) ok++; else { fail++; System.out.println("FAIL setter"); }
        Person p2 = Person.builder().name("Alice").age(31).tags(Arrays.asList("a", "b")).build();
        if (p.equals(p2) && p.hashCode() == p2.hashCode()) ok++; else { fail++; System.out.println("FAIL equals/hashCode"); }
        if (p.toString().contains("Alice")) ok++; else { fail++; System.out.println("FAIL toString"); }
        Box b = new Box();
        b.setX(1); b.setY(2);
        if (b.getX() == 1 && b.getY() == 2 && b.toString().contains("x=1")) ok++; else { fail++; System.out.println("FAIL Box"); }
        log.info("lombok @Log works");
        System.out.println("LOMBOK_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0) System.out.println("LOMBOK_DONE");
    }
}
