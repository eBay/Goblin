package com.ebay.payments.raft.client;

import com.ebay.payments.raft.client.exceptions.RaftClientAccessException;
import com.ebay.payments.raft.client.exceptions.RaftClientException;
import com.ebay.payments.raft.client.exceptions.RaftRequestConvertionException;
import com.ebay.payments.raft.client.exceptions.RaftResponseConvertionException;
import com.google.protobuf.GeneratedMessageV3;
import io.grpc.CallOptions;
import io.grpc.Channel;
import io.grpc.ClientCall;
import io.grpc.ClientInterceptor;
import io.grpc.ManagedChannel;
import io.grpc.MethodDescriptor;
import io.grpc.netty.shaded.io.grpc.netty.NegotiationType;
import io.grpc.netty.shaded.io.grpc.netty.NettyChannelBuilder;
import io.grpc.stub.AbstractStub;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;
import lombok.Builder;
import lombok.Data;
import org.apache.commons.lang3.StringUtils;

public abstract class RaftClientStub<A extends AbstractStub, B extends AbstractStub> {

    private static final int MAX_OUTBOUND_MSG_SIZE = 4 * 1024 * 1024; // 4M
    private static final int LARGE_PAYLOAD_SIZE = 100 * 1024; // 100K

    private final ManagedChannel channel;
    private final A asyncStub;
    private final B blockingStub;
    protected final RaftClientStubConfig config;
    private final NetAdminStub netAdminStub;

    protected final String CLASSNAME = this.getClass().getSimpleName();

    public RaftClientStub(RaftClientStubConfig config) {
        try {
            this.config = config;
            if (config.isTlsEnabled()) {
                channel = NettyChannelBuilder
                        .forAddress(config.getHost(), config.getPort())
                        .negotiationType(NegotiationType.TLS)
                        .sslContext(config.getSslContext())
                        .build();
            } else {
                channel = NettyChannelBuilder
                        .forAddress(config.getHost(), config.getPort())
                        .usePlaintext()
                        .build();
            }
            this.asyncStub = createAsyncStub(channel);
            this.blockingStub = createBlockingStub(channel);
            if (config.getNetAdminPort() > 0) {
                netAdminStub = new NetAdminStub(config);
            } else {
                netAdminStub = null;
            }
        } catch (RaftClientException e) {
            throw e;
        } catch (Exception e) {
            throw new RaftClientException("Failed to shutdown " + CLASSNAME + ": " + this, e);
        }
    }

    protected abstract A createAsyncStub(Channel channel);

    protected abstract B createBlockingStub(Channel channel);

    protected abstract GrpcCallResult<Void> connect(ConnectionContext context);

    public void close() throws RaftClientException {
        try {
            channel.shutdown().awaitTermination(config.getTimeoutInMilliSeconds(), TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            throw new RaftClientException("Failed to shutdown " + CLASSNAME + ": " + this, e);
        }
    }

    public NetAdminStub getNetAdmin() {
        return netAdminStub;
    }

    protected <REP> GrpcCallResult<REP> executeAPI(Callable<GrpcCallResult<REP>> grpcCall)
            throws RaftClientException {
        try {
            return grpcCall.call();
        } catch (RaftRequestConvertionException | RaftResponseConvertionException e) {
            throw e;
        } catch (Exception e) {
            throw new RaftClientAccessException(e);
        }
    }

    protected <REP> GrpcCallResult<REP> buildResult(
            REP response, Integer code, String message, Long knownVersion, String leaderHint, String redirectAddress,
            String leaderIp) {
        GrpcCallResult.GrpcCallResultBuilder<REP> builder = GrpcCallResult.builder();
        if (response != null) {
            builder.response(response);
        }
        if (code != null) {
            builder.code(code);
        }
        if (message != null) {
            builder.message(message);
        }
        if (knownVersion != null) {
            builder.knownVersion(knownVersion);
        }
        if (leaderHint != null) {
            builder.leaderHint(leaderHint);
        }
        if (redirectAddress != null) {
            builder.redirectAddress(redirectAddress);
        }
        if (!StringUtils.isBlank(leaderIp)) {
            builder.leaderIp(leaderIp);
        }
        return builder.build();
    }

    protected A getAsyncStub() {
        return this.asyncStub;
    }

    @SuppressWarnings("unchecked")
    protected <REQ extends GeneratedMessageV3> B getBlockingStub(REQ request) {
        return (B) this.blockingStub.withInterceptors(new ClientInterceptor() {
            @Override
            public <ReqT, RespT> ClientCall<ReqT, RespT> interceptCall(
                    MethodDescriptor<ReqT, RespT> method, CallOptions options, Channel next) {
                return next.newCall(method,
                        options.withDeadlineAfter(calculateTimeoutInMs(request), TimeUnit.MILLISECONDS)
                                .withMaxOutboundMessageSize(MAX_OUTBOUND_MSG_SIZE));
            }
        });
    }

    private <REQ extends GeneratedMessageV3> long calculateTimeoutInMs(REQ request) {
        int serializedSize = request.getSerializedSize();
        if (serializedSize < LARGE_PAYLOAD_SIZE) {
            return config.getTimeoutInMilliSeconds();
        } else {
            return config.getLargePayloadTimeoutInMilliSeconds();
        }
    }

    public String getInstanceId() {
        return config.getInstanceId();
    }

    public String getHost() {
        return config.getHost();
    }

    public String getDomainName() {
        return config.getDomainName();
    }

    public int getPort() {
        return config.getPort();
    }

    @Override
    public String toString() {
        return CLASSNAME + "{" +
                "config=" + config +
                '}';
    }

    @Data
    @Builder
    public static class GrpcCallResult<REP> {

        private REP response;
        private Integer code;
        private String message;
        private long knownVersion;
        private String leaderHint;
        private String redirectAddress;
        private String leaderIp;
    }

}
