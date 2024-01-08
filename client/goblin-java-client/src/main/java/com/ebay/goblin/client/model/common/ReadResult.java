package com.ebay.goblin.client.model.common;

import lombok.Getter;
import lombok.Setter;
import lombok.ToString;

import java.util.Optional;


@Getter
@Setter
@ToString(callSuper = true)
public class ReadResult extends AbstractResult {

    protected Long version;
    protected Optional<ValueType> value;
    protected long updateTime;
    private UserDefinedMeta userDefinedMeta;

}
