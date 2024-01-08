package com.ebay.goblin.client.model.common;

import java.util.List;

@FunctionalInterface
public interface WatchCallback {
    void notify(String watchId, List<WatchEvent> events);
}