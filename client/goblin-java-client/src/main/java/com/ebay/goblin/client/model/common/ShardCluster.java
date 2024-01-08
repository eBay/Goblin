package com.ebay.goblin.client.model.common;

import lombok.Builder;
import lombok.Getter;
import lombok.Setter;
import lombok.ToString;

import java.util.Set;

@Getter
@Setter
@Builder
@ToString(callSuper = true)
public class ShardCluster {

    private Set<Long> shardInfo;

    private String cluster;

}
