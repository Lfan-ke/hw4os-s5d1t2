package org.starry.dod;

import io.netty.bootstrap.Bootstrap;
import io.netty.bootstrap.ServerBootstrap;
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import io.netty.channel.*;
import io.netty.channel.nio.NioEventLoopGroup;
import io.netty.channel.socket.SocketChannel;
import io.netty.channel.socket.nio.NioServerSocketChannel;
import io.netty.channel.socket.nio.NioSocketChannel;

import java.net.InetSocketAddress;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;

/* DoD-C: Netty TCP echo (NIO transport — pure Java, exercises starry epoll/Selector
 * + non-blocking sockets via netty's event loop). Server echoes; client verifies.
 * NETTY_DONE on pass. NIO (not native epoll) since starry has no netty-native musl lib. */
public class NettyDemo {
    public static void main(String[] args) throws Exception {
        NioEventLoopGroup boss = new NioEventLoopGroup(1);
        NioEventLoopGroup worker = new NioEventLoopGroup(1);
        CompletableFuture<String> got = new CompletableFuture<>();
        try {
            ServerBootstrap sb = new ServerBootstrap();
            sb.group(boss, worker).channel(NioServerSocketChannel.class)
              .childHandler(new ChannelInitializer<SocketChannel>() {
                  protected void initChannel(SocketChannel ch) {
                      ch.pipeline().addLast(new ChannelInboundHandlerAdapter() {
                          public void channelRead(ChannelHandlerContext ctx, Object msg) {
                              ctx.writeAndFlush(msg); // echo
                          }
                      });
                  }
              });
            Channel server = sb.bind(new InetSocketAddress("127.0.0.1", 0)).sync().channel();
            int port = ((InetSocketAddress) server.localAddress()).getPort();
            System.out.println("netty listening on 127.0.0.1:" + port);

            Bootstrap cb = new Bootstrap();
            cb.group(worker).channel(NioSocketChannel.class)
              .handler(new ChannelInitializer<SocketChannel>() {
                  protected void initChannel(SocketChannel ch) {
                      ch.pipeline().addLast(new ChannelInboundHandlerAdapter() {
                          public void channelActive(ChannelHandlerContext ctx) {
                              ctx.writeAndFlush(Unpooled.copiedBuffer("netty-ping", StandardCharsets.UTF_8));
                          }
                          public void channelRead(ChannelHandlerContext ctx, Object msg) {
                              ByteBuf b = (ByteBuf) msg;
                              got.complete(b.toString(StandardCharsets.UTF_8));
                              b.release();
                          }
                      });
                  }
              });
            cb.connect(new InetSocketAddress("127.0.0.1", port)).sync();

            String resp = got.get(10, TimeUnit.SECONDS);
            if ("netty-ping".equals(resp)) System.out.println("NETTY_DONE");
            else System.out.println("NETTY FAIL: " + resp);
            server.close();
        } finally {
            boss.shutdownGracefully(); worker.shutdownGracefully();
        }
        System.exit(0);
    }
}
