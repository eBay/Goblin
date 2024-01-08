package com.ebay.payments.raft.client;

import com.ebay.goblin.client.model.DeleteRequest;
import com.ebay.goblin.client.model.common.AbstractResult;
import com.ebay.goblin.client.utils.LogUtils;
import com.ebay.payments.raft.client.exceptions.RaftClientAccessException;
import com.ebay.payments.raft.client.exceptions.RaftClientException;
import com.ebay.payments.raft.client.exceptions.RaftClientKeyNotExistException;
import com.ebay.payments.raft.client.exceptions.RaftClientMigratedRouteException;
import com.ebay.payments.raft.client.exceptions.RaftClientResourceExhaustedException;
import com.ebay.payments.raft.client.exceptions.RaftClientRouteOutOfDateException;
import com.ebay.payments.raft.client.exceptions.RaftClientWrongRouteException;
import com.ebay.payments.raft.client.exceptions.RaftConditionNotMetException;
import com.ebay.payments.raft.client.exceptions.RaftInvalidRequestException;
import com.ebay.payments.raft.client.exceptions.RaftInvokeTimeoutException;
import com.ebay.payments.raft.client.pool.RaftClientPool;
import com.ebay.payments.raft.client.pool.impl.ISRClientPool;
import com.ebay.payments.raft.client.pool.impl.LeaderOnlyClientPool;
import com.ebay.payments.raft.client.pool.impl.RoundRobinRaftClientPool;
import goblin.proto.Common;
import io.grpc.Status;
import io.grpc.StatusRuntimeException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import lombok.Getter;
import lombok.Setter;
import org.apache.commons.lang3.StringUtils;

/**
 * This is the client for a Raft cluster.
 * It creates RaftClientStub instances for each Raft server individually.
 */
public abstract class RaftClusterClient<S extends RaftClientStub> {

    private static final ThreadLocal<Integer> retryTimes = ThreadLocal.withInitial(() -> 0);
    private static final int MAX_RETRY_CYCLE = 3;
    private static final int INCREASE_FACTOR = 2;
    private static final int NET_ADMIN_PORT = 50065;

    protected ConnectionContext context;
    private RaftClientPool<S> grpcClientPool;

    private final Map<Integer, ResponseAction> actionTable = new HashMap<>();
    private final boolean tlsEnabled;
    private final RaftClusterClientConfig config;
    private final long intervalPerRound;
    private final long timeoutInMilliSeconds;
    protected final ResponseActions RESPONSE_ACTIONS = new ResponseActions();

    public RaftClusterClient(RaftClusterClientConfig config) {
        this.config = config;
        this.tlsEnabled = config.isTlsEnabled();
        this.intervalPerRound = config.getIntervalPerRound();
        this.context = ConnectionContext.builder()
                .contextId(generateNewContextId(config))
                .routeVersion(config.getRouteVersion())
                .build();
        initActionTable();
        this.timeoutInMilliSeconds = config.getTimeoutInMilliSeconds();
        grpcClientPool = initClientPool(config);
        initializeClients(config);
        connectCluster();
    }

    protected RaftClientPool<S> initClientPool(RaftClusterClientConfig config) {
        switch (config.getPoolType()) {
            case LeaderOnly:
                return new LeaderOnlyClientPool<>();
            case RoundRobin:
                LogUtils.info("init Round Robin Client pool");
                return new RoundRobinRaftClientPool<>();
            case InSyncReplica:
                LogUtils.info("init ISR Client pool");
                return new ISRClientPool<>(new RoundRobinRaftClientPool<>(), config.getISRValidMinGap(), config.getISRLeaseTime(), config.getISRRefreshTime());
            default:
                LogUtils.error("unknown client pool");
                throw new RuntimeException("no such client pool" + config.getPoolType());
        }
    }

    private void initializeClients(RaftClusterClientConfig config) {
        String resolvedClusterInfo = config.getResolvedClusterInfo();
        if (resolvedClusterInfo == null || resolvedClusterInfo.isEmpty()) {
            throw new RaftClientException("Initialization failed, raft cluster not specified.");
        }
        LogUtils.info("raft cluster info are {}", resolvedClusterInfo);
        String[] ipAddresses = resolvedClusterInfo.split(",");
        String clusterInfo = config.getClusterInfo();
        String[] addresses = clusterInfo.split(",");
        if (ipAddresses.length != addresses.length) {
            throw new RaftClientException("Initialization failed, incompatible resolved cluster info.");
        }
        List<S> list = new ArrayList<>();
        for (int i = 0; i < ipAddresses.length; i++) {
            S grpcClient = buildClient(ipAddresses[i], addresses[i], config);
            list.add(grpcClient);
        }
        grpcClientPool.init(list);
        LogUtils.info("raft cluster size is {}", grpcClientPool.size());
    }

    private String getResolvedClusterInfo(int instanceIndex) {
        String clusterInfo = config.getResolvedClusterInfo();
        if (clusterInfo == null || clusterInfo.isEmpty()) {
            throw new RaftClientException("Initialization failed, raft cluster not specified.");
        }
        String[] addresses = clusterInfo.split(",");
        if (instanceIndex < 0) {
            throw new RaftClientException(String.format("Invalid instance index %d for cluster %s", instanceIndex, clusterInfo));
        }
        return addresses[instanceIndex];
    }

    private String getExistingClusterInfo(int instanceIndex) {
        S existingClient = grpcClientPool.get(instanceIndex);
        return String.format("%s@%s:%d",
                existingClient.getInstanceId(), existingClient.getHost(), existingClient.getPort());
    }

    private void refreshIP(int index, String ip) {
        LogUtils.info("Reconnect to leader leaderIndex {} leaderip {}", index, ip);
        if (StringUtils.isBlank(ip)) {
            return;
        }
        S oldClient = grpcClientPool.get(index);
        if (oldClient.getHost().equals(ip)) {
            LogUtils.info("ip not change for {}, index is  {}", index + 1, ip);
            return;
        }
        String address = String.format("%d@%s:%d", index + 1, ip, oldClient.getPort());
        LogUtils.info("update to address {} index {}", address, index);
        S grpcClient = buildClient(address, oldClient.getDomainName(), config);
        if (grpcClient.connect(context).getKnownVersion() > 0) {
            S existingClient = grpcClientPool.refresh(index, grpcClient);
            existingClient.close();
        }
    }

    private void tryRefreshClient(int instanceIndex) {
        String address = null;
        String existingAddress = null;
        try {
            boolean changed = false;
            address = getResolvedClusterInfo(instanceIndex);
            existingAddress = getExistingClusterInfo(instanceIndex);
            if (!existingAddress.equalsIgnoreCase(address)) {
                synchronized (this) {
                    existingAddress = getExistingClusterInfo(instanceIndex);
                    if (!existingAddress.equalsIgnoreCase(address)) {
                        S grpcClient = buildClient(address, grpcClientPool.get(instanceIndex).getDomainName(), config);
                        if (grpcClient.connect(context).getKnownVersion() >= 0) {
                            S existingClient = grpcClientPool.refresh(instanceIndex, grpcClient);
                            existingClient.close();
                        }
                        changed = true;
                        LogUtils.warn(String.format("successfully refresh client %d from %s to %s", instanceIndex, existingAddress, address));
                    }
                }
            }
            if (!changed) {
                LogUtils.info(String.format("client %d %s was not changed, do not refresh.", instanceIndex, address));
            }
        } catch (Exception e) {
            LogUtils.error(String.format("failed to refresh client %d from %s to %s", instanceIndex, existingAddress, address), e);
        }
    }

    protected S buildClient(String address, String domainName, RaftClusterClientConfig config) throws RaftClientException {
        int atIndex = address.indexOf('@');
        int colonIndex = address.indexOf(':');
        if (atIndex == -1 || colonIndex == -1 || atIndex > colonIndex) {
            throw new RaftClientException(String.format("Server endpoint(%s) not formatted as 'INSTANCEID@HOST:PORT'", address));
        }
        String instanceId = address.substring(0, atIndex);
        String[] hostAndPort = address.substring(atIndex + 1).split(":");
        String host = hostAndPort[0];
        int portInt = Integer.parseInt(hostAndPort[1]);
        int netAdminPort = 0;
        if (config.getPoolType().isUseNetAdmin()) {
            netAdminPort = NET_ADMIN_PORT;
        }

        RaftClientStubConfig clientConfig = RaftClientStubConfig.builder()
                .instanceId(instanceId).host(host).port(portInt)
                .domainName(domainName)
                .tlsEnabled(config.isTlsEnabled())
                .sslContext(config.getSslContext())
                .timeoutInMilliSeconds(config.getStubTimeoutInMilliSeconds())
                .largePayloadTimeoutInMilliSeconds(config.getLargePayloadStubTimeoutInMilliSeconds())
                .netAdminPort(netAdminPort)
                .build();
        return createStub(clientConfig);
    }

    @SuppressWarnings("unchecked")
    protected void connectCluster() {
        execute(
                (req, client) -> client.connect(context)
        );
    }

    protected abstract S createStub(RaftClientStubConfig clientConfig);

    public void close() {
        grpcClientPool.shutdown();
        LogUtils.info("GrpcClients have been closed successfully!");
    }

    private String generateNewContextId(RaftClusterClientConfig config) {
        return String.format("%s-%d", config.getClusterInfo(), System.currentTimeMillis());
    }

    private void initActionTable() {
        initStandardActionTable();
        actionTable.putAll(initSpecificActionTable());
    }

    /**
     * setup the domain specific action tables.
     * this method could be overwritten by sub classes to define the actions of
     *
     * @return
     */
    protected Map<Integer, ResponseAction> initSpecificActionTable() {
        return new HashMap<>();
    }

    private void initStandardActionTable() {
        actionTable.put(Common.ResponseCode.PROCESSING_VALUE, RESPONSE_ACTIONS.INPROGRESS);

        actionTable.put(Common.ResponseCode.OK_VALUE, RESPONSE_ACTIONS.SUCCESS);
        actionTable.put(Common.ResponseCode.CREATED_VALUE, RESPONSE_ACTIONS.SUCCESS);
        actionTable.put(Common.ResponseCode.KEY_NOT_EXIST_VALUE, RESPONSE_ACTIONS.KEY_NOT_EXIST);

        actionTable.put(Common.ResponseCode.NOT_LEADER_VALUE, RESPONSE_ACTIONS.REDIRECT);

        actionTable.put(Common.ResponseCode.BAD_REQUEST_VALUE, RESPONSE_ACTIONS.CLIENT_ERROR);
        actionTable.put(Common.ResponseCode.WRONG_ROUTE_VALUE, RESPONSE_ACTIONS.WRONG_ROUTE);
        actionTable.put(Common.ResponseCode.MIGRATED_ROUTE_VALUE, RESPONSE_ACTIONS.MIGRATED_ROUTE);
        actionTable.put(Common.ResponseCode.CLIENT_ROUTE_OUT_OF_DATE_VALUE, RESPONSE_ACTIONS.CLIENT_ROUTE_OUT_OF_DATE);
        actionTable.put(Common.ResponseCode.SERVER_ROUTE_OUT_OF_DATE_VALUE, RESPONSE_ACTIONS.SERVER_ROUTE_OUT_OF_DATE);

        actionTable.put(Common.ResponseCode.GENERAL_ERROR_VALUE, RESPONSE_ACTIONS.SERVICE_UNAVAILABLE);
    }

    public <REQ, REP extends AbstractResult> REP execute(GrpcCaller<REQ, REP, S> executor) throws RaftClientException {
        return execute(new RequestWrapper<>(null, null), executor);
    }

    @SuppressWarnings("unchecked")
    public <REQ, REP extends AbstractResult> REP execute(RequestWrapper<REQ> requestWrapper, GrpcCaller<REQ, REP, S> executor) throws RaftClientException {
        /**
         * if is not stale allowed request, pick(true) = pickLeader
         * otherwise pick(false), allow request from followers
         */
        int instanceIdx = grpcClientPool.pick(!requestWrapper.isStale());
        S client = grpcClientPool.get(instanceIdx);
        REQ request = requestWrapper.getRequest();
        String requestName = request != null ? request.getClass().getSimpleName() : "unknown";
        try {
            LogUtils.info("try {}, call raft instance {} to execute kv request", retryTimes.get(), client);
            checkTimeout(requestWrapper);
            RaftClientStub.GrpcCallResult<REP> result = executor.call(request, client);
            REP response = result.getResponse();
            int responseCode = result.getCode();
            LogUtils.info("try {}, receive raft instance {} execution response: {}", retryTimes.get(),
                    client, responseCode);
            LogUtils.debug("responseContent: {}", response);
            int code = actionTable.containsKey(responseCode) ? responseCode : Common.ResponseCode.GENERAL_ERROR_VALUE;
            if (needRetry(code)) {
                increaseRetryTimeAndCheck();
            }
            return (REP) actionTable.get(code).doAction(executor, requestWrapper, result, instanceIdx);
        } catch (RaftClientAccessException e) {
            checkResourceExhaustedException(e);
            increaseRetryTimeAndCheck();
            return (REP) actionTable.get(Common.ResponseCode.GENERAL_ERROR_VALUE)
                    .doAction(executor, requestWrapper, null, instanceIdx);
        }
    }

    private static boolean checkResourceExhaustedException(Throwable throwable) {
        if (throwable == null) {
            return false;
        }
        if (throwable instanceof StatusRuntimeException) {
            StatusRuntimeException statusRuntimeException = (StatusRuntimeException) throwable;
            if (statusRuntimeException.getStatus().getCode() == Status.Code.RESOURCE_EXHAUSTED) {
                throw new RaftClientResourceExhaustedException(statusRuntimeException.getMessage());
            }
        }
        return checkResourceExhaustedException(throwable.getCause());
    }

    private <REQ> void checkTimeout(RequestWrapper<REQ> requestWrapper) {
        long currentTimestamp = System.currentTimeMillis();
        if (requestWrapper.getFirstCallTimestamp() == null) {
            requestWrapper.setFirstCallTimestamp(currentTimestamp);
        } else {
            long elapsedMilliSeconds = currentTimestamp - requestWrapper.getFirstCallTimestamp();
            if (elapsedMilliSeconds > timeoutInMilliSeconds) {
                String message = String.format("Timeout after %d ms to cluster %s [tls: %s]", elapsedMilliSeconds,
                        this.config.getClusterInfo(), String.valueOf(this.tlsEnabled));
                LogUtils.info(message);
                throw new RaftInvokeTimeoutException(message);
            }
        }
    }

    private boolean needRetry(int code) {
        return code == Common.ResponseCode.PROCESSING_VALUE
                || code == Common.ResponseCode.NOT_LEADER_VALUE
                || code == Common.ResponseCode.GENERAL_ERROR_VALUE;
    }

    private void increaseRetryTimeAndCheck() throws RaftClientException {
        int curValue = retryTimes.get();
        int clusterSize = grpcClientPool.size();
        if (curValue >= clusterSize * MAX_RETRY_CYCLE) {
            retryTimes.remove();
            String message = String.format("No raft instances available to cluster %s [tls: %s]",
                    this.config.getClusterInfo(), String.valueOf(this.tlsEnabled));
            LogUtils.error(message);
            throw new RaftClientException(message);
        }

        sleep(curValue / clusterSize);
        retryTimes.set(curValue + 1);
    }

    private void sleep(int round) {
        try {
            int times = round;
            long timeInMilliSeconds = intervalPerRound;
            while (times > 0) {
                timeInMilliSeconds *= INCREASE_FACTOR;
                times -= 1;
            }
            LogUtils.info("Sleep {} milliseconds for round {}", timeInMilliSeconds, round);
            Thread.sleep(timeInMilliSeconds);
        } catch (InterruptedException e) {
            LogUtils.error("Sleep was interrupted due to {}", e.getMessage());
        }
    }

    protected interface ResponseAction<REQ, REP extends AbstractResult, S extends RaftClientStub> {

        REP doAction(GrpcCaller<REQ, REP, S> executor,
                     RequestWrapper<REQ> requestWrapper,
                     RaftClientStub.GrpcCallResult<REP> result,
                     int index) throws RaftClientException;
    }

    @SuppressWarnings("unchecked")
    protected class ResponseActions {

        public final ResponseAction SUCCESS = (executor, requestWrapper, result, instanceIdx) -> {
            //report execution successfully
            if (!requestWrapper.isStale()) {
                grpcClientPool.reportLeader(instanceIdx);
            }
            retryTimes.remove();
            context.updateKnownVersion(result.getKnownVersion());
            return (AbstractResult) result.getResponse();
        };

        public final ResponseAction INPROGRESS = (executor, requestWrapper, result, instanceIdx) -> {
            return execute(requestWrapper, executor);
        };

        public final ResponseAction KEY_NOT_EXIST = (executor, requestWrapper, result, instanceIdx) -> {
            if (requestWrapper.getRequest() instanceof DeleteRequest) {
                throw new RaftClientKeyNotExistException(result.getMessage());
            }
            // TODO: handle cas key_not_exist condition
            if (!requestWrapper.isStale()) {
                grpcClientPool.reportLeader(instanceIdx);
            }
            retryTimes.remove();
            context.updateKnownVersion(result.getKnownVersion());
            return (AbstractResult) result.getResponse();
        };

        public final ResponseAction REDIRECT = (executor, requestWrapper, result, instanceIdx) -> {
            if (result.getLeaderHint() == null) {
                throw new RaftClientException("No leaderHint found in the response.");
            }
            grpcClientPool.leaderHint(result.getLeaderHint());
            refreshIP(grpcClientPool.findIdxFromInstanceId(result.getLeaderHint()), result.getLeaderIp());
            return execute(requestWrapper, executor);
        };

        public final ResponseAction WRONG_ROUTE = (executor, requestWrapper, result, instanceIdx) -> {
            retryTimes.remove();
            throw new RaftClientWrongRouteException(result.getMessage());
        };

        public final ResponseAction MIGRATED_ROUTE = (executor, requestWrapper, result, instanceIdx) -> {
            retryTimes.remove();
            throw new RaftClientMigratedRouteException(result.getRedirectAddress());
        };

        public final ResponseAction CLIENT_ROUTE_OUT_OF_DATE = (executor, requestWrapper, result, instanceIdx) -> {
            retryTimes.remove();
            throw new RaftClientRouteOutOfDateException(result.getMessage());
        };

        public final ResponseAction SERVER_ROUTE_OUT_OF_DATE = (executor, requestWrapper, result, instanceIdx) -> {
            // retry with the same address
            return execute(requestWrapper, executor);
        };

        public final ResponseAction CLIENT_ERROR = (executor, requestWrapper, result, instanceIdx) -> {
            retryTimes.remove();
            throw new RaftInvalidRequestException(result.getMessage());
        };

        public final ResponseAction CONDITION_NOT_MET = (executor, requestWrapper, result, instanceIdx) -> {
            retryTimes.remove();
            throw new RaftConditionNotMetException(result.getMessage());
        };

        public final ResponseAction SERVICE_UNAVAILABLE = (executor, requestWrapper, result, instanceIdx) -> {
            S client = grpcClientPool.get(instanceIdx);
            LogUtils.info("retried {}, calling raft instance failed: 503, instance: {}", retryTimes.get(), client.toString());
            tryRefreshClient(instanceIdx);
            // mark down for 1s
            grpcClientPool.markDown(instanceIdx, 1000);
            return execute(requestWrapper, executor);
        };
    }

    protected interface GrpcCaller<REQ, REP extends AbstractResult, S extends RaftClientStub> {

        RaftClientStub.GrpcCallResult<REP> call(
                REQ request, S client) throws RaftClientAccessException;
    }

    @Getter
    @Setter
    protected static class RequestWrapper<REQ> {

        private REQ request;
        private ConnectionContext context;
        private boolean stale;
        private Long firstCallTimestamp;

        public RequestWrapper(REQ request, ConnectionContext context) {
            this(request, context, false);
        }

        public RequestWrapper(REQ request, ConnectionContext context, boolean stale) {
            this.request = request;
            this.context = context == null ? null : context.clone();
            this.stale = stale;
            this.firstCallTimestamp = null;
        }
    }
}
