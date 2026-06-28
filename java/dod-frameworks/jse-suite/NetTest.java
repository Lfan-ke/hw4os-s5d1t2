import java.net.*;
import java.io.*;
import java.util.*;

/* DoD-B network coverage on starry (#764). starry net stack = axnet-ng/smoltcp.
 * Covers: NetworkInterface enumeration (exercises SIOCGIFCONF, the #223 fix),
 * TCP loopback (ServerSocket+Socket echo), UDP loopback (DatagramSocket),
 * loopback name resolution. No GUI, no external network — all 127.0.0.1. */
public class NetTest {
    public static void main(String[] args) throws Exception {
        int ok = 0, fail = 0;

        // 1) NetworkInterface enumeration — drives ioctl(SIOCGIFCONF) in the JDK.
        try {
            int n = 0;
            for (NetworkInterface ni : Collections.list(NetworkInterface.getNetworkInterfaces())) {
                n++;
                System.out.println("  iface=" + ni.getName() + " up=" + ni.isUp() + " lo=" + ni.isLoopback());
            }
            if (n > 0) { ok++; System.out.println("NIF ok (" + n + " interfaces)"); }
            else { fail++; System.out.println("NIF FAIL (0 interfaces)"); }
        } catch (Exception e) { fail++; System.out.println("NIF EXC " + e); }

        // 2) TCP loopback echo (ServerSocket + Socket).
        try {
            ServerSocket ss = new ServerSocket(0, 1, InetAddress.getByName("127.0.0.1"));
            int port = ss.getLocalPort();
            Thread srv = new Thread(() -> {
                try (Socket s = ss.accept()) {
                    BufferedReader r = new BufferedReader(new InputStreamReader(s.getInputStream()));
                    PrintWriter w = new PrintWriter(s.getOutputStream(), true);
                    w.println("echo:" + r.readLine());
                } catch (Exception ignored) {}
            });
            srv.start();
            try (Socket c = new Socket("127.0.0.1", port)) {
                new PrintWriter(c.getOutputStream(), true).println("hello");
                String resp = new BufferedReader(new InputStreamReader(c.getInputStream())).readLine();
                if ("echo:hello".equals(resp)) { ok++; System.out.println("TCP ok"); }
                else { fail++; System.out.println("TCP FAIL " + resp); }
            }
            srv.join(3000); ss.close();
        } catch (Exception e) { fail++; System.out.println("TCP EXC " + e); }

        // 3) UDP loopback datagram.
        try (DatagramSocket srv = new DatagramSocket(0, InetAddress.getByName("127.0.0.1"));
             DatagramSocket cli = new DatagramSocket()) {
            int port = srv.getLocalPort();
            byte[] msg = "ping".getBytes();
            cli.send(new DatagramPacket(msg, msg.length, InetAddress.getByName("127.0.0.1"), port));
            byte[] buf = new byte[16];
            DatagramPacket rp = new DatagramPacket(buf, buf.length);
            srv.setSoTimeout(3000);
            srv.receive(rp);
            if ("ping".equals(new String(rp.getData(), 0, rp.getLength()))) { ok++; System.out.println("UDP ok"); }
            else { fail++; System.out.println("UDP FAIL"); }
        } catch (Exception e) { fail++; System.out.println("UDP EXC " + e); }

        // 4) loopback name resolution.
        try {
            InetAddress lo = InetAddress.getByName("localhost");
            if (lo.isLoopbackAddress()) { ok++; System.out.println("RESOLVE ok " + lo); }
            else { fail++; System.out.println("RESOLVE FAIL " + lo); }
        } catch (Exception e) { fail++; System.out.println("RESOLVE EXC " + e); }

        System.out.println("NET_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0) System.out.println("NET_DONE");
    }
}
