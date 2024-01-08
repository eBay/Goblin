package com.ebay.goblin.client.api.impl.internal;

import com.ebay.goblin.client.api.WatchStream;
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
import com.ebay.payments.raft.client.ConnectionContext;
import com.ebay.payments.raft.client.RaftClientStub;
import com.ebay.payments.raft.client.RaftClientStubConfig;
import com.ebay.payments.raft.client.exceptions.RaftClientException;
import goblin.proto.Common;
import goblin.proto.KVStoreGrpc;
import goblin.proto.Service;
import io.grpc.Channel;
import io.grpc.stub.StreamObserver;

public class ObjectStoreClientStub extends RaftClientStub<KVStoreGrpc.KVStoreStub, KVStoreGrpc.KVStoreBlockingStub> {

    private PayloadConverters converters = new PayloadConverters();

    public ObjectStoreClientStub(RaftClientStubConfig config) {
        super(config);
    }

    @Override
    protected KVStoreGrpc.KVStoreBlockingStub createBlockingStub(Channel channel) {
        return KVStoreGrpc.newBlockingStub(channel);
    }

    @Override
    protected KVStoreGrpc.KVStoreStub createAsyncStub(Channel channel) {
        return KVStoreGrpc.newStub(channel);
    }

    @Override
    public GrpcCallResult<Void> connect(ConnectionContext context) {
        return executeAPI(() -> {
            Service.Connect.Request kvRequest = Service.Connect.Request.newBuilder()
                    .setHeader(Common.RequestHeader.newBuilder()
                            .setRouteVersion(context.getRouteVersion())
                            .build())
                    .build();
            Service.Connect.Response kvResponse = getBlockingStub(kvRequest).connect(kvRequest);
            return buildResult(kvResponse.getHeader());
        });
    }

    public GrpcCallResult<PutResponse> put(PutRequest request, ConnectionContext context) {
        return executeAPI(() -> {
            Service.Put.Request kvRequest = converters.toKvPutRequest(context, request);
            Service.Put.Response kvResponse = getBlockingStub(kvRequest).put(kvRequest);
            ResponseWrapper<PutResponse> response = converters.toPutResponse(kvResponse);
            return buildResult(response, kvResponse.getHeader());
        });
    }

    public GrpcCallResult<PutResponse> put(PutRequest request, ExistCondition condition, ConnectionContext context) {
        return executeAPI(() -> {
            Service.Transaction.Request kvRequest = converters.toKvPutRequest(context, request, condition);
            Service.Transaction.Response kvResponse = getBlockingStub(kvRequest).transaction(kvRequest);
            ResponseWrapper<PutResponse> response = converters.toPutResponse(kvResponse);
            return buildResult(response, kvResponse.getHeader());
        });
    }

    public GrpcCallResult<CasResponse> cas(CasRequest request, ConnectionContext context) {
        return executeAPI(() -> {
            Service.Transaction.Request kvRequest = converters.toKvCasRequest(context, request);
            Service.Transaction.Response kvResponse = getBlockingStub(kvRequest).transaction(kvRequest);
            ResponseWrapper<CasResponse> response = converters.toCasResponse(kvResponse);
            return buildResult(response, kvResponse.getHeader());
        });
    }

    public GrpcCallResult<GetResponse> get(GetRequest request, ConnectionContext context) {
        return executeAPI(() -> {
            Service.Get.Request kvRequest = converters.toKvGetRequest(context, request);
            Service.Get.Response kvResponse = getBlockingStub(kvRequest).get(kvRequest);
            ResponseWrapper<GetResponse> response = converters.toGetResponse(kvResponse);
            return buildResult(response, kvResponse.getHeader());
        });
    }

    public GrpcCallResult<DeleteResponse> delete(DeleteRequest request, ConnectionContext context) {
        return executeAPI(() -> {
            Service.Delete.Request kvRequest = converters.toKvDeleteRequest(context, request);
            Service.Delete.Response kvResponse = getBlockingStub(kvRequest).delete(kvRequest);
            ResponseWrapper<DeleteResponse> response = converters.toDeleteResponse(kvResponse);
            return buildResult(response, kvResponse.getHeader());
        });
    }

    public GrpcCallResult<TransResponse> exeTrans(TransRequest request, ConnectionContext context) {
        return executeAPI(() -> {
            Service.Transaction.Request kvRequest = converters.toKvTransRequest(context, request);
            Service.Transaction.Response kvResponse = getBlockingStub(kvRequest).transaction(kvRequest);
            ResponseWrapper<TransResponse> response = converters.toTransResponse(kvResponse);
            return buildResult(response, kvResponse.getHeader());
        });
    }

    public GrpcCallResult<WatchAckResponse> asyncWatch(WatchRequest request, ConnectionContext context) {
        return executeAPI(() -> {
            Service.Watch.Request watchRequest = converters.toKvWatchRequest(context, request);
            WatchStreamImpl.BlockingStream responseStream = new WatchStreamImpl.BlockingStream();
            StreamObserver<Service.Watch.Request> requestStream = getAsyncStub().watch(responseStream);
            WatchStream watchStream = new WatchStreamImpl(requestStream, responseStream);
            watchStream.pushNewRequest(watchRequest);
            Service.Watch.Response kvResponse = null;
            // get the first response here to get header
            if (request.isRetrieveOnConnected()) {
                // get first response but also leave it in the message callback queue
                kvResponse = watchStream.peekNextResponse(config.getTimeoutInMilliSeconds());
            } else {
                // get first response but also remove it from the message callback queue
                kvResponse = watchStream.pullNextResponse(config.getTimeoutInMilliSeconds());
            }

            if (kvResponse == null) {
                throw new RaftClientException("failed to pull ack response for watch");
            }
            ResponseWrapper<WatchAckResponse> response = converters.toWatchAckResponse(kvResponse, watchStream);
            return buildResult(response, kvResponse.getHeader());
        });
    }

    public GrpcCallResult<GenerateResponse> generate(GenerateRequest request, ConnectionContext context) {
        return executeAPI(() -> {
            Service.GenerateKV.Request kvRequest = converters.toKVGenerateRequest(context, request);
            Service.GenerateKV.Response kvResponse = getBlockingStub(kvRequest).generateKV(kvRequest);
            ResponseWrapper<GenerateResponse> response = converters.toKVGenerateResponse(kvResponse);
            return buildResult(response, kvResponse.getHeader());
        });
    }

    protected <REP> GrpcCallResult<REP> buildResult(Common.ResponseHeader responseHeader) {
        Long knownVersion = responseHeader == null ? null : responseHeader.getLatestVersion();
        String leaderHint = responseHeader == null ? null : responseHeader.getLeaderHint();
        String redirectAddress = responseHeader == null ? null : converters.toClusterAddress(responseHeader.getRouteHint());
        Integer code = responseHeader == null ? null : responseHeader.getCodeValue();
        String msg = responseHeader == null ? null : responseHeader.getMessage();
        String leaderIp = responseHeader == null ? null : responseHeader.getLeaderIp();
        return buildResult(null, code, msg, knownVersion, leaderHint, redirectAddress, leaderIp);
    }

    protected <REP extends AbstractResult> GrpcCallResult<REP> buildResult(
            ResponseWrapper<REP> response, Common.ResponseHeader responseHeader) {
        Long knownVersion = responseHeader == null ? null : responseHeader.getLatestVersion();
        String leaderHint = responseHeader == null ? null : responseHeader.getLeaderHint();
        String redirectAddress = responseHeader == null ? null : converters.toClusterAddress(responseHeader.getRouteHint());
        String leaderIp = responseHeader == null ? null : responseHeader.getLeaderIp();

        return buildResult(response.getResponse(), response.getCode(), response.getMessage(), knownVersion, leaderHint, redirectAddress, leaderIp);
    }
}
