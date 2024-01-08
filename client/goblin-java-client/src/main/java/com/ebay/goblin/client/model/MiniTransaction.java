package com.ebay.goblin.client.model;

import com.ebay.goblin.client.model.common.*;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.concurrent.atomic.AtomicInteger;

public class MiniTransaction {

    private static final Logger logger = LoggerFactory.getLogger(MiniTransaction.class);

    public enum TransType {
        BATCH,
        DISTRIBUTED
    }

    static AtomicInteger allocatedTransId = new AtomicInteger(0);

    static Integer generateNewTransactionId() {
        return allocatedTransId.incrementAndGet();
    }

    public MiniTransaction(TransType type) {
        this.id = generateNewTransactionId();
        this.type = type;
        this.request = new TransRequest();
    }

    public Integer getTransactionId() {
        return this.id;
    }

    public void withExistCond(String key, boolean expectedExist) {
        ExistCondition cond = ExistCondition.builder().key(KeyType.builder().key(key).build()).existed(expectedExist).build();
        request.addPrecond(cond);
    }

    public void withVersionCond(String key, Long version, CompareOp compareOp) {
        VersionCondition cond = VersionCondition.builder().key(KeyType.builder().key(key).build()).compareOp(compareOp).version(version).build();
        request.addPrecond(cond);
    }

    public void withUdfCond(KeyType key,
                            UserDefinedMeta meta,
                            CompareOp compareOp,
                            UdfMetaType udfMetaType,
                            int pos) {
        UserMetaCondition cond = UserMetaCondition
                .builder()
                .key(key)
                .op(compareOp)
                .pos(pos)
                .metaType(udfMetaType)
                .udfMeta(meta).build();
        request.addPrecond(cond);
    }

    public void put(String key, String value) {
        put(KeyType.builder().key(key).build(), ValueType.builder().content(value.getBytes()).build());
    }

    public void put(KeyType key, ValueType value) {
        PutRequest putRequest = new PutRequest();
        putRequest.setKey(key);
        putRequest.setValue(value);
        request.addRequest(putRequest);
    }

    public void putWithUserDefinedMeta(KeyType key, ValueType value, UserDefinedMeta userDefinedMeta) {
        PutRequest putRequest = new PutRequest();
        putRequest.setKey(key);
        putRequest.setValue(value);
        putRequest.setUdfMeta(userDefinedMeta);
        request.addRequest(putRequest);
    }

    public void putWithUserDefinedMeta(String key, String value, UserDefinedMeta userDefinedMeta) {
        putWithUserDefinedMeta(KeyType.builder().key(key).build(), ValueType.builder().content(value.getBytes()).build(), userDefinedMeta);
    }

    public void get(String key) {
        get(KeyType.builder().key(key).build());
    }

    public void get(KeyType key) {
        GetRequest getRequest = new GetRequest();
        getRequest.setKey(key);
        request.addRequest(getRequest);
    }

    public void delete(String key) {
        delete(KeyType.builder().key(key).build());
    }

    public void delete(KeyType key) {
        DeleteRequest deleteRequest = new DeleteRequest();
        deleteRequest.setKey(key);
        request.addRequest(deleteRequest);
    }

    public TransRequest getRequest() {
        return request;
    }

    public void allowStale() {
        for (AbstractEntry entry : request.getRequests()) {
            if (!(entry instanceof ReadEntry)) {
                logger.warn("trans has non-read entry, allow stale not supported.");
                return;
            }
        }
        request.setAllowStale(true);
    }

    private final Integer id;
    private final TransType type;
    private final TransRequest request;
}
