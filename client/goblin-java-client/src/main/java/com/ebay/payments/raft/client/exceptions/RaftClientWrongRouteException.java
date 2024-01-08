package com.ebay.payments.raft.client.exceptions;

public class RaftClientWrongRouteException extends RaftClientException {

    public RaftClientWrongRouteException() {
    }

    public RaftClientWrongRouteException(String message, Throwable cause) {
        super(message, cause);
    }

    public RaftClientWrongRouteException(Throwable cause) {
        super(cause);
    }

    public RaftClientWrongRouteException(String message) {
        super(message);
    }
}
