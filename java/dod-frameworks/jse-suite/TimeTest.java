import java.time.*;
import java.time.format.*;
import java.time.temporal.*;
import java.util.*;

/* DoD: java.time — LocalDate/LocalDateTime/Instant/Duration/Period/ZonedDateTime/
 * DateTimeFormatter/ChronoUnit。纯 stdlib 自校验。 */
public class TimeTest {
    static int ok = 0, fail = 0;
    static void check(boolean c, String n) { if (c) ok++; else { fail++; System.out.println("FAIL " + n); } }

    public static void main(String[] args) {
        LocalDate d = LocalDate.of(2026, 5, 21);
        check(d.getDayOfWeek() == DayOfWeek.THURSDAY, "localdate-dow");
        check(d.plusDays(11).equals(LocalDate.of(2026, 6, 1)), "localdate-plus");
        check(d.isLeapYear() == false, "leap-year");

        LocalDateTime dt = LocalDateTime.of(2026, 5, 21, 10, 30, 0);
        check(dt.getHour() == 10 && dt.getMinute() == 30, "localdatetime");
        check(dt.format(DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm")).equals("2026-05-21 10:30"), "formatter");
        check(LocalDateTime.parse("2026-05-21T10:30:00").equals(dt), "parse-iso");

        Duration dur = Duration.ofHours(2).plusMinutes(30);
        check(dur.toMinutes() == 150, "duration");
        Period p = Period.between(LocalDate.of(2026, 1, 1), LocalDate.of(2026, 5, 21));
        check(p.getMonths() == 4 && p.getDays() == 20, "period");

        Instant i1 = Instant.ofEpochSecond(1000);
        Instant i2 = i1.plusSeconds(60);
        check(Duration.between(i1, i2).getSeconds() == 60, "instant");

        check(ChronoUnit.DAYS.between(LocalDate.of(2026, 5, 1), LocalDate.of(2026, 5, 21)) == 20, "chronounit");

        ZonedDateTime z = ZonedDateTime.of(dt, ZoneOffset.UTC);
        check(z.getOffset() == ZoneOffset.UTC, "zoneddatetime");

        System.out.println("TIME_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0) System.out.println("TIME_DONE");
    }
}
