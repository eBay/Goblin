package com.ebay.goblin.client.api;

import com.ebay.goblin.client.exceptions.GoblinException;
import com.ebay.goblin.client.model.*;
import com.ebay.goblin.client.model.common.*;
import com.ebay.goblin.client.api.impl.ShardingClientImpl;
import com.ebay.goblin.client.utils.LogUtils;
import com.ebay.payments.raft.client.RaftClusterClientConfig;
import com.google.protobuf.ByteString;
import goblin.proto.Userdefine;

import java.util.ArrayList;
import java.util.List;

/**
 * Please check com.ebay.goblin.sample.SampleGoblinClient for usage.
 * The client is not supposed to be re-connected after it's closed.
 * To re-connect to a cluster, please create a new GoblinClient instance.
 */
public interface GoblinClient extends AutoCloseable {

    /* ------------------     put      ------------------ */

    PutResponse put(PutRequest request) throws GoblinException;

    default PutResponse put(KeyType key, ValueType value) throws GoblinException {
        PutRequest request = new PutRequest();
        request.setKey(key);
        request.setValue(value);
        request.setEnableTTL(false);
        return put(request);
    }

    default PutResponse putWithTTL(KeyType key, ValueType value) throws GoblinException {
        return putWithTTL(key, value, null);
    }

    default PutResponse putWithTTL(KeyType key, ValueType value, Integer ttl) throws GoblinException {
        PutRequest request = new PutRequest();
        request.setKey(key);
        request.setValue(value);
        request.setEnableTTL(true);
        request.setTtl(ttl);
        return put(request);
    }

    default PutResponse put(String key, String value) {
        return put(KeyType.builder().key(key).build(), ValueType.builder().content(value.getBytes()).build());
    }

    default PutResponse putWithTTL(String key, String value) throws GoblinException {
        return putWithTTL(KeyType.builder().key(key).build(), ValueType.builder().content(value.getBytes()).build());
    }

    default PutResponse putWithTTL(String key, String value, Integer ttl) throws GoblinException {
        return putWithTTL(KeyType.builder().key(key).build(), ValueType.builder().content(value.getBytes()).build(), ttl);
    }


    /* -----------------    put with condition    ----------------- */

    PutResponse put(PutRequest request, ExistCondition existCondition) throws GoblinException;

    default PutResponse putnx(KeyType key, ValueType value) throws GoblinException {
        PutRequest request = new PutRequest();
        request.setKey(key);
        request.setValue(value);
        request.setEnableTTL(false);
        ExistCondition condition = ExistCondition.builder().key(key).existed(false).build();
        return put(request, condition);
    }

    default PutResponse putnxWithTTL(KeyType key, ValueType value) throws GoblinException {
        return putnxWithTTL(key, value, null);
    }

    default PutResponse putnxWithTTL(KeyType key, ValueType value, Integer ttl) throws GoblinException {
        PutRequest request = new PutRequest();
        request.setKey(key);
        request.setValue(value);
        request.setEnableTTL(true);
        request.setTtl(ttl);
        ExistCondition condition = ExistCondition.builder().key(key).existed(false).build();
        return put(request, condition);
    }

    default PutResponse putnx(String key, String value) throws GoblinException {
        return putnx(KeyType.builder().key(key).build(), ValueType.builder().content(value.getBytes()).build());
    }

    default PutResponse putnxWithTTL(String key, String value) throws GoblinException {
        return putnxWithTTL(KeyType.builder().key(key).build(), ValueType.builder().content(value.getBytes()).build());
    }

    default PutResponse putnxWithTTL(String key, String value, Integer ttl) throws GoblinException {
        return putnxWithTTL(KeyType.builder().key(key).build(), ValueType.builder().content(value.getBytes()).build(), ttl);
    }


    /* -----------------       cas       ----------------- */

    CasResponse cas(CasRequest request) throws GoblinException;

    default CasResponse cas(KeyType key, ValueType value, long comparedVersion) throws GoblinException {
        CasRequest request = new CasRequest();
        request.setKey(key);
        request.setValue(value);
        request.setComparedVersion(comparedVersion);
        request.setEnableTTL(false);
        return cas(request);
    }

    default CasResponse casWithTTL(KeyType key, ValueType value,
                                   long comparedVersion) throws GoblinException {
        return casWithTTL(key, value, comparedVersion, null);
    }

    default CasResponse casWithTTL(KeyType key, ValueType value,
                                   long comparedVersion, Integer ttl) throws GoblinException {
        CasRequest request = new CasRequest();
        request.setKey(key);
        request.setValue(value);
        request.setComparedVersion(comparedVersion);
        request.setEnableTTL(true);
        request.setTtl(ttl);
        return cas(request);
    }

    default CasResponse cas(String key, String value, long comparedVersion) throws GoblinException  {
        return cas(KeyType.builder().key(key).build(), ValueType.builder().content(value.getBytes()).build(), comparedVersion);
    }

    default CasResponse casWithTTL(String key, String value,
                                   long comparedVersion) throws GoblinException {
        return casWithTTL(KeyType.builder().key(key).build(), ValueType.builder().content(value.getBytes()).build(), comparedVersion);
    }

    default CasResponse casWithTTL(String key, String value,
                                   long comparedVersion, Integer ttl) throws GoblinException  {
        return casWithTTL(KeyType.builder().key(key).build(),
                ValueType.builder().content(value.getBytes()).build(), comparedVersion, ttl);
    }


    /* -----------------    get    ----------------- */

    GetResponse get(GetRequest request) throws GoblinException;

    default GetResponse get(KeyType key) throws GoblinException {
        return get(key, null);
    }

    default GetResponse getWithMeta(KeyType key) throws GoblinException {
        return getWithMeta(key, false);
    }

    default GetResponse getWithMeta(KeyType key, boolean allowStale) throws GoblinException {
        return get(key, null, allowStale, true);
    }

    default GetResponse get(KeyType key, Long version) throws GoblinException {
        return get(key, version, false, false);
    }

    default GetResponse get(KeyType key, Long version, boolean allowStale, boolean metaReturned) throws GoblinException {
        GetRequest request = new GetRequest();
        request.setKey(key);
        request.setVersion(version);
        request.setAllowStale(allowStale);
        request.setMetaReturn(metaReturned);
        return get(request);
    }

    default GetResponse get(String key) {
        return get(KeyType.builder().key(key).build());
    }

    default GetResponse get(String key, Long version) {
        return get(KeyType.builder().key(key).build(), version);
    }

    default GetResponse get(String key, boolean allowStale) {
        return get(KeyType.builder().key(key).build(), null, allowStale, false);
    }

    default GetResponse get(String key, Long version, boolean allowStale) {
        return get(KeyType.builder().key(key).build(), version, allowStale, false);
    }


    /* -----------------      delete      ----------------- */

    DeleteResponse delete(DeleteRequest request) throws GoblinException;

    default DeleteResponse delete(KeyType key) throws GoblinException {
        return delete(key, null, true);
    }

    default DeleteResponse delete(KeyType key, Long version, boolean returnValue) throws GoblinException {
        DeleteRequest request = new DeleteRequest();
        request.setKey(key);
        request.setVersion(version);
        request.setReturnValue(returnValue);
        return delete(request);
    }

    default DeleteResponse delete(String key) throws GoblinException {
        return delete(KeyType.builder().key(key).build());
    }

    default DeleteResponse delete(String key, Long version, boolean returnValue) throws GoblinException {
        return delete(KeyType.builder().key(key).build(), version, returnValue);
    }

    /* -----------------      transaction ----------------- */
    MiniTransaction newTransaction() throws GoblinException;
    TransResponse commitTransaction(MiniTransaction trans) throws GoblinException;
    TransResponse abortTransaction(MiniTransaction trans) throws GoblinException;

    /* ------------------       watch       ------------------ */

    // return watch id as a handler for this watch operation
    // users can save it for further operation like unWatch
    String watch(WatchRequest createWatchRequest) throws GoblinException;

    default String watch(KeyType key, WatchCallback cb) throws GoblinException {
        // pass a 0L as sinceVersion which means to watch all changes by default since this time
        return watch(key, false, 0L, cb);
    }

    // users can set retrieveOnConnected so that they can get initial value when watch stream is connected
    default String watch(KeyType key, boolean retrieveOnConnected, WatchCallback cb) throws GoblinException {
        return watch(key, retrieveOnConnected, 0L, cb);
    }

    // users can use sinceVersion to specify from which version to start watching
    default String watch(KeyType key, boolean retrieveOnConnected, Long sinceVersion, WatchCallback cb) throws GoblinException {
        WatchRequest createWatchRequest = new WatchRequest();
        ArrayList<ReadEntry> list = new ArrayList<ReadEntry>();
        ReadEntry readEntry = new ReadEntry();
        readEntry.setKey(key);
        readEntry.setVersion(sinceVersion);
        list.add(readEntry);
        createWatchRequest.setWatchEntries(list);
        createWatchRequest.setWatchCallback(cb);
        createWatchRequest.setRetrieveOnConnected(retrieveOnConnected);
        return watch(createWatchRequest);
    }

    default String watch(String key, WatchCallback cb) {
        return watch(KeyType.builder().key(key).build(), cb);
    }

    default String watch(String key, boolean retrieveOnConnected, WatchCallback cb) throws GoblinException {
        return watch(KeyType.builder().key(key).build(), retrieveOnConnected, 0L, cb);
    }

    default String watch(String key, boolean retrieveOnConnected, Long sinceVersion, WatchCallback cb) throws GoblinException {
        return watch(KeyType.builder().key(key).build(), retrieveOnConnected, sinceVersion, cb);
    }


    void unWatch(String watchId) throws GoblinException;


    /* ------------------       generate       ------------------ */

    GenerateResponse generate(GenerateRequest request);

    // TODO: support generate across sharding clusters
    default GenerateResponse generate(
            Userdefine.UserDefine.CommandType cmdType,
            List<KeyType> srcKeys,
            List<KeyType> tgtKeys,
            ByteString inputInfo
    ) {
        GenerateRequest request = new GenerateRequest();
        request.setCmdType(cmdType);
        request.setSrcKeys(srcKeys);
        request.setTgtKeys(tgtKeys);
        request.setInputInfo(inputInfo);
        return generate(request);
    }


    /* ------------------       lock       ------------------ */

    boolean lock(LockRequest request) throws GoblinException;

    default boolean lock(KeyType key) throws GoblinException {
        return lock(key, ValueType.builder().content("dummy".getBytes()).build(), null);
    }

    default boolean lock(KeyType key, Integer timeout) throws GoblinException {
        return lock(key, ValueType.builder().content("dummy".getBytes()).build(), timeout);
    }

    default boolean lock(KeyType key, ValueType value) throws GoblinException {
        return lock(key, value, null);
    }

    default boolean lock(KeyType key, ValueType value, Integer timeout) throws GoblinException {
        LockRequest request = new LockRequest();
        request.setKey(key);
        request.setValue(value);
        request.setTtl(timeout);
        return lock(request);
    }

    default boolean lock(String key) throws GoblinException {
        return lock(KeyType.builder().key(key).build());
    }

    default boolean lock(String key, Integer timeout) throws GoblinException {
        return lock(KeyType.builder().key(key).build(), timeout);
    }

    default boolean lock(String key, String value) throws GoblinException {
        return lock(KeyType.builder().key(key).build(), ValueType.builder().content(value.getBytes()).build());
    }

    boolean unlock(KeyType key) throws GoblinException;

    default boolean unlock(String key) throws GoblinException {
        return unlock(KeyType.builder().key(key).build());
    }

    static GoblinClient newInstance(RaftClusterClientConfig config) {
        if (config.getLogLevel() != null) {
            LogUtils.setLogLevel(config.getLogLevel());
        }
        return new ShardingClientImpl(config);
    }
}
