import java.io.*;
import java.net.*;
import java.nio.*;
import java.nio.channels.*;
import java.nio.file.*;
import java.util.*;

/* DoD: java.nio.channels — FileChannel(map/transfer), Pipe, 非阻塞 SocketChannel +
 * Selector(epoll) loopback echo。强力 exercise starry 的 epoll/poll/非阻塞 socket syscall。 */
public class NioChannelTest {
    static int ok = 0, fail = 0;
    static void check(boolean c, String n) { if (c) ok++; else { fail++; System.out.println("FAIL " + n); } }

    public static void main(String[] args) throws Exception {
        // FileChannel write/read
        Path f = Files.createTempFile("nio", ".dat");
        try (FileChannel ch = FileChannel.open(f, java.nio.file.StandardOpenOption.WRITE)) {
            ch.write(ByteBuffer.wrap("channel-data".getBytes()));
        }
        try (FileChannel ch = FileChannel.open(f, java.nio.file.StandardOpenOption.READ)) {
            ByteBuffer buf = ByteBuffer.allocate(64);
            int n = ch.read(buf); buf.flip();
            check(new String(buf.array(), 0, n).equals("channel-data"), "filechannel");
        }

        // Pipe
        Pipe pipe = Pipe.open();
        pipe.sink().write(ByteBuffer.wrap("pipe-msg".getBytes()));
        ByteBuffer pb = ByteBuffer.allocate(16);
        int pn = pipe.source().read(pb);
        check(new String(pb.array(), 0, pn).equals("pipe-msg"), "nio-pipe");

        // Non-blocking SocketChannel + Selector (epoll) loopback echo
        ServerSocketChannel ssc = ServerSocketChannel.open();
        ssc.bind(new InetSocketAddress("127.0.0.1", 0));
        ssc.configureBlocking(false);
        int port = ((InetSocketAddress) ssc.getLocalAddress()).getPort();
        Selector sel = Selector.open();
        ssc.register(sel, SelectionKey.OP_ACCEPT);

        SocketChannel client = SocketChannel.open();
        client.configureBlocking(false);
        client.connect(new InetSocketAddress("127.0.0.1", port));

        String received = null;
        long deadline = System.currentTimeMillis() + 8000;
        boolean clientSent = false;
        while (System.currentTimeMillis() < deadline && received == null) {
            sel.select(500);
            for (Iterator<SelectionKey> it = sel.selectedKeys().iterator(); it.hasNext();) {
                SelectionKey k = it.next(); it.remove();
                if (k.isAcceptable()) {
                    SocketChannel s = ssc.accept();
                    if (s != null) { s.configureBlocking(false); s.register(sel, SelectionKey.OP_READ); }
                } else if (k.isReadable()) {
                    ByteBuffer b = ByteBuffer.allocate(32);
                    int r = ((SocketChannel) k.channel()).read(b);
                    if (r > 0) { b.flip(); received = new String(b.array(), 0, r); }
                }
            }
            if (!clientSent && client.finishConnect()) { client.write(ByteBuffer.wrap("nio-net".getBytes())); clientSent = true; }
        }
        check("nio-net".equals(received), "selector-socketchannel");
        client.close(); ssc.close(); sel.close();

        System.out.println("NIOCH_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0) System.out.println("NIOCH_DONE");
    }
}
