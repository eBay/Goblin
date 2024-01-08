package com.ebay.payments.raft.client.exceptions;

public class RaftClientKeyNotExistException extends RaftClientException {

    public RaftClientKeyNotExistException() {
    }

    public RaftClientKeyNotExistException(String message) {
        super(message);
    }

    public RaftClientKeyNotExistException(String message, Throwable cause) {
        super(message, cause);
    }

    public RaftClientKeyNotExistException(Throwable cause) {
        super(cause);
    }
}