package com.ebay.payments.raft.client.pool.impl;

import com.ebay.goblin.client.utils.CommonUtils;
import com.ebay.payments.raft.client.RaftClientStub;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;
import org.apache.commons.lang3.StringUtils;

public class RoundRobinRaftClientPool<Client extends RaftClientStub> extends LeaderOnlyClientPool<Client> {

    private final List<Long> unavailableUntil = new CopyOnWriteArrayList<>();
    private volatile int roundRobinIndex = 0;

    @Override
    public void init(List<Client> clients) {
        super.init(clients);
        unavailableUntil.clear();
        for (int i = 0; i < size(); ++i) {
            unavailableUntil.add(-1L);
        }
    }

    @Override
    public Client refresh(int index, Client cli) {
        if (index >= size()) {
            return null;
        }
        unavailableUntil.set(index, -1L);
        return super.refresh(index, cli);
    }

    @Override
    public int pick(boolean leader) {
        if (leader) {
            return super.pick(true);
        } else {
            return roundRobin();
        }
    }

    @Override
    public void markDown(int index, long downTimeInMs) {
        super.markDown(index, downTimeInMs);
        unavailableUntil.set(index, System.currentTimeMillis() + downTimeInMs);
    }

    private synchronized int roundRobin() {
        // prioritize client in the same dc to connect
        String dcName = CommonUtils.getDataCenterName();
        for (int i = 1; i <= size(); ++i) {
            int idx = (roundRobinIndex + i) % size();
            if ((unavailableUntil.get(idx) != -1 && System.currentTimeMillis() < unavailableUntil.get(idx))
                    || !(StringUtils.isNotBlank(super.get(idx).getDomainName()) && super.get(idx).getDomainName().contains(dcName))) {
                continue;
            }
            unavailableUntil.set(idx, -1L);
            roundRobinIndex = idx;
            return idx;
        }

        // random client
        for (int i = 1; i <= size(); ++i) {
            int idx = (roundRobinIndex + i) % size();
            // idx is unavailable
            if (unavailableUntil.get(idx) != -1 && System.currentTimeMillis() < unavailableUntil.get(idx)) {
                continue;
            }
            unavailableUntil.set(idx, -1L);
            roundRobinIndex = idx;
            return idx;
        }
        // if all clients is unavailable, back to 0
        return 0;
    }
}
