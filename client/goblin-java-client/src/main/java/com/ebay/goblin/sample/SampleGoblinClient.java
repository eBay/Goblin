package com.ebay.goblin.sample;

import com.ebay.goblin.client.api.GoblinClient;
import com.ebay.goblin.client.exceptions.GoblinConditionNotMetException;
import com.ebay.goblin.client.model.CasResponse;
import com.ebay.goblin.client.model.DeleteResponse;
import com.ebay.goblin.client.model.GetResponse;
import com.ebay.goblin.client.model.PutResponse;
import com.ebay.goblin.client.model.common.KeyType;
import com.ebay.goblin.client.model.common.ValueType;
import com.ebay.payments.raft.client.RaftClusterClientConfig;
import io.grpc.netty.shaded.io.grpc.netty.GrpcSslContexts;
import io.netty.handler.ssl.util.InsecureTrustManagerFactory;
import javax.net.ssl.SSLException;
import org.apache.log4j.BasicConfigurator;

public class SampleGoblinClient {

    public static void main(String[] args) throws SSLException {
        BasicConfigurator.configure();
        /**
         * The format of clusterInfo is
         * "<node_id_1>@<host1>:<port1>,<node_id_2>@<host2>:<port2>,<node_id_3>@<host3>:<port3>,..."
         */
        RaftClusterClientConfig config = RaftClusterClientConfig.builder()
                .clusterInfo("1@0.0.0.0:50055,2@0.0.0.0:50056,3@0.0.0.0:50057")
              //.tlsEnabled(true)
              //.sslContext(GrpcSslContexts.forClient().trustManager(InsecureTrustManagerFactory.INSTANCE).build())
                .timeoutInMilliSeconds(10000L)
                .stubTimeoutInMilliSeconds(10000L)
                .build();
        try (GoblinClient client = GoblinClient.newInstance(config)) {
            samplePut(client);
            samplePutWithTTL(client);
            sampleGet(client);
            sampleCas(client);
            sampleDelete(client);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private static void samplePut(GoblinClient client) {
        KeyType key = KeyType.builder().key("sampleKey").build();
        byte[] content = new byte[]{1, 2, 3, 4, 5};
        ValueType valueAll = ValueType.builder().content(content).build();
        PutResponse putResponse = null;

        /**
         * put the whole value
         */
        putResponse = client.put(key, valueAll);

        /**
         * put partial value
         */
        ValueType valuePartial = ValueType.builder()
                .content(content).offset(1).size(3)
                .build();
        putResponse = client.put(key, valuePartial);
    }

    private static void samplePutWithTTL(GoblinClient client) {
        KeyType key = KeyType.builder().key("sampleKey1").build();
        ValueType value1 = ValueType.builder().content(new byte[]{1, 2, 3}).build();
        ValueType value2 = ValueType.builder().content(new byte[]{1, 2, 3, 4}).build();
        int ttl = 90;
        PutResponse putResponse = null;

        /**
         * put the with ttl for the first time
         */
        putResponse = client.putWithTTL(key, value1, ttl);

        /**
         * extend the ttl with the same duration
         */
        putResponse = client.putWithTTL(key, value2);
    }

    private static void sampleGet(GoblinClient client) {
        KeyType key = KeyType.builder().key("sampleKey").build();
        GetResponse getResponse = null;

        /**
         * Get the value with the latest version
         */
        getResponse = client.get(key);
        if (getResponse.getValue().isPresent()) {
            byte[] content = getResponse.getValue().get().getContent();
        } else {
            System.out.println("The key does not exist.");
        }

        /**
         * Get the value with a specific version
         */
        getResponse = client.get(key, getResponse.getVersion());
    }

    private static void sampleCas(GoblinClient client) {
        KeyType key = KeyType.builder().key("sampleKey").build();
        ValueType newValue = ValueType.builder().content(new byte[]{1, 2, 3}).build();
        Long expectedVersion = client.get(key).getVersion();

        CasResponse casResponse = null;

        try {
            /**
             * CAS operation
             */
            casResponse = client.cas(key, newValue, expectedVersion);

            /**
             * CAS with TTL is also supported
             */
            casResponse = client.casWithTTL(key, newValue, casResponse.getVersion(), 90);
        } catch (GoblinConditionNotMetException e) {
            System.err.println("Version conflict.");
        }
    }

    private static void sampleDelete(GoblinClient client) {
        KeyType key = KeyType.builder().key("sampleKey").build();
        byte[] content = new byte[]{1, 2, 3, 4, 5};
        ValueType value = ValueType.builder().content(content).build();
        client.put(key, value);

        DeleteResponse deleteResponse = null;

        /**
         * Delete a key and return the deleted value
         */
        deleteResponse = client.delete(key);

        /**
         * Delete a key for a specific version and don't return the deleted value
         */
        PutResponse putResponse = client.put(key, value);
        deleteResponse = client.delete(key, putResponse.getVersion(), false);
    }
}
