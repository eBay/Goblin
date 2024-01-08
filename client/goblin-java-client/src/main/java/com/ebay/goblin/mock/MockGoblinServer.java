package com.ebay.goblin.mock;

import com.ebay.goblin.client.utils.LogUtils;
import io.grpc.Server;
import io.grpc.ServerBuilder;
import java.io.IOException;
import java.util.concurrent.TimeUnit;
import org.apache.log4j.BasicConfigurator;

public class MockGoblinServer {

    private Server server;

    public static final String KEY_TIMEOUT = "$KeyTimeout$";

    public void start(int port) throws IOException {
        server = ServerBuilder.forPort(port)
                .addService(new MockGoblinServerImpl())
                .build()
                .start();
        LogUtils.info("Server started, listening on " + port);
    }

    public synchronized void stop() throws InterruptedException {
        if (server != null) {
            LogUtils.info("Shutting down gRPC server");
            server.shutdown().awaitTermination(30, TimeUnit.SECONDS);
            server = null;
        }
    }

    /**
     * Await termination on the main thread since the grpc library uses daemon threads.
     */
    public void blockUntilShutdown() throws InterruptedException {
        if (server != null) {
            server.awaitTermination();
        }
    }

    /**
     * Main launches the server from the command line.
     */
    public static void main(String[] args) throws IOException, InterruptedException {
        BasicConfigurator.configure();
        final MockGoblinServer server = new MockGoblinServer();
        server.start(50052);
        server.blockUntilShutdown();
    }

}
