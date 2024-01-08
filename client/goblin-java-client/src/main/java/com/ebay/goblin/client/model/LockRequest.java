package com.ebay.goblin.client.model;

import com.ebay.goblin.client.model.common.KeyType;
import com.ebay.goblin.client.model.common.ValueType;
import lombok.Getter;
import lombok.Setter;

@Getter
@Setter
public class LockRequest {
    KeyType key;
    ValueType value;
    Integer ttl;
}
