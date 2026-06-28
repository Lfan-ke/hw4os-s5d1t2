package org.starry.dod;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.context.ConfigurableApplicationContext;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RestController;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;

/* DoD-C JavaEE: Spring Boot (covers #764 spring + embedded tomcat). Starts the
 * full Spring context + embedded Tomcat on a fixed loopback port, then self-tests
 * a REST endpoint via HttpURLConnection. SPRING_DONE on pass. Run with
 *   java -Dserver.port=18080 -jar spring-demo.jar
 * Exercises Spring IoC/AOP/auto-config + Tomcat servlet container + net (#223/#225). */
@SpringBootApplication
@RestController
public class SpringDemo {
    @GetMapping("/ping")
    public String ping() { return "SPRING_OK"; }

    public static void main(String[] args) throws Exception {
        int port = Integer.getInteger("server.port", 18080);
        System.setProperty("server.port", String.valueOf(port));
        ConfigurableApplicationContext ctx = SpringApplication.run(SpringDemo.class, args);
        String body = null;
        try {
            // 慢架构（riscv64/loongarch 仿真 + -Xint）下 Spring Boot + 嵌入 Tomcat 启动远慢于
            // 原先单次 500ms sleep + 8s connect 超时 → 客户端在服务器就绪前放弃 = 假阴性.
            // 改为 poll/retry: 最多 120 次尝试, 每次失败后 sleep 1s, 总预算 ~120s.
            // 每次请求 connect/read 超时保持 5s. 只有真正 HTTP 200 + 期望 body 才算成功.
            final int MAX_ATTEMPTS = 120;
            Exception last = null;
            int code = -1;
            for (int attempt = 1; attempt <= MAX_ATTEMPTS && body == null; attempt++) {
                try {
                    HttpURLConnection c = (HttpURLConnection) new URL("http://127.0.0.1:" + port + "/ping").openConnection();
                    c.setConnectTimeout(5_000); c.setReadTimeout(5_000);
                    code = c.getResponseCode();
                    if (code == 200) {
                        try (BufferedReader r = new BufferedReader(new InputStreamReader(c.getInputStream()))) {
                            body = r.readLine();
                        }
                    } else {
                        last = new java.io.IOException("HTTP " + code);
                        Thread.sleep(1000);
                    }
                } catch (Exception e) {
                    last = e;
                    Thread.sleep(1000);
                }
            }
            if (body == null && last != null) System.out.println("SPRING probe last err: " + last);
        } finally {
            SpringApplication.exit(ctx);
        }
        if ("SPRING_OK".equals(body)) System.out.println("SPRING_DONE");
        else System.out.println("SPRING FAIL: " + body);
        System.exit(0);
    }
}
