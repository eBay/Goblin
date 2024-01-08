package com.ebay.goblin.client.model.common;

import goblin.proto.Service;
import lombok.Builder;
import lombok.Getter;
import lombok.Setter;
import lombok.ToString;

@Getter
@Setter
@Builder
@ToString(callSuper = true)
public class WatchEvent {
    Service.Watch.EventType eventType;
    String key;
    String value;
    String errorMsg;
    Long version;
}