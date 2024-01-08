package com.ebay.goblin.client.model;

import com.ebay.goblin.client.model.common.ReadEntry;
import lombok.Getter;
import lombok.Setter;
import lombok.ToString;


@Getter
@Setter
@ToString(callSuper = true)
public class GetRequest extends ReadEntry {

    private boolean allowStale = false;
    private boolean metaReturn = false;

}
