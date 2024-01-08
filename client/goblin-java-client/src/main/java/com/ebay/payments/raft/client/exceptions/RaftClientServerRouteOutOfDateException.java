package com.ebay.payments.raft.client.exceptions;

public class RaftClientServerRouteOutOfDateException extends RaftClientException {

    public RaftClientServerRouteOutOfDateException() {
    }

    public RaftClientServerRouteOutOfDateException(String message) {
        super(message);
    }

    public RaftClientServerRouteOutOfDateException(String message, Throwable cause) {
        super(message, cause);
    }

    public RaftClientServerRouteOutOfDateException(Throwable cause) {
        super(cause);
    }
}
