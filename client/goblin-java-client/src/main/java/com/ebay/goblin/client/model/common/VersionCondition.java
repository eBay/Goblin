package com.ebay.goblin.client.model.common;

import lombok.Builder;
import lombok.Getter;
import lombok.Setter;
import lombok.ToString;

@Getter
@Setter
@ToString(callSuper = true)
@Builder
public class VersionCondition implements Condition {

    private KeyType key;
    private CompareOp compareOp;
    private Long version;
}
