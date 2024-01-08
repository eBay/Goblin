package com.ebay.payments.raft.client;

import com.ebay.goblin.client.utils.GoblinLogLevel;
import com.ebay.payments.raft.client.pool.PoolType;
import io.grpc.netty.shaded.io.netty.handler.ssl.SslContext;
import lombok.Builder;
import lombok.Data;

@Data
@Builder
public class RaftClusterClientConfig {
    private GoblinLogLevel logLevel;

    /**
     * Goblin servers' addresses.
     * The format is:
     * "<node_id_1>@:,<node_id_2>@:,<node_id_3>@:,..."
     * e.g.
     * "1@192.168.1.19:50055,2@192.168.1.20:50055,3@192.168.1.21:50055"
     */
    private String clusterInfo;

    /**
     * true: enable TLS in the connection between the client and Goblin server.
     * false: disable TLS in the connection between the client and Goblin server.
     */
    private boolean tlsEnabled;

    /**
     * io.grpc.netty.shaded.io.netty.handler.ssl.SslContext object for TLS connection. Mandatory if goblin.grpc.tls.enable is true.
     */
    private SslContext sslContext;

    /**
     * The dns resolver for clusterInfo
     */
    private ClusterInfoResolver clusterInfoResolver;

    private Long routeVersion;

    /**
     * The overall timeout (in milliseconds) of a method call for the client.
     * Typically, a RaftClusterClient method invocation may including several gRPC calls to the Raft server,
     * when the current server instance is not available or the leader was changed.
     *
     * RaftClusterClient ensures that a method call throws RaftInvokeTimeoutException exception shortly after
     * timeoutInMilliSeconds milliseconds when the method was not completed.
     */
    @Builder.Default
    private long timeoutInMilliSeconds = 10000L;

    /**
     * The gPRC timeout (in milliseconds) of a single API call between the client and Goblin server.
     * Change this value only when the connection between the server and client is very unstable.
     * In most cases, set #timeoutInMilliSeconds# for the overall timeout configuration.
     */
    @Builder.Default
    private long stubTimeoutInMilliSeconds = 500;

    @Builder.Default
    private long largePayloadStubTimeoutInMilliSeconds = 2000;

    /**
     * pool type means how to select client to connect
     * leaderOnly: always found leader to connect
     * RoundRobin: if pick(leader=false) will round robin to pick a client
     * InSyncReplica: if pick(leader=false) will round robin to pick a client but it should be closed to leader enough
     */
    @Builder.Default
    private PoolType poolType = PoolType.LeaderOnly;

    /**
     * if client offset + minGap >= leader commit index, the client is a ISR client
     */
    @Builder.Default
    private long ISRValidMinGap = 100;

    /**
     * Time interval to refresh ISR list
     */
    @Builder.Default
    private long ISRRefreshTime = 1000 * 60 * 2; // 2min;

    /**
     * Lease for an ISR List becomes invalid
     */
    @Builder.Default
    private long ISRLeaseTime = 1000 * 60 * 10; // 10 mins

    @Builder.Default
    private long intervalPerRound = 5;

    /**
     * convert 1@a:100,2@b:100 to 1@0.0.0.0:100,2@0.0.0.1:100
     * @return
     */
    public String getResolvedClusterInfo() {
        if (clusterInfoResolver == null) {
            return clusterInfo;
        }
        return clusterInfoResolver.getResolvedClusterInfo(clusterInfo);
    }

    /**
     * Goblin servers' addresses.
     * The format is:
     * "<node_id_1>@fqdn:port,<node_id_2>@fqdn:port,<node_id_3>@fqdn:port,..."
     */
    public String getClusterInfo() {
        if (clusterInfoResolver == null) {
            return clusterInfo;
        }
        return clusterInfoResolver.getClusterInfo(clusterInfo);
    }
}
