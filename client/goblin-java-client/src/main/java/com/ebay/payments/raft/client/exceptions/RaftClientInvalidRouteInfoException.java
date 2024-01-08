package com.ebay.payments.raft.client.exceptions;

public class RaftClientInvalidRouteInfoException extends RaftClientException {

    public RaftClientInvalidRouteInfoException() {
    }

    public RaftClientInvalidRouteInfoException(String message) {
        super(message);
    }

    public RaftClientInvalidRouteInfoException(String message, Throwable cause) {
        super(message, cause);
    }

    public RaftClientInvalidRouteInfoException(Throwable cause) {
        super(cause);
    }
}
