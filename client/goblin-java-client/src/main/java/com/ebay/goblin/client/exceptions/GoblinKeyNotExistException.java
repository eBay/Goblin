package com.ebay.goblin.client.exceptions;

public class GoblinKeyNotExistException extends GoblinException {

    public GoblinKeyNotExistException(String message) {
        super(message);
    }

    public GoblinKeyNotExistException(String message, Throwable cause) {
        super(message, cause);
    }

    public GoblinKeyNotExistException(Throwable cause) {
        super(cause);
    }
}
