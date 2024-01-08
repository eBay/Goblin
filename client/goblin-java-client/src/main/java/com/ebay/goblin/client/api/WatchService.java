package com.ebay.goblin.client.api;


import com.ebay.goblin.client.api.impl.internal.WatchServiceImpl;
import com.ebay.goblin.client.exceptions.GoblinException;

import com.ebay.goblin.client.model.common.ReadEntry;
import com.ebay.goblin.client.model.common.WatchCallback;

import java.util.List;

public interface WatchService extends AutoCloseable {

    // server will send heartbeat every 30 seconds
    // this value is check the heartbeat, so typically 2 times of the heartbeat interval from server
    long HEARTBEAT_INTERVAL_IN_SEC = 60;

    void submitWatchTask(
            String id,
            List<ReadEntry> entries,
            WatchCallback cb,
            WatchStream stream) throws GoblinException;

    void cancelWatchTask(String watchId) throws GoblinException;

    static WatchService newInstance() {
        return new WatchServiceImpl();
    }
}
