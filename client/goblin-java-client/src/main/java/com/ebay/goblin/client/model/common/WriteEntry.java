package com.ebay.goblin.client.model.common;

import lombok.Getter;
import lombok.Setter;
import lombok.ToString;


@Getter
@Setter
@ToString(callSuper = true)
public class WriteEntry extends AbstractEntry {

    protected KeyType key;
    protected ValueType value;
    protected Boolean enableTTL = false;
    protected Integer ttl;
    protected UserDefinedMeta udfMeta;

}
