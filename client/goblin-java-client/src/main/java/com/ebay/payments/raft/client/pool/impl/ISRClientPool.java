package com.ebay.payments.raft.client.pool.impl;

import com.ebay.goblin.client.model.MemberOffsetResponse;
import com.ebay.goblin.client.utils.LogUtils;
import com.ebay.payments.raft.client.NetAdminStub;
import com.ebay.payments.raft.client.RaftClientStub;
import com.ebay.payments.raft.client.pool.RaftClientPool;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.locks.ReadWriteLock;
import java.util.concurrent.locks.ReentrantReadWriteLock;

public class ISRClientPool<Client extends RaftClientStub> implements RaftClientPool<Client> {

    private final static int RETRY_TIME = 5;
    private final RaftClientPool<Client> inner;
    private final long minISRGap;
    private final Thread updateTread;
    private final AtomicBoolean running;
    // only followers will be checked in ISR list;
    private Set<Integer> ISRList = new HashSet<>();
    private final ReadWriteLock ISRLock = new ReentrantReadWriteLock();
    private final long leaseTime;
    private final long updateInterval;
    private long lastUpdate;

    public ISRClientPool(RaftClientPool<Client> inner, long minISRGap, long leaseTime, long updateInterval) {
        this.inner = inner;
        running = new AtomicBoolean(true);
        LogUtils.info("min gap is {}, refresh time is {}, lease time is {}", minISRGap, updateInterval, leaseTime);
        this.minISRGap = minISRGap;
        this.updateInterval = updateInterval;
        this.leaseTime = leaseTime;
        this.lastUpdate = 0;
        updateTread = new Thread(this::updateISRList);
    }

    private String instanceIdOf(String address) {
        return address.split("@")[0];
    }

    private void updateISRList() {
        while (running.get()) {
            boolean updated = false;
            Set<Integer> tmpList = new HashSet<>();
            int retryTimes = 0;
            int leaderIdx = 0;
            // update ISR List
            while (retryTimes <= RETRY_TIME) {
                try {
                    // try pick leader
                    leaderIdx = pick(true);
                    Client cli = inner.get(leaderIdx);
                    NetAdminStub netAdminStub = cli.getNetAdmin();
                    NetAdminStub.Result<MemberOffsetResponse> memberOffsetResult = netAdminStub.getMemberOffset();
                    if (memberOffsetResult.getCode() == 200 && memberOffsetResult.getRes() != null) {
                        LogUtils.info("get member offset from leader, try to update IRS list");
                        MemberOffsetResponse response = memberOffsetResult.getRes();
                        long leaderOffset = response.getLeaderCommitIndex();
                        LogUtils.info("try-{}: {}@{} returns member offset\n{}", retryTimes, cli.getInstanceId(), cli.getHost(), response);
                        leaderIdx = findIdxFromInstanceId(instanceIdOf(response.getLeaderAddress()));
                        reportLeader(leaderIdx);
                        for (Map.Entry<String, Long> entry : response.getFollowerMatchedIndex().entrySet()) {
                            long offset = entry.getValue();
                            if (leaderOffset - offset <= minISRGap) {
                                int idx = findIdxFromInstanceId(instanceIdOf(entry.getKey()));
                                tmpList.add(idx);
                            }
                        }
                        updated = true;
                        break;
                    } else {
                        LogUtils.error("try-{}: {}@{} code is {}, message {}, it report {} is leader",
                                retryTimes, cli.getInstanceId(), cli.getHost(),
                                memberOffsetResult.getCode(), memberOffsetResult.getMessage(),
                                memberOffsetResult.getLeaderHint());
                        if (memberOffsetResult.getLeaderHint() != null) {
                            int leaderHintIdx = findIdxFromInstanceId(instanceIdOf(memberOffsetResult.getLeaderHint()));
                            if (leaderHintIdx >= 0) {
                                reportLeader(leaderHintIdx);
                                // find leader hint
                                continue;
                            }
                        }
                    }
                } catch (Throwable anyError) {
                    LogUtils.error("try-{}: meet error {} from idx: {}", retryTimes, anyError.getMessage(), leaderIdx);
                }
                // otherwise, try other client
                reportLeader((leaderIdx + 1) % size());
                retryTimes++;
            }
            if (updated) {
                // swap pointer
                ISRLock.writeLock().lock();
                ISRList = tmpList;
                ISRLock.writeLock().unlock();
                lastUpdate = System.currentTimeMillis();
                LogUtils.info("update IRSList to {}", printISRList(ISRList));
            }
            // check lease
            if (System.currentTimeMillis() > lastUpdate + leaseTime) {
                // lease time out, remove all IRS
                ISRLock.writeLock().lock();
                ISRList.clear();
                ISRLock.writeLock().unlock();
                LogUtils.warn("lease time out, but no ISR update successfully, it will make no IRS followers");
            }
            try {
                Thread.sleep(updateInterval);
            } catch (InterruptedException e) {
                LogUtils.warn(e.getMessage());
                e.printStackTrace();
            }
        }

    }

    private String printISRList(Set<Integer> list) {
        StringBuilder sb = new StringBuilder("[");
        for (Integer i : list) {
            sb.append(inner.get(i).getInstanceId()).append("@").append(inner.get(i).getHost());
            sb.append(", ");
        }
        sb.append("]");
        return sb.toString();
    }

    @Override
    public void init(List<Client> clients) {
        inner.init(clients);
        updateTread.start();
    }

    @Override
    public Client refresh(int index, Client cli) {
        return inner.refresh(index, cli);
    }

    @Override
    public Client get(int index) {
        return inner.get(index);
    }

    @Override
    public int pick(boolean leader) {
        if (!leader) {
            // ISR only effect when allow pick a follower
            int leaderIdx = inner.pick(true);
            for (int retry = 0; retry < inner.size(); ++retry) {
                int pick = inner.pick(false);
                ISRLock.readLock().lock();
                boolean exist = ISRList.contains(pick);
                ISRLock.readLock().unlock();
                if (exist || pick == leaderIdx) {
                    return pick;
                } else {
                    inner.markDown(pick, 1000);
                }
            }
        }
        // case 1 leader is true, just pick a leader
        // case 2, failed find an ISR follower, just pick a leader
        return inner.pick(leader);
    }

    @Override
    public void reportLeader(int index) {
        inner.reportLeader(index);
    }

    @Override
    public void leaderHint(String hint) {
        inner.leaderHint(hint);
    }

    @Override
    public void markDown(int index, long downTimeInMs) {
        inner.markDown(index, downTimeInMs);
    }

    @Override
    public int size() {
        return inner.size();
    }

    @Override
    public void shutdown() {
        running.set(false);
        updateTread.interrupt();
        try {
            updateTread.join();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        inner.shutdown();
    }
}
