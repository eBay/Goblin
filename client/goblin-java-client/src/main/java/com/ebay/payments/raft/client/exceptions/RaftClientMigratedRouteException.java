package com.ebay.payments.raft.client.exceptions;

public class RaftClientMigratedRouteException extends RaftClientException {

    private String redirectAddress;

    public String getRedirectAddress() {
        return redirectAddress;
    }

    public RaftClientMigratedRouteException(String redirectAddress) {
        this.redirectAddress = redirectAddress;
    }
}
