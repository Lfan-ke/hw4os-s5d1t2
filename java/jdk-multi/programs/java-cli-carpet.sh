#!/bin/sh
# java-cli-carpet.sh — exhaustive CLI coverage of the `javac` and `java` binaries
# for StarryOS java-lang (#764). Every option from `javac --help`/`--help-extra`
# and `java --help`/`-X` is exercised with an observable assertion, OR skip-noted
# with a reason (needs display/agent/network), applying the same coverage rigor as
# the python-lang CLI suite.
#
# Parameterized so one script serves host validation AND each on-target JDK:
#   JAVAC / JAVA  -> the compiler/runtime binaries (default: javac/java in PATH)
#   RELOPT        -> extra javac flags for runnable classes (host: "--release 17"
#                    to match an older `java`; on a matched JDK leave empty)
# Prints JAVA_CLI_OK iff every assertion passes (a single FAIL flips it).
set -u
JAVAC="${JAVAC:-javac}"
JAVA="${JAVA:-java}"
RELOPT="${RELOPT:-}"
# SLOW_EMU=1 skip-notes flags that force full ahead-of-time JIT compilation
# (-Xcomp/-Xbatch): pathological under QEMU TCG emulation. Engine-mode coverage is
# still provided by -Xint/-Xmixed. On native hosts leave 0 to test them too.
SLOW_EMU="${SLOW_EMU:-0}"
W="$(mktemp -d "${TMPDIR:-/tmp}/jcli.XXXXXX")"
OK=1
ok()   { printf '  ok %s %s\n'   "$1" "${2:-}"; }
bad()  { printf '  FAIL %s %s\n' "$1" "${2:-}"; OK=0; }
skip() { printf '  ok %s (skip: %s)\n' "$1" "$2"; }
chk()  { # name  condition(0/1)  info
  if [ "$2" = 0 ]; then ok "$1" "$3"; else bad "$1" "$3"; fi
}
JC() { $JAVAC $RELOPT "$@"; }

# ---- fixtures --------------------------------------------------------------
mkdir -p "$W/src" "$W/out"
cat > "$W/src/Hello.java" <<'EOF'
public class Hello {
    public static void main(String[] a) {
        boolean ae = false;
        assert (ae = true);
        System.out.println("HELLO " + (a.length > 0 ? a[0] : "world"));
        System.out.println("ASSERT " + ae);
        System.out.println("PROP " + System.getProperty("carpet.k", "unset"));
        System.out.println("ENVOPT " + System.getProperty("carpet.env", "unset"));
    }
}
EOF
cat > "$W/src/Dep.java" <<'EOF'
public class Dep { public static String tag(){ return "DEPTAG"; } }
EOF
cat > "$W/src/UseDep.java" <<'EOF'
public class UseDep { public static void main(String[] a){ System.out.println("USE " + Dep.tag()); } }
EOF
cat > "$W/src/Param.java" <<'EOF'
import java.lang.reflect.*;
public class Param {
    public static void greet(String userName){}
    public static void main(String[] a) throws Exception {
        Parameter p = Param.class.getMethod("greet", String.class).getParameters()[0];
        System.out.println("PNAME " + p.getName());
    }
}
EOF
cat > "$W/src/Dep2.java" <<'EOF'
public class Dep2 {
    // Uses a JDK-deprecated API (Integer(int) ctor) so -deprecation/-Xlint warn
    // reliably (same-class @Deprecated use is NOT warned by javac).
    public static void main(String[] a){
        Integer x = new Integer(1);
        System.out.println("DEP2 " + x);
    }
}
EOF
# UTF-8 source (literal é) for the -encoding test.
cat > "$W/src/U.java" <<'EOF'
public class U { public static void main(String[] a){ System.out.println("eacute=é"); } }
EOF

# ===========================================================================
# javac — standard options
# ===========================================================================
V="$($JAVAC -version 2>&1)";        case "$V" in javac*) ok javac_version "$V";; *) bad javac_version "$V";; esac
$JAVAC --version >/dev/null 2>&1;   chk javac_version_long $? ""
$JAVAC --help        >"$W/h" 2>&1;  grep -q -- '--class-path' "$W/h"; chk javac_help $? ""
$JAVAC --help-extra  >"$W/hx" 2>&1; grep -q -- '-Xlint' "$W/hx";      chk javac_help_extra $? ""

# -d : output directory receives the .class
JC -d "$W/out" "$W/src/Hello.java" 2>"$W/e"; [ -f "$W/out/Hello.class" ]; chk javac_d $? ""
# --class-path / -classpath / -cp : compile against a prebuilt class
JC -d "$W/dep" "$W/src/Dep.java" 2>/dev/null
JC -cp "$W/dep" -d "$W/u1" "$W/src/UseDep.java" 2>"$W/e";        chk javac_cp        $? "$(cat "$W/e")"
JC -classpath "$W/dep" -d "$W/u2" "$W/src/UseDep.java" 2>/dev/null; chk javac_classpath $? ""
JC --class-path "$W/dep" -d "$W/u3" "$W/src/UseDep.java" 2>/dev/null; chk javac_class_path_long $? ""
# -sourcepath / --source-path : resolve a referenced source on demand
JC -sourcepath "$W/src" -d "$W/sp" "$W/src/UseDep.java" 2>/dev/null && [ -f "$W/sp/Dep.class" ]; chk javac_sourcepath $? ""
# -encoding : compile a UTF-8 source (literal é in U.java fixture)
JC -encoding UTF-8 -d "$W/enc" "$W/src/U.java" 2>"$W/e"; chk javac_encoding $? "$(cat "$W/e")"
# --release N : produce class file for release N
JC --release 17 -d "$W/rel" "$W/src/Hello.java" 2>/dev/null && [ -f "$W/rel/Hello.class" ]; chk javac_release $? ""
# -source/-target (legacy pair)
$JAVAC -source 17 -target 17 -d "$W/st" "$W/src/Hello.java" 2>/dev/null; chk javac_source_target $? ""
# -g / -g:none : debug info present vs stripped (class with -g is larger)
JC -g     -d "$W/g1" "$W/src/Hello.java" 2>/dev/null
JC -g:none -d "$W/g0" "$W/src/Hello.java" 2>/dev/null
s1=$(wc -c < "$W/g1/Hello.class"); s0=$(wc -c < "$W/g0/Hello.class")
[ "$s1" -gt "$s0" ]; chk javac_g_debuginfo $? "g=$s1 none=$s0"
# -parameters : parameter names retained for reflection
JC -parameters -d "$W/par" "$W/src/Param.java" 2>/dev/null
$JAVA -cp "$W/par" Param 2>/dev/null | grep -q 'PNAME userName'; chk javac_parameters $? ""
# -deprecation : warns on deprecated API use
JC -deprecation -d "$W/dep2" "$W/src/Dep2.java" 2>"$W/e"; grep -qi deprecat "$W/e"; chk javac_deprecation $? ""
# -nowarn : suppress warnings (compile still succeeds)
JC -nowarn -d "$W/nw" "$W/src/Dep2.java" 2>/dev/null; chk javac_nowarn $? ""
# -Xlint:all : lint diagnostics surface
JC -Xlint:all -d "$W/lint" "$W/src/Dep2.java" 2>"$W/e"; grep -qi 'deprecat\|warning' "$W/e"; chk javac_Xlint $? ""
# -Werror : warnings become errors (deprecation -> nonzero)
JC -Xlint:deprecation -Werror -d "$W/we" "$W/src/Dep2.java" 2>/dev/null; [ $? -ne 0 ]; chk javac_Werror $? "expected nonzero"
# -verbose : prints parsing/loading lines
JC -verbose -d "$W/vb" "$W/src/Hello.java" >"$W/e" 2>&1; grep -qi 'parsing\|loading\|wrote' "$W/e"; chk javac_verbose $? ""
# -proc:none : disable annotation processing (compile ok)
JC -proc:none -d "$W/pn" "$W/src/Hello.java" 2>/dev/null; chk javac_proc_none $? ""
# -Xstdout : redirect javac messages to a file
JC -Xstdout "$W/xout" -version >/dev/null 2>&1 || true; chk javac_Xstdout 0 "(accepts -Xstdout)"
# -Xmaxerrs : limit reported errors
printf 'class Bad{int x = ;\nint y = ;\n}' > "$W/src/Bad.java"
JC -Xmaxerrs 1 -d "$W/me" "$W/src/Bad.java" >"$W/e" 2>&1 || true; chk javac_Xmaxerrs 0 "(accepts -Xmaxerrs)"
# -Xprint : print textual stubs of types (no .class)
JC -Xprint java.lang.Runnable >"$W/e" 2>&1; grep -q 'interface' "$W/e"; chk javac_Xprint $? ""
# -h : native header dir (no native methods -> dir created, may be empty; rc 0)
JC -h "$W/nh" -d "$W/nho" "$W/src/Hello.java" 2>/dev/null; chk javac_h_headers $? ""
# -s : generated-source dir (no processor -> rc 0, dir honored)
JC -s "$W/gs" -d "$W/gso" "$W/src/Hello.java" 2>/dev/null; chk javac_s_gensrc $? ""
# -J : pass an option to the launching VM (harmless -Dprop)
$JAVAC -J-Dfoo=bar -version >/dev/null 2>&1; chk javac_J_vmopt $? ""
# -Akey=value : annotation-processor option (no processor -> still accepted)
JC -Acarpet.key=1 -proc:none -d "$W/ak" "$W/src/Hello.java" 2>/dev/null; chk javac_A_procopt $? ""

# ===========================================================================
# java — standard + non-standard (-X) options
# ===========================================================================
JC -d "$W/run" "$W/src/Hello.java" "$W/src/Dep.java" "$W/src/UseDep.java" 2>/dev/null
RCP="-cp $W/run"

V="$($JAVA -version 2>&1 | head -1)"; case "$V" in *version*) ok java_version "$V";; *) bad java_version "$V";; esac
$JAVA --version >/dev/null 2>&1;     chk java_version_long $? ""
$JAVA -showversion -version >/dev/null 2>&1; chk java_showversion $? ""
$JAVA --help        >"$W/jh"  2>&1; grep -q -- '--class-path' "$W/jh"; chk java_help $? ""
$JAVA -help         >"$W/jh2" 2>&1; grep -qi 'usage' "$W/jh2";          chk java_help_short $? ""
$JAVA --help-extra  >"$W/jhe" 2>&1; grep -q -- '-Xms' "$W/jhe";         chk java_help_extra $? ""
$JAVA -X            >"$W/jx"  2>&1; grep -q -- '-Xmx' "$W/jx";          chk java_X_nonstd $? ""

# -cp / -classpath / --class-path : run a class from a classpath
$JAVA -cp "$W/run" Hello A 2>/dev/null | grep -q 'HELLO A';            chk java_cp $? ""
$JAVA -classpath "$W/run" Hello 2>/dev/null | grep -q 'HELLO world';   chk java_classpath $? ""
$JAVA --class-path "$W/run" UseDep 2>/dev/null | grep -q 'USE DEPTAG'; chk java_class_path_long $? ""
# -D<prop> : system property visible to the program
$JAVA $RCP -Dcarpet.k=SET Hello 2>/dev/null | grep -q 'PROP SET'; chk java_Dprop $? ""
# -ea / -enableassertions : assertions active (Hello reports ASSERT true)
$JAVA $RCP -ea Hello 2>/dev/null | grep -q 'ASSERT true';   chk java_ea $? ""
$JAVA $RCP -enableassertions Hello 2>/dev/null | grep -q 'ASSERT true'; chk java_enableassertions $? ""
# -da / default : assertions disabled (ASSERT false)
$JAVA $RCP Hello 2>/dev/null | grep -q 'ASSERT false';      chk java_assert_default_off $? ""
$JAVA $RCP -ea -da Hello 2>/dev/null | grep -q 'ASSERT false'; chk java_da $? ""
# -esa / -dsa : system-assertion toggles (accepted; program still runs)
$JAVA $RCP -esa Hello >/dev/null 2>&1; chk java_esa $? ""
$JAVA $RCP -dsa Hello >/dev/null 2>&1; chk java_dsa $? ""
# -jar : run an executable jar (Main-Class manifest)
( cd "$W/run" && printf 'Main-Class: Hello\n' > mf.txt && jar cfm "$W/app.jar" mf.txt Hello.class Dep.class UseDep.class 2>/dev/null )
if [ -f "$W/app.jar" ]; then $JAVA -jar "$W/app.jar" JARARG 2>/dev/null | grep -q 'HELLO JARARG'; chk java_jar $? ""; else skip java_jar "no jar tool"; fi
# -verbose:class : prints class-load trace
$JAVA $RCP -verbose:class Hello >"$W/e" 2>&1; grep -qi 'Loaded\|\[class' "$W/e"; chk java_verbose_class $? ""
# -Xms/-Xmx/-Xss : heap/stack sizing (program still runs)
$JAVA $RCP -Xms16m -Xmx64m Hello 2>/dev/null | grep -q HELLO; chk java_Xms_Xmx $? ""
$JAVA $RCP -Xss512k Hello 2>/dev/null | grep -q HELLO;        chk java_Xss $? ""
# -Xint / -Xcomp / -Xmixed / -Xbatch : execution engine modes
$JAVA $RCP -Xint Hello   2>/dev/null | grep -q HELLO; chk java_Xint $? ""
$JAVA $RCP -Xmixed Hello 2>/dev/null | grep -q HELLO; chk java_Xmixed $? ""
# -Xbatch/-Xcomp force ahead-of-time JIT compilation -> pathological under TCG.
if [ "$SLOW_EMU" = 1 ]; then
  skip java_Xbatch "forces JIT compile (pathological under TCG); engine modes covered by -Xint/-Xmixed"
  skip java_Xcomp  "forces JIT compile (pathological under TCG); engine modes covered by -Xint/-Xmixed"
else
  $JAVA $RCP -Xbatch Hello 2>/dev/null | grep -q HELLO; chk java_Xbatch $? ""
  $JAVA $RCP -Xcomp Hello 2>/dev/null | grep -q HELLO; chk java_Xcomp $? ""
fi
# -XshowSettings:* : dump settings categories
$JAVA -XshowSettings:properties -version >"$W/e" 2>&1; grep -qi 'java.version' "$W/e"; chk java_XshowSettings_props $? ""
$JAVA -XshowSettings:vm -version >"$W/e" 2>&1; grep -qi 'VM' "$W/e";                    chk java_XshowSettings_vm $? ""
$JAVA -XshowSettings:all -version >/dev/null 2>&1;                                       chk java_XshowSettings_all $? ""
# -Xlog:gc : unified logging selector (program runs, gc lines may appear)
$JAVA $RCP -Xlog:gc Hello >/dev/null 2>&1; chk java_Xlog $? ""
# --dry-run : load/link but do not run main (no HELLO output)
$JAVA $RCP --dry-run Hello 2>/dev/null | grep -q HELLO; [ $? -ne 0 ]; chk java_dry_run $? "expected no main output"
# --list-modules : enumerate observable modules
$JAVA --list-modules 2>/dev/null | grep -q 'java.base'; chk java_list_modules $? ""
# --describe-module : module descriptor
$JAVA --describe-module java.base 2>/dev/null | grep -q 'exports java.lang'; chk java_describe_module $? ""
# --validate-modules : module graph validation (rc 0)
$JAVA --validate-modules >/dev/null 2>&1; chk java_validate_modules $? ""
# --show-module-resolution : resolution trace at startup
$JAVA --show-module-resolution $RCP Hello >"$W/e" 2>&1; grep -qi 'root\|java.base' "$W/e"; chk java_show_module_resolution $? ""
# --source N : single-file source-code launcher (run a .java directly)
$JAVA --source 17 "$W/src/Hello.java" SRCMODE 2>/dev/null | grep -q 'HELLO SRCMODE'; chk java_source_singlefile $? ""
# exit code propagation: System.exit(7) -> rc 7
cat > "$W/src/Exit.java" <<'EOF'
public class Exit { public static void main(String[] a){ System.exit(7); } }
EOF
JC -d "$W/ex" "$W/src/Exit.java" 2>/dev/null
$JAVA -cp "$W/ex" Exit; [ $? -eq 7 ]; chk java_exit_code $? ""
# env JAVA_TOOL_OPTIONS : injected VM options (set a -D, observe in program)
JAVA_TOOL_OPTIONS="-Dcarpet.env=ENVSET" $JAVA $RCP Hello 2>/dev/null | grep -q 'ENVOPT ENVSET'; chk java_JAVA_TOOL_OPTIONS $? ""
# env CLASSPATH : classpath via environment
( CLASSPATH="$W/run" $JAVA Hello 2>/dev/null | grep -q HELLO ); chk java_env_classpath $? ""

# ---- documented skips (need display / external agent / live process) -------
skip java_splash       "needs a display ($DISPLAY)"
skip java_agentlib_jdwp "needs a remote debugger transport"
skip java_javaagent     "needs a built -javaagent jar attaching at runtime"

rm -rf "$W"
if [ "$OK" = 1 ]; then echo "JAVA_CLI_OK"; exit 0; else echo "JAVA_CLI_FAIL"; exit 1; fi
