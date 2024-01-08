package com.ebay.goblin.client.model;

import com.ebay.goblin.client.model.common.AbstractResult;
import lombok.Getter;
import lombok.Setter;
import lombok.ToString;

@Getter
@Setter
@ToString(callSuper = true)
public class WatchResponse extends AbstractResult {
    private String watchId;
}
