package com.ebay.goblin.client.exceptions;

public class GoblinInternalException extends GoblinException {

    public GoblinInternalException() {
    }

    public GoblinInternalException(String message) {
        super(message);
    }

    public GoblinInternalException(Throwable cause) {
        super(cause);
    }

    public GoblinInternalException(String message, Throwable cause) {
        super(message, cause);
    }
}
