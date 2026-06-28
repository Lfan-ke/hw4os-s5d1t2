package org.starry.dod;

import io.quarkus.runtime.StartupEvent;
import jakarta.enterprise.context.ApplicationScoped;
import jakarta.enterprise.event.Observes;
import org.eclipse.microprofile.config.inject.ConfigProperty;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;

/* After Quarkus finishes startup (HTTP server bound), a daemon thread loopback-GETs the
 * JAX-RS endpoint and prints QUARKUS_DONE on success, then forces exit. A daemon thread is
 * used so the StartupEvent observer returns immediately (HTTP listener only becomes ready
 * once startup completes). Retry loop tolerates slow -Xint / emulated arch warmup. */
@ApplicationScoped
public class SelfTest {

    @ConfigProperty(name = "quarkus.http.port", defaultValue = "18083")
    int port;

    void onStart(@Observes StartupEvent ev) {
        Thread t = new Thread(this::probe, "quarkus-selftest");
        t.setDaemon(true);
        t.start();
    }

    private void probe() {
        int ok = 0, fail = 0;
        String body = null;
        try {
            long deadline = System.currentTimeMillis() + 60_000;
            while (System.currentTimeMillis() < deadline && body == null) {
                try {
                    HttpURLConnection c = (HttpURLConnection) new URL("http://127.0.0.1:" + port + "/ping").openConnection();
                    c.setConnectTimeout(5_000); c.setReadTimeout(10_000);
                    int code = c.getResponseCode();
                    if (code == 200) {
                        try (BufferedReader r = new BufferedReader(new InputStreamReader(c.getInputStream()))) {
                            body = r.readLine();
                        }
                    } else {
                        Thread.sleep(2_000);
                    }
                } catch (Exception e) {
                    Thread.sleep(2_000);
                }
            }
            if ("QUARKUS_OK".equals(body)) ok++; else { fail++; System.out.println("FAIL body=" + body); }
        } catch (Throwable th) {
            fail++; System.out.println("QUARKUS_ERR: " + th); th.printStackTrace();
        }
        System.out.println("QUARKUS_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0 && ok == 1) System.out.println("QUARKUS_DONE");
        Runtime.getRuntime().halt(fail == 0 ? 0 : 1);
    }
}
