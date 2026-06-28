package org.starry.dod;

import jakarta.ws.rs.GET;
import jakarta.ws.rs.Path;
import jakarta.ws.rs.Produces;
import jakarta.ws.rs.core.MediaType;

/* DoD-C / P6: Quarkus (Supersonic Subatomic Java) JVM mode — minimal JAX-RS endpoint.
 * NOT native (GraalVM native-image is infeasible on starry; see SOURCES.md). Runs the
 * standard Quarkus runner JVM (quarkus-run.jar / -uber.jar) which boots the Vert.x HTTP
 * server + RESTEasy Reactive on a fixed loopback port. QuarkusSelfTest drives the GET. */
@Path("/ping")
public class PingResource {
    @GET
    @Produces(MediaType.TEXT_PLAIN)
    public String ping() {
        return "QUARKUS_OK";
    }
}
