package com.ebay.goblin.client.api.impl.internal;

import com.ebay.goblin.client.api.LockService;
import com.ebay.goblin.client.api.impl.ShardingClientImpl;
import com.ebay.goblin.client.exceptions.GoblinConditionNotMetException;
import com.ebay.goblin.client.exceptions.GoblinException;
import com.ebay.goblin.client.exceptions.GoblinKeyNotExistException;
import com.ebay.goblin.client.model.*;
import com.ebay.goblin.client.model.common.ExistCondition;
import com.ebay.goblin.client.model.common.KeyType;
import com.ebay.goblin.client.model.common.ValueType;
import com.ebay.goblin.client.utils.LogUtils;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicReference;

public class LockServiceImpl implements LockService {

    private ShardingClientImpl client;

    private final ConcurrentHashMap<KeyType, LockTask> lockTaskMap = new ConcurrentHashMap<>();
    private final ExecutorService executor = Executors.newCachedThreadPool();
    private static final int DEFAULT_LOCK_TTL_IN_SEC = 20;


    public LockServiceImpl(ShardingClientImpl client) {
        this.client = client;
    }

    @Override
    public boolean lock(KeyType key, ValueType value, Integer timeout) throws GoblinException {
        PutRequest request = new PutRequest();
        request.setKey(key);
        request.setValue(value);
        request.setEnableTTL(true);
        request.setTtl(timeout == null ? DEFAULT_LOCK_TTL_IN_SEC : timeout);
        ExistCondition condition = ExistCondition.builder().key(key).existed(false).build();
        PutResponse response;
        LockTask lockTask = new LockTask(key, value, this.client);
        // TODO: clean up lockTaskMap if timeout == null
        LockTask preTask = lockTaskMap.putIfAbsent(key, lockTask);
        if (preTask != null) {
            if (TaskStatus.FAILED == preTask.getStatus()) {
                lockTaskMap.put(key, lockTask);
            } else if (timeout == null) {
                return false;
            }
        }
        try {
            response = client.put(request, condition);
        } catch (GoblinConditionNotMetException e) {
            if (timeout == null || preTask != null) {
                lockTaskMap.remove(key);
            }
            return false;
        } catch (GoblinException e) {
            lockTaskMap.remove(key);
            throw e;
        }
        lockTask.setVersion(response.getVersion());
        if (timeout == null) {
            // trigger extend lock task
            executor.submit(lockTask);
        } else {
            if (preTask != null) {
                lockTaskMap.put(key, lockTask);
            }
        }
        return true;
    }

    @Override
    public boolean unlock(KeyType key) throws GoblinException {
        LockTask lockTask = lockTaskMap.get(key);
        if (lockTask == null) {
            return false;
        }
        boolean result;
        try {
            result = lockTask.cancelTask();
        } finally {
            lockTaskMap.remove(key);
        }
        return result;
    }

    class LockTask implements Runnable {
        private static final int EXTEND_INTERVAL_IN_SEC = 15;
        private static final int EXTEND_TIME_IN_SEC = 20;
        private static final int RETRY_TIMES = 3;

        private final ShardingClientImpl client;
        private final KeyType key;
        private final ValueType value;

        private volatile long version;
        private AtomicReference<TaskStatus> status = new AtomicReference<>(TaskStatus.SUSPENDING);
        private volatile boolean isCanceled = false;

        public LockTask(KeyType key,
                        ValueType value,
                        ShardingClientImpl client) {
            this.key = key;
            this.value = value;
            this.client = client;
        }

        public TaskStatus getStatus() {
            return status.get();
        }

        public void setVersion(long version) {
            this.version = version;
        }

        @Override
        public void run() {
            boolean failed = false;
            try {
                while (!isCanceled && !failed) {
                    Thread.sleep(EXTEND_INTERVAL_IN_SEC * 1000);
                    failed = !extendLock();
                }
            } catch (Exception e) {
                LogUtils.error("failed to extend lock for key {} due to {}", key, e);
                failed = true;
            }
            if (failed) {
                status.set(TaskStatus.FAILED);
            }
            LogUtils.info("extend lock task finished for key {}", key);
        }

        private boolean extendLock() {
            return extendLock(RETRY_TIMES);
        }

        private boolean extendLock(int retryTimes) {
            if (retryTimes <= 0) {
                return false;
            }
            if (status.compareAndSet(TaskStatus.SUSPENDING, TaskStatus.RUNNING) && !isCanceled) {
                CasRequest request = new CasRequest();
                request.setKey(key);
                request.setValue(value);
                request.setComparedVersion(version);
                request.setEnableTTL(true);
                request.setTtl(EXTEND_TIME_IN_SEC);
                CasResponse response;
                try {
                    response = client.cas(request);
                    version = response.getVersion();
                } catch (GoblinConditionNotMetException e) {
                    return false;
                } catch (GoblinException e) {
                    LogUtils.error("try extend lock failed for key {} due to {}, remaining retry times {}",
                            key, e, retryTimes);
                    status.compareAndSet(TaskStatus.RUNNING, TaskStatus.SUSPENDING);
                    return extendLock(--retryTimes);
                } finally {
                    status.compareAndSet(TaskStatus.RUNNING, TaskStatus.SUSPENDING);
                }
                if (isCanceled) {
                    DeleteRequest deleteRequest = new DeleteRequest();
                    deleteRequest.setKey(key);
                    deleteRequest.setVersion(this.version);
                    try {
                        client.delete(deleteRequest);
                    } catch (GoblinConditionNotMetException ignored) {
                    }
                }
            }
            return true;
        }

        public boolean cancelTask() throws GoblinException {
            this.isCanceled = true;
            if (status.compareAndSet(TaskStatus.SUSPENDING, TaskStatus.CANCELING)) {
                DeleteRequest request = new DeleteRequest();
                request.setKey(key);
                request.setVersion(this.version);
                try {
                    client.delete(request);
                    return true;
                } catch (GoblinConditionNotMetException | GoblinKeyNotExistException e) {
                    return false;
                } finally {
                    status.compareAndSet(TaskStatus.CANCELING, TaskStatus.SUSPENDING);
                }
            }
            return false;
        }
    }

    public enum TaskStatus {
        SUSPENDING, RUNNING, FAILED, CANCELING
    }

}
