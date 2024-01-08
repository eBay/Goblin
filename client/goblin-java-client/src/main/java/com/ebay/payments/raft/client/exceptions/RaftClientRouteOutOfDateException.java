package com.ebay.payments.raft.client.exceptions;


public class RaftClientRouteOutOfDateException extends RaftClientException {

    public RaftClientRouteOutOfDateException() {
    }

    public RaftClientRouteOutOfDateException(String message) {
        super(message);
    }

    public RaftClientRouteOutOfDateException(String message, Throwable cause) {
        super(message, cause);
    }

    public RaftClientRouteOutOfDateException(Throwable cause) {
        super(cause);
    }
}
