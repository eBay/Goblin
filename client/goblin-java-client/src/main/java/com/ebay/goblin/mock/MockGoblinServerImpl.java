package com.ebay.goblin.mock;

import com.google.protobuf.ByteString;
import goblin.proto.Common;
import goblin.proto.KVStoreGrpc;
import goblin.proto.Service;
import io.grpc.stub.StreamObserver;

import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicLong;

class MockGoblinServerImpl extends KVStoreGrpc.KVStoreImplBase {

    private Map<String, ValueWrapper<ByteString>> store = new ConcurrentHashMap<>();
    private AtomicLong version = new AtomicLong(0l);

    @Override
    public synchronized void connect(Service.Connect.Request request, StreamObserver<Service.Connect.Response> responseObserver) {
        Service.Connect.Response response = Service.Connect.Response.newBuilder()
                .setHeader(buildHeader())
                .build();
        responseObserver.onNext(response);
        responseObserver.onCompleted();
    }

    @Override
    public synchronized void put(Service.Put.Request request, StreamObserver<Service.Put.Response> responseObserver) {
        Service.Write.Result result = putEntry(request.getEntry(), false);
        Service.Put.Response response = Service.Put.Response.newBuilder()
                .setHeader(buildHeader())
                .setResult(result)
                .build();
        responseObserver.onNext(response);
        responseObserver.onCompleted();
    }

    @Override
    public synchronized void get(Service.Get.Request request, StreamObserver<Service.Get.Response> responseObserver) {
        Service.Read.Result result = getEntry(request.getEntry(), false);
        Service.Get.Response response = Service.Get.Response.newBuilder()
                .setHeader(buildHeader())
                .setResult(result)
                .build();
        responseObserver.onNext(response);
        responseObserver.onCompleted();
    }

    @Override
    public synchronized void delete(Service.Delete.Request request, StreamObserver<Service.Delete.Response> responseObserver) {
        Service.Remove.Result result = deleteEntry(request.getEntry(), request.getReturnValue());
        Service.Delete.Response response = Service.Delete.Response.newBuilder()
                .setHeader(buildHeader())
                .setResult(result)
                .build();
        responseObserver.onNext(response);
        responseObserver.onCompleted();
    }

    @Override
    public synchronized void exeBatch(Service.ExeBatch.Request request, StreamObserver<Service.ExeBatch.Response> responseObserver) {
        super.exeBatch(request, responseObserver);
    }

    @Override
    public StreamObserver<Service.Watch.Request> watch(StreamObserver<Service.Watch.Response> responseObserver) {
        return super.watch(responseObserver);
    }

    @Override
    public synchronized void transaction(Service.Transaction.Request request, StreamObserver<Service.Transaction.Response> responseObserver) {
        Service.Transaction.Response.Builder responseBuilder = Service.Transaction.Response.newBuilder();
        if (!matchCondition(request.getPrecondsList())) {
            responseBuilder.setHeader(Common.ResponseHeader.newBuilder()
                    .setCode(Common.ResponseCode.PRECOND_NOT_MATCHED)
                    .build());
        } else {
            Common.ResponseCode code = Common.ResponseCode.OK;
            for (Service.ReadWrite.Entry entry: request.getEntriesList()) {
                Service.ReadWrite.Result readWriteResult = null;
                Common.ResponseCode resultCode = Common.ResponseCode.UNKNOWN;
                if (entry.getReadEntry() != null
                        && !Service.Read.Entry.getDefaultInstance().equals(entry.getReadEntry())) {
                    Service.Read.Result readResult = getEntry(entry.getReadEntry(), true);
                    resultCode = readResult.getCode();
                    readWriteResult = Service.ReadWrite.Result.newBuilder()
                            .setReadResult(readResult).build();
                } else if (entry.getWriteEntry() != null
                        && !Service.Write.Entry.getDefaultInstance().equals(entry.getWriteEntry())) {
                    Service.Write.Result writeResult = putEntry(entry.getWriteEntry(), true);
                    resultCode = writeResult.getCode();
                    readWriteResult = Service.ReadWrite.Result.newBuilder()
                            .setWriteResult(writeResult).build();
                }
                if (readWriteResult != null) {
                    responseBuilder.addResults(readWriteResult);
                    if (resultCode.getNumber() > code.getNumber()) {
                        code = resultCode;
                    }
                }
            }
            responseBuilder.setHeader(Common.ResponseHeader.newBuilder().setCode(code).build());
        }
        Service.Transaction.Response response = responseBuilder.build();
        responseObserver.onNext(response);
        responseObserver.onCompleted();
    }

    private boolean matchCondition(List<Service.Precondition> preconds) {
        boolean matched = true;
        for (Service.Precondition precond: preconds) {
            if (precond.hasVersionCond()) {
                matched = false;
                String conditionKey = precond.getVersionCond().getKey().toStringUtf8();
                Service.Precondition.CompareOp op = precond.getVersionCond().getOp();
                ValueWrapper<ByteString> storeValue = store.get(conditionKey);
                switch (op) {
                    case EQUAL:
                        matched = storeValue != null
                                && storeValue.getVersion() == precond.getVersionCond().getVersion();
                        break;
                    case NOT_EQUAL:
                        matched = storeValue == null
                                || storeValue.getVersion() != precond.getVersionCond().getVersion();
                        break;
                    case GREATER:
                        matched = storeValue != null
                                && storeValue.getVersion() < precond.getVersionCond().getVersion();
                        break;
                    case LESS:
                        matched = storeValue != null
                                && storeValue.getVersion() > precond.getVersionCond().getVersion();
                        break;
                    default:
                        break;
                }
            }
            if (!matched) {
                break;
            }

            if (precond.hasExistCond()) {
                String conditionKey = precond.getExistCond().getKey().toStringUtf8();
                ValueWrapper<ByteString> storeValue = store.get(conditionKey);
                matched = precond.getExistCond().getShouldExist() == (storeValue != null);
            }
            if (!matched) {
                break;
            }
        }
        return matched;
    }

    private Service.Write.Result putEntry(Service.Write.Entry requestEntry, boolean isTransaction) {
        try {
            String key = requestEntry.getKey().toStringUtf8();
            mockTimeout(key);
            long newVersion = this.version.incrementAndGet();
            store.put(key, new ValueWrapper<>(requestEntry.getValue(), newVersion));
            Service.Write.Result result = isTransaction ?
                    Service.Write.Result.newBuilder()
                            .setVersion(newVersion)
                            .build()
                    :
                    Service.Write.Result.newBuilder()
                    .setCode(Common.ResponseCode.OK)
                    .setVersion(newVersion)
                    .build();
            return result;
        } catch (Exception e) {
            return Service.Write.Result.newBuilder()
                    .setCode(Common.ResponseCode.GENERAL_ERROR)
                    .build();
        }
    }

    private Service.Read.Result getEntry(Service.Read.Entry requestEntry, boolean isTransaction) {
        try {
            String key = requestEntry.getKey().toStringUtf8();
            mockTimeout(key);
            ValueWrapper<ByteString> value = store.get(key);
            if (value == null) {
                return Service.Read.Result.newBuilder()
                        .setCode(Common.ResponseCode.KEY_NOT_EXIST)
                        .build();
            } else {
                return isTransaction ?
                        Service.Read.Result.newBuilder()
                                .setValue(value.getValue())
                                .setVersion(value.getVersion())
                                .build()
                        :
                        Service.Read.Result.newBuilder()
                                .setCode(Common.ResponseCode.OK)
                                .setValue(value.getValue())
                                .setVersion(value.getVersion())
                                .build();
            }
        } catch (Exception e) {
            return Service.Read.Result.newBuilder()
                    .setCode(Common.ResponseCode.GENERAL_ERROR)
                    .build();
        }
    }

    private Service.Remove.Result deleteEntry(Service.Remove.Entry requestEntry, boolean returnValue) {
        try {
            String key = requestEntry.getKey().toStringUtf8();
            mockTimeout(key);
            ValueWrapper<ByteString> value = store.remove(key);
            Common.ResponseCode code = value == null ? Common.ResponseCode.KEY_NOT_EXIST : Common.ResponseCode.OK;
            Service.Remove.Result result = returnValue ?
                    Service.Remove.Result.newBuilder()
                            .setCode(code)
                            .setValue(value.getValue())
                            .setVersion(value.getVersion())
                            .build()
                    :
                    Service.Remove.Result.newBuilder()
                            .setCode(code)
                            .build();
            return result;
        } catch (Exception e) {
            return Service.Remove.Result.newBuilder()
                    .setCode(Common.ResponseCode.GENERAL_ERROR)
                    .build();
        }
    }

    private Common.ResponseHeader buildHeader() {
        return Common.ResponseHeader.newBuilder()
                .setLatestVersion(this.version.longValue())
                .setLeaderHint("1")
                .build();
    }

    private boolean mockTimeout(String key) {
        if (MockGoblinServer.KEY_TIMEOUT.equalsIgnoreCase(key)) {
            try {
                Thread.sleep(1200l);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
            return true;
        }
        return false;
    }

    static class ValueWrapper<T> {

        private T value;
        private long version;

        public ValueWrapper(T value, long version) {
            this.value = value;
            this.version = version;
        }

        public T getValue() {
            return value;
        }

        public long getVersion() {
            return version;
        }
    }
}
