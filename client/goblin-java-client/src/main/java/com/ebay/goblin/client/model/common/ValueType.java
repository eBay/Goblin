package com.ebay.goblin.client.model.common;

import lombok.Builder;
import lombok.Getter;
import lombok.Setter;
import lombok.ToString;


@Getter
@Setter
@Builder
@ToString(callSuper = true)
public class ValueType {

    protected byte[] content;
    protected Integer offset;
    protected Integer size;

}
