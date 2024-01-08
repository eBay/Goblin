package com.ebay.payments.raft.client;

import lombok.Builder;
import lombok.Getter;
import lombok.ToString;

@Builder
@ToString
@Getter
public class ConnectionContext {

    private String contextId;
    private Long knownVersion;
    private Long routeVersion;

    public synchronized void updateKnownVersion(long newKnownVersion) {
        if (knownVersion == null || newKnownVersion > knownVersion) {
            knownVersion = newKnownVersion;
        }
    }

    public synchronized void updateRouteVersion(long newRouteVersion) {
        if (routeVersion == null || newRouteVersion > routeVersion ) {
            routeVersion = newRouteVersion;
        }
    }

    public ConnectionContext clone() {
        return ConnectionContext.builder()
                .contextId(contextId)
                .knownVersion(knownVersion)
                .routeVersion(routeVersion)
                .build();
    }
}
