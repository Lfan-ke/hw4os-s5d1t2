package org.starry.dod

import io.ktor.http.*
import io.ktor.serialization.kotlinx.json.*
import io.ktor.server.application.*
import io.ktor.server.engine.*
import io.ktor.server.netty.*
import io.ktor.server.plugins.contentnegotiation.*
import io.ktor.server.request.*
import io.ktor.server.response.*
import io.ktor.server.routing.*
import kotlinx.serialization.Serializable

/* DoD-C / P6: Ktor (JetBrains Kotlin async web framework, Netty engine) on StarryOS.
 *
 * COMPREHENSIVE web-framework smoke test. Host-compiled fat jar (Kotlin compiled on the
 * host to dodge the on-starry kotlinc crash #237, exactly like exposed/springkt). Exercises
 * the Kotlin coroutines runtime + Netty NIO event loop + Ktor routing/plugin pipeline +
 * kotlinx.serialization JSON content-negotiation, end-to-end over the StarryOS net stack
 * (#223/#225) on the IPv4 loopback.
 *
 * This binary is LONG-RUNNING: it binds 127.0.0.1:18082, prints `KTOR_READY` once the
 * engine is accepting connections, then blocks. The StarryOS test harness (busybox wget)
 * drives every route from the guest shell and asserts the exact bodies / status codes,
 * then kills the JVM. (No in-process self-test: the harness IS the test.)
 *
 *   java -Xint -Xms64m -Xmx256m -jar ktor-demo.jar
 *
 * Routes:
 *   GET  /ping            -> 200 text/plain   "KTOR_PONG"
 *   GET  /add/{a}/{b}     -> 200 application/json  {"a":..,"b":..,"sum":..}
 *   GET  /add/{a}/{b}     -> 400 (non-integer path param) "BAD_INT"
 *   GET  /echo?msg=...    -> 200 application/json  {"echo":"<msg>","len":N}
 *   POST /sum  (JSON body {"nums":[..]}) -> 200 application/json {"total":..,"count":..}
 *   GET  /missing/...     -> 404 (Ktor default)   exercised by the harness
 */

@Serializable
data class AddResult(val a: Int, val b: Int, val sum: Int)

@Serializable
data class EchoResult(val echo: String, val len: Int)

@Serializable
data class SumRequest(val nums: List<Int>)

@Serializable
data class SumResult(val total: Int, val count: Int)

fun main() {
    // Ktor 2.3.x: embeddedServer(Netty, ...) returns an ApplicationEngine (NettyApplicationEngine).
    // (EmbeddedServer is the Ktor 3.x return type — do NOT use it here.)
    val server = embeddedServer(Netty, host = "127.0.0.1", port = 18082) {
        install(ContentNegotiation) { json() }
        routing {
            // 1) plain text + a stable success marker
            get("/ping") {
                call.respondText("KTOR_PONG", ContentType.Text.Plain)
            }
            // 2) path params + JSON content negotiation + 400 on bad input
            get("/add/{a}/{b}") {
                val a = call.parameters["a"]?.toIntOrNull()
                val b = call.parameters["b"]?.toIntOrNull()
                if (a == null || b == null) {
                    call.respondText("BAD_INT", ContentType.Text.Plain, HttpStatusCode.BadRequest)
                } else {
                    call.respond(AddResult(a, b, a + b))
                }
            }
            // 3) query param echo, JSON content negotiation
            get("/echo") {
                val msg = call.request.queryParameters["msg"] ?: ""
                call.respond(EchoResult(msg, msg.length))
            }
            // 4) JSON request body deserialization + JSON response
            post("/sum") {
                val req = call.receive<SumRequest>()
                call.respond(SumResult(req.nums.sum(), req.nums.size))
            }
        }
    }
    server.start(wait = false)
    // Tiny wait so the listening socket is bound before the harness starts polling.
    // Netty binds synchronously on start(), but print the marker only after a probe-free settle.
    Thread.sleep(500)
    println("KTOR_READY")
    System.out.flush()
    // Block forever; the StarryOS harness kills the JVM after asserting all routes.
    Thread.currentThread().join()
}
