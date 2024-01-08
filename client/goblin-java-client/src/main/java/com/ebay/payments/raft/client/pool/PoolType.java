package com.ebay.payments.raft.client.pool;

import lombok.AllArgsConstructor;
import lombok.Getter;

@AllArgsConstructor
public enum PoolType {
    LeaderOnly(false),
    InSyncReplica(true),
    RoundRobin(false);

    @Getter
    private boolean useNetAdmin;
}
