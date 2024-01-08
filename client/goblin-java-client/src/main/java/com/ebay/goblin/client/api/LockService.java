package com.ebay.goblin.client.api;

import com.ebay.goblin.client.api.impl.ShardingClientImpl;
import com.ebay.goblin.client.api.impl.internal.LockServiceImpl;
import com.ebay.goblin.client.exceptions.GoblinException;
import com.ebay.goblin.client.model.common.KeyType;
import com.ebay.goblin.client.model.common.ValueType;

public interface LockService {

    boolean lock(KeyType key, ValueType value, Integer timeout) throws GoblinException;

    boolean unlock(KeyType key) throws GoblinException;

    static LockService newInstance(ShardingClientImpl client) {
        return new LockServiceImpl(client);
    }

}