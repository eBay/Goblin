package com.ebay.goblin.client.api.impl.internal;

import com.ebay.goblin.client.model.common.AbstractResult;
import lombok.AllArgsConstructor;
import lombok.Getter;
import lombok.Setter;
import lombok.ToString;

@Getter
@Setter
@AllArgsConstructor
@ToString
public class ResponseWrapper<REP extends AbstractResult> {

    private REP response;
    private Integer code;
    private String message;

}
