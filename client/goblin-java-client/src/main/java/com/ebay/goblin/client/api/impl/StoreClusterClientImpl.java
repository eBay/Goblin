package com.ebay.goblin.client.api.impl;

import com.ebay.goblin.client.api.ObjectStoreClient;
import com.ebay.goblin.client.api.impl.internal.ObjectStoreClientStub;
import com.ebay.goblin.client.exceptions.GoblinException;
import com.ebay.goblin.client.model.CasRequest;
import com.ebay.goblin.client.model.CasResponse;
import com.ebay.goblin.client.model.DeleteRequest;
import com.ebay.goblin.client.model.DeleteResponse;
import com.ebay.goblin.client.model.GenerateRequest;
import com.ebay.goblin.client.model.GenerateResponse;
import com.ebay.goblin.client.model.GetRequest;
import com.ebay.goblin.client.model.GetResponse;
import com.ebay.goblin.client.model.PutRequest;
import com.ebay.goblin.client.model.PutResponse;
import com.ebay.goblin.client.model.TransRequest;
import com.ebay.goblin.client.model.TransResponse;
import com.ebay.goblin.client.model.WatchAckResponse;
import com.ebay.goblin.client.model.WatchRequest;
import com.ebay.goblin.client.model.common.AbstractResult;
import com.ebay.goblin.client.model.common.ExistCondition;
import com.ebay.payments.raft.client.RaftClientStubConfig;
import com.ebay.payments.raft.client.RaftClusterClient;
import com.ebay.payments.raft.client.RaftClusterClientConfig;
import com.ebay.payments.raft.client.exceptions.RaftClientException;
import goblin.proto.Common;
import java.util.Map;

public class StoreClusterClientImpl
        extends RaftClusterClient<ObjectStoreClientStub> implements ObjectStoreClient {
    public static String RMSTRX_KEY_PREFIX = "/RMSTRX";
    public static long RMSTRX_SHARD_IDX = 1;

    public void updateRouteVersion(long routeVersion) {
        if (context != null) {
            this.context.updateRouteVersion(routeVersion);
        }
    }

    public StoreClusterClientImpl(RaftClusterClientConfig config) {
        super(config);
    }

    @Override
    protected ObjectStoreClientStub createStub(RaftClientStubConfig clientConfig) {
        return new ObjectStoreClientStub(clientConfig);
    }

    @Override
    public PutResponse put(PutRequest request) {
        return execute(
                new RequestWrapper<>(request, context),
                (req, client) -> client.put(request, context)
        );
    }

    @Override
    public PutResponse put(PutRequest request, ExistCondition condition) {
        return execute(
                new RequestWrapper<>(request, context),
                (req, client) -> client.put(request, condition, context)
        );
    }

    @Override
    public CasResponse cas(CasRequest request) {
        return execute(
                new RequestWrapper<>(request, context),
                (req, client) -> client.cas(request, context)
        );
    }

    @Override
    public GetResponse get(GetRequest request) {
        return execute(
                new RequestWrapper<>(request, context, request.isAllowStale()),
                (req, client) -> client.get(request, context)
        );
    }

    @Override
    public DeleteResponse delete(DeleteRequest request) {
        return execute(
                new RequestWrapper<>(request, context),
                (req, client) -> client.delete(request, context)
        );
    }

    @Override
    public TransResponse exeTrans(TransRequest request) {
        return execute(
                new RequestWrapper<>(request, context, request.isAllowStale()),
                (req, client) -> client.exeTrans(request, context)
        );
    }

    @Override
    public WatchAckResponse asyncWatch(WatchRequest createWatchRequest) throws GoblinException {
        return execute(
                new RequestWrapper<>(createWatchRequest, context),
                (req, client) -> client.asyncWatch(createWatchRequest, context)
        );
    }

    @Override
    public void asyncUnWatch(String watchId) throws GoblinException {
        throw new GoblinException("not supported");
    }

    @Override
    public GenerateResponse generate(GenerateRequest request) {
        return execute(
                new RequestWrapper<>(request, context),
                (req, client) -> client.generate(request, context)
        );
    }

    public <REQ, REP extends AbstractResult> REP execute(
            RequestWrapper<REQ> request,
            GrpcCaller<REQ, REP, ObjectStoreClientStub> executor) throws RaftClientException {
        return super.execute(request, executor);
    }

    @Override
    protected Map<Integer, ResponseAction> initSpecificActionTable() {
        Map<Integer, ResponseAction> actionTable = super.initSpecificActionTable();
        actionTable.put(Common.ResponseCode.KEY_NOT_EXIST_VALUE, RESPONSE_ACTIONS.KEY_NOT_EXIST);
        actionTable.put(Common.ResponseCode.PRECOND_NOT_MATCHED_VALUE, RESPONSE_ACTIONS.CONDITION_NOT_MET);
        return actionTable;
    }

}
