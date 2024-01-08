package com.ebay.goblin.client.exceptions;

public class GoblinException extends RuntimeException {

    public GoblinException() {
    }

    public GoblinException(String message) {
        super(message);
    }

    public GoblinException(String message, Throwable cause) {
        super(message, cause);
    }

    public GoblinException(Throwable cause) {
        super(cause);
    }

}
