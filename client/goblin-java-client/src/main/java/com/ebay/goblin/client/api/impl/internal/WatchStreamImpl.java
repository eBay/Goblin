package com.ebay.goblin.client.api.impl.internal;

import com.ebay.goblin.client.api.WatchStream;
import com.ebay.goblin.client.exceptions.GoblinException;
import com.ebay.goblin.client.utils.LogUtils;
import goblin.proto.Service;
import io.grpc.stub.StreamObserver;

import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

public class WatchStreamImpl implements WatchStream {

    static class BlockingStream implements StreamObserver<Service.Watch.Response> {

        private final int MAX_STREAM_RESPONSE_BUFFER_NUM = 10;
        private final int MAX_OFFER_BLOCKED_TIMEOUT_IN_MILL_SEC = 100;
        private final BlockingQueue<Service.Watch.Response> blockingDeque = new LinkedBlockingQueue<>(MAX_STREAM_RESPONSE_BUFFER_NUM);
        private volatile boolean isShutdown = false;
        private volatile Throwable error = null;

        @Override
        public void onNext(Service.Watch.Response response) {
            try {
                boolean succeed = false;
                while (!isShutdown && !succeed) {
                    succeed = blockingDeque.offer(response, MAX_OFFER_BLOCKED_TIMEOUT_IN_MILL_SEC, TimeUnit.MILLISECONDS);
                }
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }

        @Override
        public void onError(Throwable throwable) {
            isShutdown = true;
            error = throwable;
            LogUtils.error("error on blocking watch stream", error);
        }

        @Override
        public void onCompleted() {
            isShutdown = true;
            LogUtils.info("completed on blocking watch stream");
        }

        public void shutdown() {
            isShutdown = true;
        }

        public Throwable getError() {
            return error;
        }

        public boolean isShutdown() {
            return isShutdown;
        }
    }

    private final StreamObserver<Service.Watch.Request> requestStreamObserver;
    private final BlockingStream responseStreamObserver;

    public WatchStreamImpl(StreamObserver<Service.Watch.Request> requestStream, BlockingStream responseStream) {
        requestStreamObserver = requestStream;
        responseStreamObserver = responseStream;
    }

    @Override
    public Service.Watch.Response pullNextResponse(long timeoutInMillSec) throws GoblinException {
        try {
            return responseStreamObserver.blockingDeque.poll(timeoutInMillSec, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            throw new GoblinException(e.getMessage());
        }
    }

    @Override
    public Service.Watch.Response peekNextResponse(long timeoutInMillSec) throws GoblinException {
        try {
            long deadline = System.currentTimeMillis() + timeoutInMillSec;
            Service.Watch.Response resp = null;
            while (System.currentTimeMillis() < deadline) {
                resp = responseStreamObserver.blockingDeque.peek();
                if (resp != null) {
                    break;
                } else {
                    Thread.sleep(50);
                }
            }
            return resp;
        } catch (InterruptedException e) {
            throw new GoblinException(e.getMessage());
        }
    }

    @Override
    public void pushNewRequest(Service.Watch.Request request) {
        requestStreamObserver.onNext(request);
    }

    @Override
    public void cancel(String watchId) {
        // cancel the response stream to stop receiving responses
        responseStreamObserver.shutdown();
        // send a request to notify server to cancel this watch
        Service.Watch.CancelRequest.Builder builder = Service.Watch.CancelRequest.newBuilder().setWatchId(watchId);
        Service.Watch.Request request = Service.Watch.Request.newBuilder().setCancelReq(builder).build();
        pushNewRequest(request);
        // complete the client stream so that server will send response to shutdown the response stream
        requestStreamObserver.onCompleted();
    }

    @Override
    public Throwable getError() {
        return responseStreamObserver.getError();
    }

    @Override
    public boolean isShutDown() {
        return responseStreamObserver.isShutdown();
    }

    @Override
    public void close() throws Exception {
        // cancel the response stream to stop receiving responses
        responseStreamObserver.shutdown();
        // complete the client stream so that server will send response to shutdown the response stream
        requestStreamObserver.onCompleted();
    }

}
