package com.ebay.goblin.client.exceptions;

public class GoblinConditionNotMetException extends GoblinException {

    public GoblinConditionNotMetException() {
    }

    public GoblinConditionNotMetException(String message) {
        super(message);
    }

    public GoblinConditionNotMetException(String message, Throwable cause) {
        super(message, cause);
    }

    public GoblinConditionNotMetException(Throwable cause) {
        super(cause);
    }

}
