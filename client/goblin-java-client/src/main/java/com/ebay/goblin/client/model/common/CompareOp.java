package com.ebay.goblin.client.model.common;

import com.ebay.goblin.client.exceptions.GoblinInvalidRequestException;
import goblin.proto.Service;

public enum CompareOp {

    EQUAL,
    GREATER,
    LESS,
    NOT_EQUAL,
    ;

    public static Service.Precondition.CompareOp toProtoCompareOp(CompareOp op) {
        if (op == EQUAL) {
            return Service.Precondition.CompareOp.EQUAL;
        }
        if (op == GREATER) {
            return Service.Precondition.CompareOp.GREATER;
        }
        if (op == LESS) {
            return Service.Precondition.CompareOp.LESS;
        }
        if (op == NOT_EQUAL) {
            return Service.Precondition.CompareOp.NOT_EQUAL;
        }
        throw new GoblinInvalidRequestException("invalid compare op");
    }
}
