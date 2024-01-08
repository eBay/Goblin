package com.ebay.payments.raft.client;

import io.grpc.netty.shaded.io.netty.handler.ssl.SslContext;
import lombok.Builder;
import lombok.Data;

@Data
@Builder
public class RaftClientStubConfig {

    private String instanceId;
    private String host;
    private String domainName;
    private int port;
    private int netAdminPort;
    private boolean tlsEnabled;
    private SslContext sslContext;
    @Builder.Default
    private long timeoutInMilliSeconds = 500;
    @Builder.Default
    private long largePayloadTimeoutInMilliSeconds = 2000;
}
