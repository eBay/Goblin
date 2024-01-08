package com.ebay.goblin.client.model.common;

import lombok.Getter;
import lombok.Setter;
import lombok.ToString;


@Getter
@Setter
@ToString(callSuper = true)
public class ReadEntry extends AbstractEntry {

    protected KeyType key;
    protected Long version;

}
