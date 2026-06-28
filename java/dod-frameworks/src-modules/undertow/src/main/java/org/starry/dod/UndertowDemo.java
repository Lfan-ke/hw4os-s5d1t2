package org.starry.dod;

import io.undertow.Undertow;
import io.undertow.util.Headers;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;

/* DoD-C / P6: Undertow embedded HTTP server (loopback self-test). 127.0.0.1 bind
 * (starry IPv6 stub). UNDERTOW_DONE on pass. */
public class UndertowDemo {
    public static void main(String[] args) {
        int ok = 0, fail = 0;
        Undertow server = null;
        try {
            server = Undertow.builder()
                    .addHttpListener(18081, "127.0.0.1")
                    .setHandler(exchange -> {
                        exchange.getResponseHeaders().put(Headers.CONTENT_TYPE, "text/plain");
                        exchange.getResponseSender().send("UNDERTOW_OK");
                    })
                    .build();
            server.start();
            // loopback GET with retry (starry net warmup)
            String body = null;
            long deadline = System.currentTimeMillis() + 60000;
            while (System.currentTimeMillis() < deadline) {
                try {
                    HttpURLConnection c = (HttpURLConnection) new URL("http://127.0.0.1:18081/").openConnection();
                    c.setConnectTimeout(5000); c.setReadTimeout(10000);
                    int code = c.getResponseCode();
                    try (BufferedReader r = new BufferedReader(new InputStreamReader(c.getInputStream()))) {
                        body = r.readLine();
                    }
                    if (code == 200) break;
                } catch (Exception e) { Thread.sleep(2000); }
            }
            if ("UNDERTOW_OK".equals(body)) ok++; else { fail++; System.out.println("FAIL body=" + body); }
        } catch (Throwable t) {
            fail++; System.out.println("UNDERTOW_ERR: " + t); t.printStackTrace();
        } finally {
            if (server != null) try { server.stop(); } catch (Throwable ignored) {}
        }
        System.out.println("UNDERTOW_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0 && ok == 1) System.out.println("UNDERTOW_DONE");
        System.exit(fail == 0 ? 0 : 1);
    }
}
