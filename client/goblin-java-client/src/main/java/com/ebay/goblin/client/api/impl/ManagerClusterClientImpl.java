package com.ebay.goblin.client.api.impl;

import com.ebay.goblin.client.api.ObjectManagerClient;
import com.ebay.goblin.client.api.impl.internal.ObjectManagerStub;
import com.ebay.goblin.client.model.common.RouteResult;
import com.ebay.goblin.client.utils.LogUtils;
import com.ebay.payments.raft.client.*;
import com.ebay.payments.raft.client.pool.RaftClientPool;
import com.ebay.payments.raft.client.pool.impl.LeaderOnlyClientPool;
import com.ebay.payments.raft.client.pool.impl.RoundRobinRaftClientPool;

import java.util.Map;

public class ManagerClusterClientImpl
        extends RaftClusterClient<ObjectManagerStub> implements ObjectManagerClient {

    public ManagerClusterClientImpl(RaftClusterClientConfig config) {
        super(config);
    }

    @Override
    protected ObjectManagerStub createStub(RaftClientStubConfig clientConfig) {
        return new ObjectManagerStub(clientConfig);
    }

    @Override
    public RouteResult route() {
        return execute(
                new RequestWrapper<>(null, context),
                (req, client) -> client.route()
        );
    }

    /**
     * OM only allow to use leader only pool
     * @param config
     * @return
     */
    @Override
    protected RaftClientPool<ObjectManagerStub> initClientPool(RaftClusterClientConfig config) {
        LogUtils.info("init Leader only Client pool");
        return new LeaderOnlyClientPool<>();
    }

    @Override
    protected Map<Integer, ResponseAction> initSpecificActionTable() {
        return super.initSpecificActionTable();
    }
}
