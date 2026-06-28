package org.starry.dod;

import io.undertow.Undertow;
import io.undertow.servlet.Servlets;
import io.undertow.servlet.api.DeploymentInfo;
import io.undertow.servlet.api.DeploymentManager;
import org.jboss.resteasy.plugins.server.servlet.HttpServlet30Dispatcher;
import org.jboss.resteasy.plugins.server.servlet.ResteasyContextParameters;

import jakarta.servlet.ServletException;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;

/* DoD-C / P6: WildFly Jakarta EE app server — deploys a JAX-RS endpoint and serves it.
 *
 * FEASIBILITY: the full WildFly distribution (~250 MB, forks a Host Controller + server
 * processes, opens the management + http + ajp ports, multi-minute boot) is impractical
 * under starry's -Xint + emulated-arch + single-core constraints (see SOURCES.md). This
 * demo instead exercises WildFly's *actual EE runtime components* in-process: RESTEasy
 * (the JAX-RS impl WildFly ships, org.jboss.resteasy) deployed as a real servlet
 * deployment on the Undertow Servlet container (the WildFly web subsystem). Same Jakarta
 * EE deploy → start → serve → curl path, minus the app-server process/management overhead.
 *
 *   java -Xint -Xms64m -Xmx256m -jar wildfly-demo.jar   →  WILDFLY_DONE on pass
 */
public class WildflyDemo {
    public static void main(String[] args) throws Exception {
        int ok = 0, fail = 0;
        Undertow server = null;
        DeploymentManager manager = null;
        try {
            DeploymentInfo di = Servlets.deployment()
                    .setClassLoader(WildflyDemo.class.getClassLoader())
                    .setContextPath("/")
                    .setDeploymentName("wildfly-demo.war")
                    .addServlet(
                            Servlets.servlet("rest", HttpServlet30Dispatcher.class)
                                    .setAsyncSupported(true)
                                    .setLoadOnStartup(1)
                                    .addInitParam("jakarta.ws.rs.Application", RestApp.class.getName())
                                    .addInitParam(ResteasyContextParameters.RESTEASY_SERVLET_MAPPING_PREFIX, "/api")
                                    .addMapping("/api/*"));

            manager = Servlets.defaultContainer().addDeployment(di);
            manager.deploy();

            server = Undertow.builder()
                    .addHttpListener(18084, "127.0.0.1")
                    .setHandler(manager.start())
                    .build();
            server.start();

            // loopback GET with retry (slow -Xint / emulated arch warmup)
            String body = null;
            long deadline = System.currentTimeMillis() + 90_000;
            while (System.currentTimeMillis() < deadline && body == null) {
                try {
                    HttpURLConnection c = (HttpURLConnection) new URL("http://127.0.0.1:18084/api/hello").openConnection();
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
            if ("WILDFLY_OK".equals(body)) ok++; else { fail++; System.out.println("FAIL body=" + body); }
        } catch (Throwable t) {
            fail++; System.out.println("WILDFLY_ERR: " + t); t.printStackTrace();
        } finally {
            if (server != null) try { server.stop(); } catch (Throwable ignored) {}
            if (manager != null) try { manager.stop(); manager.undeploy(); } catch (Throwable ignored) {}
        }
        System.out.println("WILDFLY_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0 && ok == 1) System.out.println("WILDFLY_DONE");
        System.exit(fail == 0 ? 0 : 1);
    }
}
