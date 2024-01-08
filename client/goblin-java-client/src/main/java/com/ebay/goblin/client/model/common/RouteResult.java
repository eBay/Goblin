package com.ebay.goblin.client.model.common;

import lombok.Getter;
import lombok.Setter;
import lombok.ToString;

import java.util.List;

@Getter
@Setter
@ToString(callSuper = true)
public class RouteResult extends AbstractResult {

    private long routeVersion;

    private int shardFactor;

    private List<ShardCluster> shardClusters;

}
