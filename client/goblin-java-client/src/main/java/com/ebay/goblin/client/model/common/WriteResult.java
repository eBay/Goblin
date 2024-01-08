package com.ebay.goblin.client.model.common;

import lombok.Getter;
import lombok.Setter;
import lombok.ToString;


@Getter
@Setter
@ToString(callSuper = true)
public class WriteResult extends AbstractResult {

    protected Long version;

}
