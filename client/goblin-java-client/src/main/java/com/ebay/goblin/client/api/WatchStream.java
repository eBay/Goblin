package com.ebay.goblin.client.api;

import com.ebay.goblin.client.exceptions.GoblinException;
import goblin.proto.Service;

// encapsulate the request and response stream
public interface WatchStream extends AutoCloseable {

    // blocked until there is data or return null if timeout reached
    Service.Watch.Response pullNextResponse(long timeoutInMillSec) throws GoblinException;
    Service.Watch.Response peekNextResponse(long timeoutInMillSec) throws GoblinException;

    // send new request to server
    void pushNewRequest(Service.Watch.Request request);

    void cancel(String watchId);

    Throwable getError();

    boolean isShutDown();
}
