package com.ebay.goblin.client.model.common;

import lombok.Builder;
import lombok.Getter;
import lombok.Setter;
import lombok.ToString;

@Getter
@Setter
@ToString(callSuper = true)
@Builder
public class ExistCondition implements Condition {

    private KeyType key;
    private boolean existed;

}
