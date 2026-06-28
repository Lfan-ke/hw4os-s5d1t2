package org.starry.dod;

import jakarta.ws.rs.GET;
import jakarta.ws.rs.Path;
import jakarta.ws.rs.Produces;
import jakarta.ws.rs.core.MediaType;

/* DoD-C / P6: a trivial Jakarta EE JAX-RS resource, deployed into WildFly's own EE
 * runtime components (RESTEasy JAX-RS impl on the Undertow Servlet container — the exact
 * stack the WildFly app server ships). Returns WILDFLY_OK for the loopback self-test. */
@Path("/hello")
public class HelloResource {
    @GET
    @Produces(MediaType.TEXT_PLAIN)
    public String hello() {
        return "WILDFLY_OK";
    }
}
