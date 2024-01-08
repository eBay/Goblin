package com.ebay.goblin.client.api.impl.internal;

import com.ebay.goblin.client.model.common.AbstractResult;
import com.ebay.goblin.client.model.common.RouteResult;
import com.ebay.payments.raft.client.ConnectionContext;
import com.ebay.payments.raft.client.RaftClientStub;
import com.ebay.payments.raft.client.RaftClientStubConfig;
import goblin.proto.Common;
import goblin.proto.Control;
import goblin.proto.KVManagerGrpc;
import io.grpc.Channel;

public class ObjectManagerStub extends RaftClientStub<KVManagerGrpc.KVManagerStub, KVManagerGrpc.KVManagerBlockingStub> {

    private PayloadConverters converters = new PayloadConverters();

    public ObjectManagerStub(RaftClientStubConfig config) {
        super(config);
    }

    @Override
    public KVManagerGrpc.KVManagerStub createAsyncStub(Channel channel) {
        return KVManagerGrpc.newStub(channel);
    }

    @Override
    public KVManagerGrpc.KVManagerBlockingStub createBlockingStub(Channel channel) {
        return KVManagerGrpc.newBlockingStub(channel);
    }

    @Override
    public GrpcCallResult<Void> connect(ConnectionContext context) {
        //TODO: call real connect API of ObjectManager after it is implemented in server side
        return GrpcCallResult.<Void>builder()
                .code(Common.ResponseCode.OK_VALUE)
                .knownVersion(0L)
                .build();
    }

    public GrpcCallResult<RouteResult> route() {
        return executeAPI(() -> {
            Control.Router.Request request = Control.Router.Request.newBuilder().build();
            Control.Router.Response response = getBlockingStub(request).router(request);
            ResponseWrapper<RouteResult> result = converters.toRouteResponse(response);
            return buildResult(result, response.getHeader());
        });
    }

    protected <REP extends AbstractResult> GrpcCallResult<REP> buildResult(ResponseWrapper<REP> response, Common.ResponseHeader responseHeader) {
        //TODO: put knownVersion in the result after it is implemented in server side
        String leaderHint = responseHeader == null ? null : responseHeader.getLeaderHint();
        String leaderIp = responseHeader == null ? null : responseHeader.getLeaderIp();
        return buildResult(response.getResponse(), response.getCode(), response.getMessage(),
                null, leaderHint, null, leaderIp);
    }

}