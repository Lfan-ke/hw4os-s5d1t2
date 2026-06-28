package org.starry.dod;

import org.eclipse.jetty.server.Server;
import org.eclipse.jetty.server.ServerConnector;
import org.eclipse.jetty.server.Request;
import org.eclipse.jetty.server.handler.AbstractHandler;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;

/* DoD-C JavaEE: embedded Jetty HTTP server on starry. Starts on a loopback
 * port, serves a handler, then self-tests with an HttpURLConnection GET.
 * Exercises servlet container + net stack (#223) end-to-end. JETTY_DONE on pass. */
public class JettyDemo {
    public static void main(String[] args) throws Exception {
        Server server = new Server();
        ServerConnector connector = new ServerConnector(server);
        connector.setHost("127.0.0.1");
        connector.setPort(0);
        server.addConnector(connector);
        server.setHandler(new AbstractHandler() {
            @Override
            public void handle(String target, Request baseRequest,
                               HttpServletRequest request, HttpServletResponse response) throws IOException {
                response.setContentType("text/plain;charset=utf-8");
                response.setStatus(HttpServletResponse.SC_OK);
                response.getWriter().println("JETTY_OK");
                baseRequest.setHandled(true);
            }
        });
        server.start();
        int port = connector.getLocalPort();
        System.out.println("jetty listening on 127.0.0.1:" + port);

        String body = null;
        try {
            HttpURLConnection conn = (HttpURLConnection) new URL("http://127.0.0.1:" + port + "/").openConnection();
            conn.setConnectTimeout(5000);
            conn.setReadTimeout(5000);
            try (BufferedReader r = new BufferedReader(new InputStreamReader(conn.getInputStream()))) {
                body = r.readLine();
            }
        } finally {
            server.stop();
        }
        if ("JETTY_OK".equals(body)) System.out.println("JETTY_DONE");
        else System.out.println("JETTY FAIL: " + body);
    }
}
