package com.ebay.goblin.client.exceptions;

public class GoblinInvalidRequestException extends GoblinException {

    public GoblinInvalidRequestException() {
    }

    public GoblinInvalidRequestException(String message) {
        super(message);
    }

    public GoblinInvalidRequestException(String message, Throwable cause) {
        super(message, cause);
    }

    public GoblinInvalidRequestException(Throwable cause) {
        super(cause);
    }

}
