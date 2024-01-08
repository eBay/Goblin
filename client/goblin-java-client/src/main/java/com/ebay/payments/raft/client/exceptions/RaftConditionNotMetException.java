package com.ebay.payments.raft.client.exceptions;

public class RaftConditionNotMetException extends RaftClientException {

    public RaftConditionNotMetException(String message) {
        super(message);
    }

}
