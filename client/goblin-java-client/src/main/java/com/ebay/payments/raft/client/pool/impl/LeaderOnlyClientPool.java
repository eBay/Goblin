package com.ebay.payments.raft.client.pool.impl;

import com.ebay.goblin.client.utils.LogUtils;
import com.ebay.payments.raft.client.RaftClientStub;
import com.ebay.payments.raft.client.pool.RaftClientPool;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;

public class LeaderOnlyClientPool<Client extends RaftClientStub> implements RaftClientPool<Client> {
    private final List<Client> grpcClients = new CopyOnWriteArrayList<>();
    private volatile int leaderIndex = 0;

    @Override
    public void init(List<Client> clients) {
        grpcClients.clear();
        grpcClients.addAll(clients);
    }

    @Override
    public Client refresh(int index, Client cli) {
        if (index >= grpcClients.size()) {
            return null;
        }
        return grpcClients.set(index, cli);
    }

    @Override
    public Client get(int index) {
        return grpcClients.get(index);
    }

    @Override
    public int pick(boolean leader) {
        if (!leader) {
            throw new RuntimeException("Select leader client pool always pick leader");
        }
        return leaderIndex;
    }

    @Override
    public void reportLeader(int index) {
        if (index >=0 && index < size()) {
            leaderIndex = index;
        }
    }

    @Override
    public void leaderHint(String hint) {
        int idx = findIdxFromInstanceId(hint);
        if (idx >= 0) {
            reportLeader(idx);
        }
    }

    @Override
    public void markDown(int index, long downTimeInMs) {
        synchronized (this) {
            if (leaderIndex == index) {
                // it means service is down, try other one to find a leader
                leaderIndex = (index + 1) % size();
            }
        }
    }

    @Override
    public int size() {
        return grpcClients.size();
    }

    @Override
    public void shutdown() {
        for (Client grpcClient : grpcClients) {
            grpcClient.close();
            LogUtils.info("GrpcClient {} has been shutdown successfully!", grpcClient);
        }
        grpcClients.clear();
    }
}
