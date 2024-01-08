package com.ebay.payments.raft.client.exceptions;

public class RaftClientException extends RuntimeException {
    public RaftClientException() {
    }

    public RaftClientException(String message) {
        super(message);
    }

    public RaftClientException(String message, Throwable cause) {
        super(message, cause);
    }

    public RaftClientException(Throwable cause) {
        super(cause);
    }

    public RaftClientException(String message, Throwable cause, boolean enableSuppression, boolean writableStackTrace) {
        super(message, cause, enableSuppression, writableStackTrace);
    }
}
