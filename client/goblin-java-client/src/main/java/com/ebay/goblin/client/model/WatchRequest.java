package com.ebay.goblin.client.model;

import com.ebay.goblin.client.model.common.ReadEntry;
import com.ebay.goblin.client.model.common.WatchCallback;
import lombok.Getter;
import lombok.Setter;
import lombok.ToString;
import java.util.List;

@Getter
@Setter
@ToString(callSuper = true)
public class WatchRequest {
    List<ReadEntry> watchEntries;
    WatchCallback watchCallback;
    boolean retrieveOnConnected = false;
}
