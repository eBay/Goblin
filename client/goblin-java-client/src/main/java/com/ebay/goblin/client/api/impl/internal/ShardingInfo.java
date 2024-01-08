package com.ebay.goblin.client.api.impl.internal;


import com.ebay.goblin.client.api.impl.StoreClusterClientImpl;
import com.ebay.goblin.client.exceptions.GoblinException;
import com.ebay.goblin.client.exceptions.GoblinInternalException;
import com.ebay.goblin.client.model.common.RouteResult;
import com.ebay.goblin.client.model.common.ShardCluster;
import com.ebay.payments.raft.client.RaftClusterClientConfig;
import com.google.common.collect.Maps;

import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReadWriteLock;
import java.util.concurrent.locks.ReentrantReadWriteLock;
import java.util.function.Supplier;
import java.util.stream.Collectors;

public class ShardingInfo {
    private volatile RouteResult routeInfo = new RouteResult();
    private volatile Map<String, StoreClusterClientImpl> clientMap = Maps.newConcurrentMap();
    private RaftClusterClientConfig.RaftClusterClientConfigBuilder configBuilder;
    private final ReadWriteLock lock = new ReentrantReadWriteLock();

    public ShardingInfo(RaftClusterClientConfig config) {
        this.configBuilder = createConfigBuilder(config);
    }

    private RaftClusterClientConfig.RaftClusterClientConfigBuilder createConfigBuilder(RaftClusterClientConfig config) {
        RaftClusterClientConfig.RaftClusterClientConfigBuilder builder = RaftClusterClientConfig.builder()
                .timeoutInMilliSeconds(config.getTimeoutInMilliSeconds())
                .stubTimeoutInMilliSeconds(config.getStubTimeoutInMilliSeconds())
                .largePayloadStubTimeoutInMilliSeconds(config.getLargePayloadStubTimeoutInMilliSeconds())
                .tlsEnabled(config.isTlsEnabled())
                .poolType(config.getPoolType())
                .ISRValidMinGap(config.getISRValidMinGap())
                .ISRLeaseTime(config.getISRLeaseTime())
                .ISRRefreshTime(config.getISRRefreshTime())
                .intervalPerRound(config.getIntervalPerRound());
        if (config.isTlsEnabled()) {
            builder.sslContext(config.getSslContext());
        }
        if (config.getClusterInfoResolver() != null) {
            builder.clusterInfoResolver(config.getClusterInfoResolver());
        }
        return builder;
    }

    public void refreshClientMap(RouteResult newRouteInfo) {
        long routeVersion = newRouteInfo.getRouteVersion();
        if (routeVersion <= routeInfo.getRouteVersion()) {
            return;
        }
        executeExclusively(() -> {
            if (routeVersion <= routeInfo.getRouteVersion()) {
                return;
            }
            List<ShardCluster> shardClusters = newRouteInfo.getShardClusters();
            Map<String, StoreClusterClientImpl> newClientMap = Maps.newConcurrentMap();
            shardClusters.forEach(shardCluster -> {
                String address = shardCluster.getCluster();
                StoreClusterClientImpl client = clientMap.get(address);
                if (client == null) {
                    client = new StoreClusterClientImpl(configBuilder
                            .clusterInfo(address)
                            .routeVersion(newRouteInfo.getRouteVersion())
                            .build());
                }
                newClientMap.put(address, client);
            });
            routeInfo = newRouteInfo;
            clientMap = newClientMap;
        });
    }


    /**
     * get real object store cluster client by sharding key
     *
     * @param key sharding key
     * @return object store cluster client
     */
    public StoreClusterClientImpl getShardingClient(String key) {
        return executeShared(() -> {
            List<ShardCluster> shardClusters = routeInfo.getShardClusters();
            long modKey;
            if (key.startsWith(StoreClusterClientImpl.RMSTRX_KEY_PREFIX)) {
                modKey = StoreClusterClientImpl.RMSTRX_SHARD_IDX;
            } else {
                modKey = hash(key) % (1 << routeInfo.getShardFactor());
            }
            for (ShardCluster shardCluster : shardClusters) {
                Set<Long> shardInfo = shardCluster.getShardInfo();
                StoreClusterClientImpl client = clientMap.get(shardCluster.getCluster());
                if (shardInfo != null && client != null && shardInfo.contains(modKey)) {
                    client.updateRouteVersion(routeInfo.getRouteVersion());
                    return client;
                }
            }
            throw new GoblinInternalException("No goblin cluster is available for key " + key);
        });
    }

    /**
     * get redirect object store cluster client by redirect address
     *
     * @param address redirect address
     * @return redirect client
     */
    public StoreClusterClientImpl getRedirectClient(String address) {
        if (address == null || address.isEmpty()) {
            throw new GoblinException("Redirect address is null");
        }
        StoreClusterClientImpl result = null;
        try {
            lock.readLock().lock();
            if (clientMap.get(address) == null) {
                lock.readLock().unlock();
                lock.writeLock().lock();
                try {
                    try {
                        if (clientMap.get(address) == null) {
                            result = new StoreClusterClientImpl(configBuilder.clusterInfo(address).build());
                            clientMap.put(address, result);
                        }
                    } finally {
                        lock.readLock().lock();
                    }
                } finally {
                    lock.writeLock().unlock();
                }
            }
            return result == null ? clientMap.get(address) : result;
        } finally {
            lock.readLock().unlock();
        }

    }

    public boolean isInSameShard(List<String> keys) {
        // TODO: add the logic when @junling has enabled the logic that routes keys with the specific
        //       prefix to the same shard
        if (keys.stream().allMatch(key -> key.startsWith(StoreClusterClientImpl.RMSTRX_KEY_PREFIX))) {
            return true;
        }
        int modSize = 1 << routeInfo.getShardFactor();
        List<Long> modKeys = keys.stream()
                .map(key -> hash(key) % modSize)
                .collect(Collectors.toList());
        return routeInfo.getShardClusters().stream()
                .map(ShardCluster::getShardInfo)
                .anyMatch(set -> set.containsAll(modKeys));
    }

    private long hash(String key) {
        long hash = 5381;
        for (byte c : key.getBytes()) {
            hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
        }
        return hash & 0xFFFFFFFFL;
    }

    public void closeClientMap() {
        executeShared(() -> {
            for (StoreClusterClientImpl client : clientMap.values()) {
                client.close();
            }
            return null;
        });
    }

    private void executeExclusively(Runnable runnable) {
        Lock writeLock = lock.writeLock();
        writeLock.lock();
        try {
            runnable.run();
        } finally {
            writeLock.unlock();
        }
    }

    private <T> T executeShared(Supplier<T> supplier) {
        Lock readLock = lock.readLock();
        readLock.lock();
        try {
            return supplier.get();
        } finally {
            readLock.unlock();
        }
    }
}
