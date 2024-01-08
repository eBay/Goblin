package com.ebay.payments.raft.client;

import com.ebay.goblin.client.api.impl.internal.PayloadConverters;
import com.ebay.goblin.client.model.MemberOffsetResponse;
import com.ebay.payments.raft.client.exceptions.RaftClientException;
import com.ebay.payments.raft.client.exceptions.RaftRequestConvertionException;
import com.ebay.payments.raft.client.exceptions.RaftResponseConvertionException;
import gringofts.app.protos.AppNetAdminGrpc;
import gringofts.app.protos.Netadmin;
import io.grpc.ManagedChannel;
import io.grpc.netty.shaded.io.grpc.netty.NegotiationType;
import io.grpc.netty.shaded.io.grpc.netty.NettyChannelBuilder;
import lombok.AllArgsConstructor;
import lombok.Getter;

import static goblin.proto.Common.ResponseCode.*;

public class NetAdminStub {
    private final ManagedChannel channel;
    private final PayloadConverters converters = new PayloadConverters();
    private final AppNetAdminGrpc.AppNetAdminBlockingStub blockingStub;
    private final AppNetAdminGrpc.AppNetAdminStub asyncStub;
    protected final String CLASSNAME = this.getClass().getSimpleName();

    public NetAdminStub(RaftClientStubConfig config) {
        try {
            if (config.isTlsEnabled()) {
                channel = NettyChannelBuilder
                        .forAddress(config.getHost(), config.getNetAdminPort())
                        .negotiationType(NegotiationType.TLS)
                        .sslContext(config.getSslContext())
                        .build();
            } else {
                channel = NettyChannelBuilder
                        .forAddress(config.getHost(), config.getNetAdminPort())
                        .usePlaintext()
                        .build();
            }
            asyncStub = AppNetAdminGrpc.newStub(channel);
            blockingStub = AppNetAdminGrpc.newBlockingStub(channel);
        } catch (RaftClientException e) {
            throw e;
        } catch (Exception e) {
            throw new RaftClientException("Failed to shutdown " + CLASSNAME + ": " + this, e);
        }
    }

    public Result<MemberOffsetResponse> getMemberOffset() {
        Netadmin.GetMemberOffsets.Request request = Netadmin.GetMemberOffsets.Request.newBuilder().build();
        try {
            Netadmin.GetMemberOffsets.Response r = blockingStub.getMemberOffsets(request);
            return new Result<>(converters.toMemberOffsetResponse(r), r.getHeader().getCode(), r.getHeader().getMessage(), r.getHeader().getReserved());
        } catch (RaftRequestConvertionException | RaftResponseConvertionException e) {
            return new Result<>(null, BAD_REQUEST.getNumber(), "convert request/response error", null);
        } catch (Throwable e) {
            return new Result<>(null, GENERAL_ERROR.getNumber(), "server error: " + e.getMessage(), null);
        }
    }

    @Getter
    @AllArgsConstructor
    public static class Result<RES> {
        private final RES res;
        private final int code;
        private final String message;
        private final String leaderHint;

        public String leaderHint() {
            if (code == NOT_LEADER_VALUE) {
                return message;
            } else {
                return "";
            }
        }
    }
}
