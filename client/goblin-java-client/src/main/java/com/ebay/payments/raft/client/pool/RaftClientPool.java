package com.ebay.payments.raft.client.pool;

import com.ebay.payments.raft.client.RaftClientStub;

import java.util.List;

public interface RaftClientPool<Client extends RaftClientStub> {

    void init(List<Client> clientList);

    /**
     * @param index
     * @param cli
     * @return the older one
     */
    Client refresh(int index, Client cli);

    Client get(int index);

    int pick(boolean leader);

    void reportLeader(int index);

    void leaderHint(String hint);

    void markDown(int index, long downTimeInMs);

    int size();

    void shutdown();

    default int findIdxFromInstanceId(String id) {
        if (id == null) {
            return -1;
        }
        for (int i = 0; i < size(); ++i) {
            if (id.equalsIgnoreCase(get(i).getInstanceId())) {
                return i;
            }
        }
        return -1;
    }
}
