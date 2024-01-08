package com.ebay.goblin.client.api.impl.internal;

import com.ebay.goblin.client.api.WatchService;
import com.ebay.goblin.client.api.WatchStream;
import com.ebay.goblin.client.exceptions.GoblinException;
import com.ebay.goblin.client.model.common.ReadEntry;
import com.ebay.goblin.client.model.common.WatchCallback;
import com.ebay.goblin.client.model.common.WatchEvent;
import com.ebay.goblin.client.utils.LogUtils;
import com.google.common.collect.Lists;
import com.google.common.collect.Maps;
import goblin.proto.Service;

import java.util.*;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class WatchServiceImpl implements WatchService {

    private final ExecutorService executor = Executors.newCachedThreadPool();
    private final Map<String, WatchTask> watchId2Task = Maps.newConcurrentMap();

    @Override
    public void submitWatchTask(
            String id,
            List<ReadEntry> entries,
            WatchCallback cb,
            WatchStream stream) throws GoblinException {
        if (watchId2Task.get(id) != null) {
            throw new GoblinException("invalid task id for watch task " + id);
        }
        WatchTask task = new WatchTask(id, entries, cb, stream);
        executor.submit(task);
        watchId2Task.put(id, task);
    }

    @Override
    public void cancelWatchTask(String watchId) throws GoblinException {
        WatchTask cancelTask = watchId2Task.get(watchId);
        if (cancelTask == null) {
            throw new GoblinException("invalid task id for cancel task " + watchId);
        }
        cancelTask.cancel(watchId);
    }

    @Override
    public void close() throws Exception {
        for (Map.Entry<String, WatchTask> entry : watchId2Task.entrySet()) {
            entry.getValue().cancel(entry.getKey());
        }
    }

    class WatchTask implements Runnable {
        private long MAX_PULL_MESSAGE_INTERVAL_IN_MILL_SEC = 100;
        private final String watchId;
        private final List<ReadEntry> watchEntries;
        private final WatchCallback watchCallback;
        private final WatchStream watchStream;
        private long nextHeartBeatDeadline = 0;
        private volatile boolean isCanceled = false;

        public WatchTask(String id, List<ReadEntry> entries, WatchCallback cb, WatchStream stream) {
            watchId = id;
            watchEntries = entries;
            watchCallback = cb;
            watchStream = stream;
        }

        @Override
        public void run() {
            try {
                nextHeartBeatDeadline = System.currentTimeMillis() + HEARTBEAT_INTERVAL_IN_SEC * 1000;
                while (!isCanceled && !watchStream.isShutDown()) {
                    Service.Watch.Response resp = watchStream.pullNextResponse(MAX_PULL_MESSAGE_INTERVAL_IN_MILL_SEC);
                    long curTime = System.currentTimeMillis();
                    if (curTime > nextHeartBeatDeadline) {
                        LogUtils.error("no heartbeat from server, shutdown the stream");
                        cancel(watchId);
                        break;
                    }
                    if (resp == null) {
                        continue;
                    }
                    if (!resp.getWatchId().equals(watchId)) {
                        LogUtils.error("not a right watch id, expected: {}, got: {}", watchId, resp.getWatchId());
                        break;
                    }
                    nextHeartBeatDeadline = System.currentTimeMillis() + HEARTBEAT_INTERVAL_IN_SEC * 1000;
                    List<WatchEvent> events = Lists.newArrayList();
                    for (Service.Watch.Change change : resp.getChangesList()) {
                        if (change.getEventType() != Service.Watch.EventType.HEARTBEAT) {
                            // ignore heartbeat event which is used for server to check if the client is alive
                            events.add(WatchEvent.builder()
                                    .eventType(change.getEventType())
                                    .key(change.getKey().toStringUtf8())
                                    .value(change.getValue().toStringUtf8())
                                    .version(change.getVersion())
                                    .build());
                        }
                    }
                    if (!events.isEmpty()) {
                        watchCallback.notify(watchId, events);
                    }
                }
            } catch (Exception e) {
                LogUtils.error("failed to notify change for watch {} due to {}", watchId, e);
            }
            if (watchStream.getError() != null) {
                watchCallback.notify(watchId,
                        Collections.singletonList(WatchEvent.builder()
                                .eventType(Service.Watch.EventType.DISCONNECTED)
                                .errorMsg(watchStream.getError().getMessage())
                                .build()));
            }
        }

        public synchronized void cancel(String watchId) {
            if (!isCanceled) {
                watchStream.cancel(watchId);
                isCanceled = true;
                LogUtils.info("watch id {} is canceled successfully", watchId);
            } else {
                LogUtils.info("watch id {} is already canceled", watchId);
            }

        }
    }
}
