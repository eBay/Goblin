package com.ebay.payments.raft.client.exceptions;

public class RaftInvokeTimeoutException extends RaftClientException {

    public RaftInvokeTimeoutException() {
    }

    public RaftInvokeTimeoutException(String message) {
        super(message);
    }
}
