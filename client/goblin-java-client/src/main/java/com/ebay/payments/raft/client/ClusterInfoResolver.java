package com.ebay.payments.raft.client;

public interface ClusterInfoResolver {

    String getResolvedClusterInfo(String clusterInfo);

    String getClusterInfo(String clusterInfo);
}
