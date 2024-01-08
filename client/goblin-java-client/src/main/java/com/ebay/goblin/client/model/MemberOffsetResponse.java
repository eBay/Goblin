package com.ebay.goblin.client.model;

import com.ebay.goblin.client.model.common.AbstractResult;
import lombok.Getter;
import lombok.Setter;

import java.util.Map;
import java.util.TreeMap;

@Getter
@Setter
public class MemberOffsetResponse extends AbstractResult {
    private String leaderAddress;
    private Long leaderCommitIndex;
    private Map<String, Long> followerMatchedIndex = new TreeMap<>();

    public String toString() {
        StringBuilder info = new StringBuilder(String.format("leader: %s, offset : %d\n", leaderAddress, leaderCommitIndex));
        info.append("followers index:\n");
        for (String follower : followerMatchedIndex.keySet()) {
            info.append(String.format("%s: %d\n", follower, followerMatchedIndex.get(follower)));
        }
        return info.toString();
    }

}
