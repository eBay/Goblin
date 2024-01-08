package com.ebay.goblin.client.api.impl.internal;

import com.ebay.goblin.client.api.WatchStream;
import com.ebay.goblin.client.exceptions.GoblinInvalidRequestException;
import com.ebay.payments.raft.client.exceptions.RaftClientInvalidRouteInfoException;
import com.ebay.goblin.client.model.common.*;
import com.ebay.payments.raft.client.exceptions.RaftRequestConvertionException;
import com.ebay.payments.raft.client.exceptions.RaftResponseConvertionException;
import com.ebay.goblin.client.model.*;
import com.ebay.payments.raft.client.ConnectionContext;
import com.google.common.collect.Lists;
import com.google.common.collect.Sets;
import com.google.protobuf.ByteString;
import goblin.proto.Control;
import goblin.proto.Common;
import goblin.proto.Service;
import goblin.proto.Userdefine;
import gringofts.app.protos.Netadmin;

import java.util.*;

public class PayloadConverters {

    private static final long EMPTY_VERSION = 0L;

    private Service.Read.Entry toReadEntry(ReadEntry readEntry) {
        if (readEntry == null) {
            return null;
        }
        return Service.Read.Entry.newBuilder()
                .setKey(ByteString.copyFromUtf8(readEntry.getKey().getKey()))
                .setVersion(readEntry.getVersion() == null ? EMPTY_VERSION : readEntry.getVersion())
                .build();
    }

    private Service.Write.Entry toWriteEntry(WriteEntry writeEntry) {
        if (writeEntry == null) {
            return null;
        }
        ValueType value = writeEntry.getValue();
        Service.Write.Entry.Builder builder = Service.Write.Entry.newBuilder()
                .setKey(ByteString.copyFromUtf8(writeEntry.getKey().getKey()));
        if (value.getOffset() == null || value.getSize() == null) {
            builder.setValue(ByteString.copyFrom(value.getContent()));
        } else {
            builder.setValue(
                    ByteString.copyFrom(value.getContent(), value.getOffset(), value.getSize()));
        }
        if (writeEntry.getEnableTTL()) {
            builder.setEnableTTL(true);
            if (writeEntry.getTtl() != null) {
                builder.setTtl(writeEntry.getTtl());
            }
        }
        if (writeEntry.getUdfMeta() != null) {
            builder.setUdfMeta(writeEntry.getUdfMeta().toProto());
        }
        return builder.build();
    }

    private Service.Remove.Entry toRemoveEntry(ReadEntry readEntry) {
        if (readEntry == null) {
            return null;
        }
        return Service.Remove.Entry.newBuilder()
                .setKey(ByteString.copyFromUtf8(readEntry.getKey().getKey()))
                .setVersion(readEntry.getVersion() == null ? EMPTY_VERSION : readEntry.getVersion())
                .build();
    }

    private Service.ReadWrite.Entry toReadWriteEntry(AbstractEntry entry) {
        if (entry == null) {
            return null;
        }
        if (entry instanceof PutRequest || entry instanceof CasRequest) {
            WriteEntry writeEntry = (WriteEntry) entry;
            return Service.ReadWrite.Entry.newBuilder()
                    .setWriteEntry(toWriteEntry(writeEntry))
                    .build();
        } else if (entry instanceof GetRequest) {
            ReadEntry readEntry = (ReadEntry) entry;
            return Service.ReadWrite.Entry.newBuilder()
                    .setReadEntry(toReadEntry(readEntry))
                    .build();
        } else if (entry instanceof DeleteRequest) {
            DeleteRequest deleteEntry = (DeleteRequest) entry;
            return Service.ReadWrite.Entry.newBuilder()
                    .setRemoveEntry(toRemoveEntry(deleteEntry))
                    .build();
        } else {
            throw new RaftRequestConvertionException("toReadWriteEntry", new GoblinInvalidRequestException("invalid entry"));
        }
    }

    private Service.Generate.Entry toGenerateEntry(GenerateEntry generateEntry) {
        if (generateEntry == null) {
            return null;
        }
        Service.Generate.Entry.Builder builder = Service.Generate.Entry.newBuilder()
                .setCmdType(generateEntry.getCmdType())
                .setInputInfo(generateEntry.getInputInfo());
        for (KeyType key : generateEntry.getSrcKeys()) {
            builder.addSrcKeys(ByteString.copyFromUtf8(key.getKey()));
        }
        for (KeyType key : generateEntry.getTgtKeys()) {
            builder.addTgtKeys(ByteString.copyFromUtf8(key.getKey()));
        }
        return builder.build();
    }

    private Optional<ValueType> toValueType(int code, ByteString byteString) {
        if (byteString == null
                || code == Common.ResponseCode.KEY_NOT_EXIST_VALUE
                || code == Common.ResponseCode.UNKNOWN_VALUE) {
            return Optional.empty();
        }
        byte[] bytes = byteString.toByteArray();
        ValueType value = ValueType.builder().content(bytes).offset(0).size(bytes.length).build();
        return Optional.of(value);
    }

    private Common.RequestHeader toKvRequestHeader(ConnectionContext context) {
        if (context == null) {
            return null;
        }
        return Common.RequestHeader.newBuilder()
                .setKnownVersion(context.getKnownVersion())
                .setRouteVersion(context.getRouteVersion())
                .build();
    }

    public Service.Put.Request toKvPutRequest(ConnectionContext context, PutRequest putRequest) {
        try {
            Common.RequestHeader header = toKvRequestHeader(context);
            return Service.Put.Request.newBuilder()
                    .setHeader(header)
                    .setEntry(toWriteEntry(putRequest))
                    .build();
        } catch (Exception e) {
            throw new RaftRequestConvertionException("toKvPutRequest", e);
        }
    }

    public ResponseWrapper<PutResponse> toPutResponse(Service.Put.Response kvPutResponse) {
        try {
            PutResponse putResponse = new PutResponse();
            putResponse.setVersion(kvPutResponse.getResult().getVersion());
            return new ResponseWrapper<>(putResponse,
                    kvPutResponse.getHeader().getCode().getNumber(),
                    kvPutResponse.getResult().getMessage());
        } catch (Exception e) {
            throw new RaftResponseConvertionException("toPutResponse", e);
        }
    }

    public Service.Get.Request toKvGetRequest(ConnectionContext context, GetRequest getRequest) {
        try {
            Common.RequestHeader header = toKvRequestHeader(context);
            return Service.Get.Request.newBuilder()
                    .setHeader(header)
                    .setAllowStale(getRequest.isAllowStale())
                    .setIsMetaReturned(getRequest.isMetaReturn())
                    .setEntry(toReadEntry(getRequest))
                    .build();
        } catch (Exception e) {
            throw new RaftRequestConvertionException("toKvGetRequest", e);
        }
    }

    public UserDefinedMeta toUserDefinedMeta(Userdefine.UserDefinedMeta metaProto) {
        return UserDefinedMeta.builder()
                .strField(metaProto.getStrFieldList())
                .uintField(metaProto.getUintFieldList())
                .build();
    }

    public ResponseWrapper<GetResponse> toGetResponse(Service.Get.Response kvGetResponse) {
        try {
            GetResponse getResponse = new GetResponse();
            Service.Read.Result result = kvGetResponse.getResult();
            getResponse.setUserDefinedMeta(toUserDefinedMeta(result.getUdfMeta()));
            getResponse.setUpdateTime(result.getUpdateTime());
            getResponse.setVersion(result.getVersion());
            getResponse.setValue(toValueType(result.getCode().getNumber(), result.getValue()));
            return new ResponseWrapper<>(getResponse, kvGetResponse.getHeader().getCode().getNumber(), result.getMessage());
        } catch (Exception e) {
            throw new RaftResponseConvertionException("toGetResponse", e);
        }
    }

    public Service.Delete.Request toKvDeleteRequest(ConnectionContext context, DeleteRequest deleteRequest) {
        try {
            Common.RequestHeader header = toKvRequestHeader(context);
            return Service.Delete.Request.newBuilder()
                    .setHeader(header)
                    .setReturnValue(deleteRequest.isReturnValue())
                    .setEntry(toRemoveEntry(deleteRequest))
                    .build();
        } catch (Exception e) {
            throw new RaftRequestConvertionException("toKvDeleteRequest", e);
        }
    }

    public ResponseWrapper<DeleteResponse> toDeleteResponse(Service.Delete.Response kvDeleteResponse) {
        try {
            DeleteResponse deleteResponse = new DeleteResponse();
            Service.Remove.Result result = kvDeleteResponse.getResult();
            int code = kvDeleteResponse.getHeader().getCode().getNumber();
            if (Common.ResponseCode.OK_VALUE == code) {
                code = result.getCode().getNumber();
            }
            deleteResponse.setVersion(result.getVersion());
            deleteResponse.setValue(toValueType(result.getCode().getNumber(), result.getValue()));
            return new ResponseWrapper<>(deleteResponse, code, result.getMessage());
        } catch (Exception e) {
            throw new RaftResponseConvertionException("toDeleteResponse", e);
        }
    }

    public Service.Precondition toKvTransPrecond(Condition c) {
        Service.Precondition.Builder builder = Service.Precondition.newBuilder();
        if (c instanceof VersionCondition) {
            VersionCondition vc = (VersionCondition) c;
            Service.Precondition.VersionCondition condition = Service.Precondition.VersionCondition.newBuilder()
                    .setOp(CompareOp.toProtoCompareOp(vc.getCompareOp()))
                    .setVersion(vc.getVersion())
                    .setKey(ByteString.copyFromUtf8(vc.getKey().getKey()))
                    .build();
            return builder.setVersionCond(condition).build();
        } else if (c instanceof ExistCondition) {
            ExistCondition ec = (ExistCondition) c;
            Service.Precondition.ExistCondition condition = Service.Precondition.ExistCondition.newBuilder()
                    .setShouldExist(ec.isExisted())
                    .setKey(ByteString.copyFromUtf8(ec.getKey().getKey()))
                    .build();
            return builder.setExistCond(condition).build();
        } else if (c instanceof UserMetaCondition) {
            UserMetaCondition uc = (UserMetaCondition) c;
            Service.Precondition.UserMetaCondition condition = Service.Precondition.UserMetaCondition.newBuilder()
                    .setOp(CompareOp.toProtoCompareOp(uc.getOp()))
                    .setMetaType(Service.Precondition.UserMetaCondition.UdfMetaType.valueOf(uc.getMetaType().name()))
                    .setPos(uc.getPos())
                    .setKey(ByteString.copyFromUtf8(uc.getKey().getKey()))
                    .setUdfMeta(uc.getUdfMeta().toProto())
                    .build();
            return builder.setUserMetaCond(condition).build();
        } else {
            throw new RaftRequestConvertionException("toKvTransPrecond", new GoblinInvalidRequestException("invalid precond"));
        }
    }

    public Service.Transaction.Request toKvTransRequest(ConnectionContext context, TransRequest transRequest) {
        try {
            Service.Transaction.Request.Builder builder = Service.Transaction.Request.newBuilder();
            Common.RequestHeader header = toKvRequestHeader(context);
            builder.setHeader(header);
            builder.setIsMetaReturned(true);
            for (Condition c : transRequest.getPreconds()) {
                builder.addPreconds(toKvTransPrecond(c));
            }
            for (AbstractEntry entry : transRequest.getAllRequests()) {
                builder.addEntries(toReadWriteEntry(entry));
            }
            if (transRequest.isAllowStale()) {
                for (AbstractEntry entry : transRequest.getAllRequests()) {
                    if (!(entry instanceof ReadEntry)) {
                        throw new RaftRequestConvertionException("AllowStale request can only include read request", null);
                    }
                }
                builder.setAllowStale(transRequest.isAllowStale());
            }
            return builder.build();
        } catch (Exception e) {
            throw new RaftRequestConvertionException("toKvTransRequest", e);
        }
    }

    public ResponseWrapper<TransResponse> toTransResponse(Service.Transaction.Response kvTransResponse) {
        try {
            TransResponse transResponse = new TransResponse();
            Common.ResponseHeader responseHeader = kvTransResponse.getHeader();
            int code = responseHeader.getCode().getNumber(); // TODO: if header.code == 200, code = transaction.code
            // add ignored keys
            for (ByteString byteString : kvTransResponse.getIgnoredPutKeysList()) {
                transResponse.getIgnoredKeys().add(new String(byteString.toByteArray()));
            }
            StringBuilder messageBuilder = new StringBuilder(responseHeader.getMessage());
            for (Service.ReadWrite.Result result : kvTransResponse.getResultsList()) {
                if (result.hasReadResult()) {
                    GetResponse getResponse = new GetResponse();
                    getResponse.setVersion(result.getReadResult().getVersion());
                    getResponse.setUpdateTime(result.getReadResult().getUpdateTime());
                    getResponse.setUserDefinedMeta(toUserDefinedMeta(result.getReadResult().getUdfMeta()));
                    getResponse.setValue(toValueType(result.getReadResult().getCode().getNumber(), result.getReadResult().getValue()));
                    transResponse.addResult(getResponse);
                    if (result.getReadResult().getMessage() != null) {
                        messageBuilder.append(" [Detail: ").append(result.getReadResult().getMessage()).append("]");
                    }
                } else if (result.hasWriteResult()) {
                    PutResponse putResponse = new PutResponse();
                    putResponse.setVersion(result.getWriteResult().getVersion());
                    transResponse.addResult(putResponse);
                    if (result.getWriteResult().getMessage() != null) {
                        messageBuilder.append(" [Detail: ").append(result.getWriteResult().getMessage()).append("]");
                    }
                } else if (result.hasRemoveResult()) {
                    DeleteResponse deleteResponse = new DeleteResponse();
                    deleteResponse.setVersion(result.getRemoveResult().getVersion());
                    deleteResponse.setValue(toValueType(result.getRemoveResult().getCode().getNumber(), result.getRemoveResult().getValue()));
                    transResponse.addResult(deleteResponse);
                    if (result.getRemoveResult().getMessage() != null) {
                        messageBuilder.append(" [Detail: ").append(result.getRemoveResult().getMessage()).append("]");
                    }
                } else {
                    throw new RaftRequestConvertionException("toTransResponse", new GoblinInvalidRequestException("invalid trans response"));
                }
            }
            return new ResponseWrapper<>(transResponse, code, messageBuilder.toString());
        } catch (Exception e) {
            throw new RaftResponseConvertionException("toTransResponse", e);
        }
    }

    public Service.Transaction.Request toKvCasRequest(ConnectionContext context, CasRequest casRequest) {
        try {
            Common.RequestHeader header = toKvRequestHeader(context);
            Service.Precondition.VersionCondition condition = Service.Precondition.VersionCondition.newBuilder()
                    .setOp(Service.Precondition.CompareOp.EQUAL)
                    .setVersion(casRequest.getComparedVersion())
                    .setKey(ByteString.copyFromUtf8(casRequest.getKey().getKey()))
                    .build();
            Service.Precondition precond = Service.Precondition.newBuilder()
                    .setVersionCond(condition)
                    .build();
            return Service.Transaction.Request.newBuilder()
                    .setHeader(header)
                    .addPreconds(precond)
                    .addEntries(toReadWriteEntry(casRequest))
                    .build();
        } catch (Exception e) {
            throw new RaftRequestConvertionException("toKvCasRequest", e);
        }
    }

    public ResponseWrapper<CasResponse> toCasResponse(Service.Transaction.Response kvTxnResponse) {
        try {
            CasResponse casResponse = new CasResponse();
            Common.ResponseHeader responseHeader = kvTxnResponse.getHeader();
            int code = responseHeader.getCode().getNumber(); // TODO: if header.code == 200, code = transaction.code

            StringBuilder messageBuilder = new StringBuilder(responseHeader.getMessage());
            if (kvTxnResponse.getResultsCount() > 0) {
                Service.Write.Result result = kvTxnResponse.getResults(0).getWriteResult();
                casResponse.setVersion(result.getVersion());
                if (result.getMessage() != null) {
                    messageBuilder.append(" [Detail: ").append(result.getMessage()).append("]");
                }
            }
            return new ResponseWrapper<>(casResponse, code, messageBuilder.toString());
        } catch (Exception e) {
            throw new RaftResponseConvertionException("toCasResponse", e);
        }
    }

    public Service.Transaction.Request toKvPutRequest(ConnectionContext context, PutRequest putRequest, ExistCondition condition) {
        try {
            Service.Precondition precond = Service.Precondition.newBuilder()
                    .setExistCond(Service.Precondition.ExistCondition.newBuilder()
                            .setShouldExist(condition.isExisted())
                            .setKey(ByteString.copyFromUtf8(condition.getKey().getKey()))
                            .build())
                    .build();

            return Service.Transaction.Request.newBuilder()
                    .setHeader(toKvRequestHeader(context))
                    .addPreconds(precond)
                    .addEntries(toReadWriteEntry(putRequest))
                    .build();
        } catch (Exception e) {
            throw new RaftRequestConvertionException("toKvPutWithConditionRequest", e);
        }
    }

    public ResponseWrapper<PutResponse> toPutResponse(Service.Transaction.Response kvTxnResponse) {
        try {
            PutResponse putResponse = new PutResponse();
            Common.ResponseHeader responseHeader = kvTxnResponse.getHeader();
            StringBuilder messageBuilder = new StringBuilder(responseHeader.getMessage());
            if (kvTxnResponse.getResultsCount() > 0) {
                Service.Write.Result result = kvTxnResponse.getResults(0).getWriteResult();
                putResponse.setVersion(result.getVersion());
                if (result.getMessage() != null) {
                    messageBuilder.append(" [Detail: ")
                            .append(result.getMessage())
                            .append("]");
                }
            }
            return new ResponseWrapper<>(putResponse,
                    responseHeader.getCode().getNumber(),
                    messageBuilder.toString());
        } catch (Exception e) {
            throw new RaftResponseConvertionException("toPutWithConditionResponse", e);
        }
    }

    public Service.Watch.Request toKvWatchRequest(ConnectionContext context, WatchRequest watchRequest) {
        try {
            Common.RequestHeader header = toKvRequestHeader(context);
            Service.Watch.CreateRequest.Builder builder = Service.Watch.CreateRequest.newBuilder();
            for (ReadEntry entry : watchRequest.getWatchEntries()) {
                builder.addEntries(toReadEntry(entry));
            }
            builder.setRetrieveOnConnected(watchRequest.isRetrieveOnConnected());
            return Service.Watch.Request.newBuilder()
                    .setHeader(header)
                    .setCreateReq(builder)
                    .build();
        } catch (Exception e) {
            throw new RaftRequestConvertionException("toKvWatchRequest", e);
        }
    }

    public ResponseWrapper<WatchAckResponse> toWatchAckResponse(Service.Watch.Response response, WatchStream stream) {
        try {
            WatchAckResponse ack = new WatchAckResponse();
            ack.setWatchId(response.getWatchId());
            ack.setWatchStream(stream);
            Common.ResponseHeader responseHeader = response.getHeader();
            return new ResponseWrapper<>(ack,
                    responseHeader.getCode().getNumber(),
                    responseHeader.getMessage());
        } catch (Exception e) {
            throw new RaftResponseConvertionException("toWatchAckResponse", e);
        }
    }

    public Service.GenerateKV.Request toKVGenerateRequest(ConnectionContext context, GenerateRequest generateRequest) {
        try {
            Common.RequestHeader header = toKvRequestHeader(context);
            Service.GenerateKV.Request kvGenerateRequest = Service.GenerateKV.Request.newBuilder()
                    .setHeader(header)
                    .setEntry(toGenerateEntry(generateRequest))
                    .build();
            return kvGenerateRequest;
        } catch (Exception e) {
            throw new RaftRequestConvertionException("toKvPutRequest", e);
        }
    }

    public ResponseWrapper<GenerateResponse> toKVGenerateResponse(Service.GenerateKV.Response kvGenerateResponse) {
        try {
            GenerateResponse generateResponse = new GenerateResponse();
            generateResponse.setOutputInfo(kvGenerateResponse.getResult().getOutputInfo());
            return new ResponseWrapper<>(generateResponse,
                    kvGenerateResponse.getHeader().getCode().getNumber(),
                    kvGenerateResponse.getResult().getMessage());
        } catch (Exception e) {
            throw new RaftResponseConvertionException("toPutResponse", e);
        }
    }

    public ResponseWrapper<RouteResult> toRouteResponse(Control.Router.Response response) {
        try {
            Common.RouteInfo routeInfo = response.getRouteInfo();
            if (invalidRouteInfo(routeInfo)) {
                throw new RaftClientInvalidRouteInfoException(
                        "Invalid router info response " + response.getRouteInfo());
            }
            RouteResult result = new RouteResult();
            result.setRouteVersion(routeInfo.getVersion());
            result.setShardFactor(Common.Shard.ShardFactor.DEFAULT_VALUE);
            List<Common.Placement.ClusterWithShardSet> clusters = routeInfo.getPlacementInfo()
                    .getPlacementMap().getClusterWithShardsList();
            List<ShardCluster> shardClusters = Lists.newArrayList();
            clusters.forEach(cluster -> shardClusters.add(
                    ShardCluster.builder()
                            .shardInfo(toShardInfo(cluster.getShardsList()))
                            .cluster(toClusterAddress(cluster.getCluster()))
                            .build()));
            result.setShardClusters(shardClusters);
            Common.ResponseHeader responseHeader = response.getHeader();
            return new ResponseWrapper<>(result, responseHeader.getCode().getNumber(), responseHeader.getMessage());
        } catch (Exception e) {
            throw new RaftResponseConvertionException("toRouteResponse", e);
        }
    }

    public Set<Long> toShardInfo(List<Common.Shard.ShardInfo> shardInfos) {
        try {
            Set<Long> result = Sets.newHashSet();
            shardInfos.forEach(shardInfo
                    -> result.add(shardInfo.getId()));
            return result;
        } catch (Exception e) {
            throw new RaftResponseConvertionException("toShardInfo", e);
        }
    }

    public String toClusterAddress(Common.Cluster.ClusterAddress clusterAddress) {
        try {
            List<String> result = Lists.newArrayList();
            List<Common.Cluster.ServerAddress> servers = clusterAddress.getServersList();
            for (Common.Cluster.ServerAddress server : servers) {
                String address = server.getHostname();
                if (address == null || address.isEmpty() || !address.contains("@")) {
                    return null;
                }
                result.add(address);
            }
            result.sort(Comparator.comparing(
                    address -> Integer.valueOf(address.split("@")[0])));
            return String.join(",", result);
        } catch (Exception e) {
            throw new RaftResponseConvertionException("toClusterAddress", e);
        }
    }

    private boolean invalidRouteInfo(Common.RouteInfo routeInfo) {
        return routeInfo == null
                || routeInfo.getPartitionInfo() == null
                || routeInfo.getPlacementInfo() == null
                || routeInfo.getPlacementInfo().getPlacementMap() == null
                || Common.Partition.PartitionStrategy.HASH_MOD !=
                routeInfo.getPartitionInfo().getStrategy()
                || Common.Placement.PlacementStrategy.CONSIST_HASHING !=
                routeInfo.getPlacementInfo().getStrategy();
    }

    public MemberOffsetResponse toMemberOffsetResponse(Netadmin.GetMemberOffsets.Response grpcResp) {
        MemberOffsetResponse response = new MemberOffsetResponse();
        Netadmin.ServerOffsetInfo leader = grpcResp.getLeader();
        response.setLeaderAddress(leader.getServer());
        response.setLeaderCommitIndex(leader.getOffset());
        for (Netadmin.ServerOffsetInfo follower : grpcResp.getFollowersList()) {
            response.getFollowerMatchedIndex().put(follower.getServer(), follower.getOffset());
        }
        return response;
    }
}
