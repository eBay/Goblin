package com.ebay.goblin.client.api.impl;

import com.ebay.goblin.client.api.GoblinClient;
import com.ebay.goblin.client.api.LockService;
import com.ebay.goblin.client.api.ObjectManagerClient;
import com.ebay.goblin.client.api.ObjectStoreClient;
import com.ebay.goblin.client.api.WatchService;
import com.ebay.goblin.client.api.impl.internal.ShardingInfo;
import com.ebay.goblin.client.exceptions.GoblinConditionNotMetException;
import com.ebay.goblin.client.exceptions.GoblinException;
import com.ebay.goblin.client.exceptions.GoblinInternalException;
import com.ebay.goblin.client.exceptions.GoblinInvalidRequestException;
import com.ebay.goblin.client.exceptions.GoblinKeyNotExistException;
import com.ebay.goblin.client.exceptions.GoblinTimeoutException;
import com.ebay.goblin.client.model.CasRequest;
import com.ebay.goblin.client.model.CasResponse;
import com.ebay.goblin.client.model.DeleteRequest;
import com.ebay.goblin.client.model.DeleteResponse;
import com.ebay.goblin.client.model.GenerateRequest;
import com.ebay.goblin.client.model.GenerateResponse;
import com.ebay.goblin.client.model.GetRequest;
import com.ebay.goblin.client.model.GetResponse;
import com.ebay.goblin.client.model.LockRequest;
import com.ebay.goblin.client.model.MiniTransaction;
import com.ebay.goblin.client.model.PutRequest;
import com.ebay.goblin.client.model.PutResponse;
import com.ebay.goblin.client.model.TransRequest;
import com.ebay.goblin.client.model.TransResponse;
import com.ebay.goblin.client.model.WatchAckResponse;
import com.ebay.goblin.client.model.WatchRequest;
import com.ebay.goblin.client.model.common.AbstractEntry;
import com.ebay.goblin.client.model.common.AbstractResult;
import com.ebay.goblin.client.model.common.ExistCondition;
import com.ebay.goblin.client.model.common.KeyType;
import com.ebay.goblin.client.model.common.ReadEntry;
import com.ebay.goblin.client.model.common.WriteEntry;
import com.ebay.goblin.client.utils.ClientVersion;
import com.ebay.goblin.client.utils.LogUtils;
import com.ebay.payments.raft.client.RaftClusterClientConfig;
import com.ebay.payments.raft.client.exceptions.RaftClientKeyNotExistException;
import com.ebay.payments.raft.client.exceptions.RaftClientMigratedRouteException;
import com.ebay.payments.raft.client.exceptions.RaftClientRouteOutOfDateException;
import com.ebay.payments.raft.client.exceptions.RaftClientWrongRouteException;
import com.ebay.payments.raft.client.exceptions.RaftConditionNotMetException;
import com.ebay.payments.raft.client.exceptions.RaftInvalidRequestException;
import com.ebay.payments.raft.client.exceptions.RaftInvokeTimeoutException;
import io.micrometer.core.instrument.Counter;
import io.micrometer.core.instrument.Metrics;
import io.micrometer.core.instrument.Timer;
import java.util.ArrayList;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.TimeUnit;

public class ShardingClientImpl implements GoblinClient {

    private static final String METHOD_TAG_NAME = "method";
    private static final String CLIENT_VERSION_TAG_NAME = "client_version";

    private final WatchService watchService = WatchService.newInstance();
    private final LockService lockService = LockService.newInstance(this);
    private final Map<Integer, MiniTransaction> transactionMap = new ConcurrentHashMap<>();
    private final ObjectManagerClient managerClient;
    private volatile ShardingInfo shardingInfo;

    public ShardingClientImpl(RaftClusterClientConfig config) {
        this.shardingInfo = new ShardingInfo(config);

        this.managerClient = new ManagerClusterClientImpl(config);
        LogUtils.info("init object manager cluster client: {}", managerClient);

        shardingInfo.refreshClientMap(managerClient.route());
        LogUtils.info("init object store cluster clients: {}", shardingInfo);
    }

    @Override
    public PutResponse put(PutRequest request) throws GoblinException {
        return trackMetrics("put", () -> execute(
                request, request.getKey().getKey(),
                (req, client) -> client.put(req)
        ));
    }

    @Override
    public PutResponse put(PutRequest request, ExistCondition existCondition) {
        return trackMetrics("putWithCondition", () -> execute(
                request, request.getKey().getKey(),
                (req, client) -> client.put(req, existCondition)
        ));
    }

    @Override
    public CasResponse cas(CasRequest request) throws GoblinException {
        return trackMetrics("cas", () -> execute(
                request, request.getKey().getKey(),
                (req, client) -> client.cas(req)
        ));
    }

    @Override
    public GetResponse get(GetRequest request) throws GoblinException {
        return trackMetrics(request.isAllowStale() ? "followerGet" : "leaderGet", () -> execute(
                request, request.getKey().getKey(),
                (req, client) -> client.get(req)
        ));
    }

    @Override
    public DeleteResponse delete(DeleteRequest request) throws GoblinException {
        return trackMetrics("delete", () -> execute(
                request, request.getKey().getKey(),
                (req, client) -> client.delete(req)
        ));
    }

    @Override
    public MiniTransaction newTransaction() throws GoblinException {
        return trackMetrics("newTransaction", () -> {
            // TODO: support distributed transaction
            MiniTransaction trans = new MiniTransaction(MiniTransaction.TransType.BATCH);
            transactionMap.put(trans.getTransactionId(), trans);
            return trans;
        });
    }

    @Override
    public TransResponse abortTransaction(MiniTransaction trans) throws GoblinException {
        return trackMetrics("abortTransaction", () -> {
            // TODO: for distributed, we need to cancel the in-progress transaction in server
            transactionMap.remove(trans.getTransactionId());
            return null;
        });
    }

    @Override
    public TransResponse commitTransaction(MiniTransaction trans) throws GoblinException {
        return trackMetrics("commitTransaction", () -> {
            TransRequest request = trans.getRequest();
            if (!transactionMap.containsKey(trans.getTransactionId()) ||
                    request.getAllRequests().isEmpty()) {
                throw new GoblinInvalidRequestException("invalid transaction");
            }
            // TODO: support multiple keys for distributed transaction cross clusters
            // PHASE1: support multiple keys for transactions within one cluster (shard)
            // check if keys are located in one shard
            ArrayList<String> keyList = new ArrayList<String>();
            for (AbstractEntry entry : request.getAllRequests()) {
                if (entry == null) {
                    continue;
                }
                String key;
                if (entry instanceof WriteEntry) {
                    WriteEntry writeEntry = (WriteEntry) entry;
                    key = writeEntry.getKey().getKey();
                } else if (entry instanceof ReadEntry) {
                    ReadEntry readEntry = (ReadEntry) entry;
                    key = readEntry.getKey().getKey();
                } else {
                    throw new GoblinInvalidRequestException("invalid entry");
                }
                keyList.add(key);
            }
            boolean isInOneShard = shardingInfo.isInSameShard(keyList);
            if (!isInOneShard) {
                LogUtils.error("transaction keys are not in one shard");
                throw new GoblinInvalidRequestException("keys cross clusters in one transaction not supported");
            }
            String keyForRouting = keyList.isEmpty() ? "dummyForRouting" : keyList.get(0);
            TransResponse resp = execute(
                    request, keyForRouting,
                    (req, client) -> client.exeTrans(req)
            );
            transactionMap.remove(trans.getTransactionId());
            return resp;
        });
    }

    @Override
    public String watch(WatchRequest request) throws GoblinException {
        return trackMetrics("watch", () -> {
            if (request.getWatchEntries().isEmpty()) {
                LogUtils.error("invalid key number to watch");
                throw new GoblinInvalidRequestException("empty key entries to watch");
            }
            // TODO: support multiple keys to watch cross clusters
            WatchAckResponse response = execute(
                    request, request.getWatchEntries().get(0).getKey().getKey(),
                    (req, client) -> client.asyncWatch(req)
            );
            watchService.submitWatchTask(
                    response.getWatchId(),
                    request.getWatchEntries(),
                    request.getWatchCallback(),
                    response.getWatchStream());
            return response.getWatchId();
        });
    }

    @Override
    public void unWatch(String watchId) throws GoblinException {
        trackMetrics("unWatch", () -> {
            watchService.cancelWatchTask(watchId);
            return null;
        });
    }

    @Override
    public boolean lock(LockRequest request) throws GoblinException {
        return trackMetrics("lock", () -> lockService.lock(request.getKey(), request.getValue(), request.getTtl()));
    }

    @Override
    public boolean unlock(KeyType key) throws GoblinException {
        return trackMetrics("unlock", () -> lockService.unlock(key));
    }

    @Override
    public GenerateResponse generate(GenerateRequest request) {
        return trackMetrics("generate", () -> {
            if (request.getSrcKeys().isEmpty()) {
                throw new GoblinInvalidRequestException("no src keys");
            }
            return execute(
                    request, request.getSrcKeys().get(0).getKey(),
                    (req, client) -> client.generate(req)
            );
        });
    }

    private interface MethodCallable<T> {

        T call() throws GoblinException;
    }

    private <T> T trackMetrics(String methodName, MethodCallable<T> method) throws GoblinException {
        long start = System.nanoTime();
        boolean isSuccess = false;
        Timer timer = Timer.builder("goblin_client_request_latency")
                .description("summary of request latency")
                .tags(METHOD_TAG_NAME, methodName, CLIENT_VERSION_TAG_NAME, ClientVersion.VERSION)
                .publishPercentileHistogram(true)
                .publishPercentiles(0.5, 0.9, 0.95, 0.99, 0.999)
                .register(Metrics.globalRegistry);
        Counter totalCounter = Counter.builder("goblin_client_request_total_counter")
                .description("number of total requests")
                .tags(METHOD_TAG_NAME, methodName, CLIENT_VERSION_TAG_NAME, ClientVersion.VERSION)
                .register(Metrics.globalRegistry);
        totalCounter.increment();
        Counter failureCounter = Counter.builder("goblin_client_request_failure_counter")
                .description("number of failed requests")
                .tags(METHOD_TAG_NAME, methodName, CLIENT_VERSION_TAG_NAME, ClientVersion.VERSION)
                .register(Metrics.globalRegistry);

        T response;
        try {
            response = method.call();
            isSuccess = true;
        } finally {
            if (!isSuccess) {
                long end = System.nanoTime();
                timer.record(end - start, TimeUnit.NANOSECONDS);
                failureCounter.increment();
            }
        }

        long end = System.nanoTime();
        timer.record(end - start, TimeUnit.NANOSECONDS);
        return response;
    }

    private <REQ, REP extends AbstractResult> REP execute(REQ request, String key, ShardingCaller<REQ, REP> caller)
            throws GoblinException {
        StoreClusterClientImpl shardingClient = null;
        while (true) {
            try {
                if (shardingClient == null) {
                    shardingClient = shardingInfo.getShardingClient(key);
                }
                LogUtils.debug("clientRequest: {}, shardingClient: {}", request, shardingClient);
                return caller.call(request, shardingClient);
            } catch (RaftClientRouteOutOfDateException e) {
                shardingInfo.refreshClientMap(managerClient.route());
                shardingClient = shardingInfo.getShardingClient(key);
            } catch (RaftClientMigratedRouteException e) {
                shardingClient = shardingInfo.getRedirectClient(e.getRedirectAddress());
            } catch (RaftClientWrongRouteException e) {
                throw new GoblinInternalException(e.getMessage(), e);
            } catch (RaftConditionNotMetException e) {
                throw new GoblinConditionNotMetException(e.getMessage(), e);
            } catch (RaftInvalidRequestException e) {
                throw new GoblinInvalidRequestException(e.getMessage(), e);
            } catch (RaftClientKeyNotExistException e) {
                throw new GoblinKeyNotExistException(e.getMessage(), e);
            } catch (RaftInvokeTimeoutException e) {
                throw new GoblinTimeoutException(e.getMessage(), e);
            } catch (Exception e) {
                throw new GoblinException(e.getMessage(), e);
            }
        }
    }

    private interface ShardingCaller<REQ, REP extends AbstractResult> {

        REP call(REQ request, ObjectStoreClient client);
    }

    @Override
    public void close() throws Exception {
        managerClient.close();
        shardingInfo.closeClientMap();

    }
}