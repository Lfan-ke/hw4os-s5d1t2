package org.starry.dod;

import jakarta.ws.rs.ApplicationPath;
import jakarta.ws.rs.core.Application;

/* JAX-RS application root: maps the JAX-RS servlet at /api so /api/hello resolves. */
@ApplicationPath("/api")
public class RestApp extends Application {
}
