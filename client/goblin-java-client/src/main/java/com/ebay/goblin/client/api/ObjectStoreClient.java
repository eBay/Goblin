package com.ebay.goblin.client.api;


import com.ebay.goblin.client.exceptions.GoblinException;
import com.ebay.goblin.client.model.*;
import com.ebay.goblin.client.model.common.ExistCondition;
import com.ebay.goblin.client.model.common.RouteResult;
import com.ebay.payments.raft.client.RaftClusterClient;

public interface ObjectStoreClient extends AutoCloseable {

    PutResponse put(PutRequest request);

    PutResponse put(PutRequest request, ExistCondition condition);

    CasResponse cas(CasRequest request);

    GetResponse get(GetRequest request);

    DeleteResponse delete(DeleteRequest request);

    TransResponse exeTrans(TransRequest request);

    WatchAckResponse asyncWatch(WatchRequest createWatchRequest) throws GoblinException;

    void asyncUnWatch(String watchId) throws GoblinException;

    GenerateResponse generate(GenerateRequest request);

}
