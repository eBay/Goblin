# Demo App
This section will illustrate how to set up a demo Java application using Goblin.
- [Maven Dependency](#maven-dependency)
- [Create the Client Configuration](#create-the-client-configuration)
- [Initialize the GoblinClient](#initialize-the-goblinclient)

# Maven Dependency
Use the latest release of goblin-java-client.
```xml
<dependency>
    <groupId>com.ebay.magellan</groupId>
    <artifactId>goblin-java-client</artifactId>
    <version>3.5.0-RELEASE</version>
</dependency>
```

# Create the Client Configuration
```java
    RaftClusterClientConfig config = RaftClusterClientConfig.builder()
        .clusterInfo(clusterAddresses)
        .timeoutInMilliSeconds(timeoutInMilliSeconds)
        .stubTimeoutInMilliSeconds(stubTimeoutInMilliSeconds)
        .tlsEnabled(grpcTlsEnable)
        .clusterInfoResolver(new TessClusterInfoResolver())
        .poolType(poolType)
        .build();
```

| Key                       | Value                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
|---------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| timeoutInMilliSeconds     | The overall timeout (in milliseconds) of a _RaftClusterClient_ method call<br>_RaftClusterClient_ ensures that a method call throws _RaftInvokeTimeoutException_ exception shortly after timeoutInMilliSeconds milliseconds when the method was not completed.<br/>Default value: 10000                                                                                                                                                                                     |
| stubTimeoutInMilliSeconds | The gPRC timeout (in milliseconds) of a single API call between the client and Goblin server.<br>Change this value only when the connection between the server and client is very unstable.<br>In most cases, set #timeoutInMilliSeconds# for the overall timeout configuration.<br/>Default value: 500                                                                                                                                                                     |
| clusterInfo               | Goblin servers' addresses.<br>The format is:<br>"<node_id_1>@<host1>:<port1>,<node_id_2>@<host2>:<port2>,<node_id_3>@<host3>:<port3>,..."<br>e.g. experimental dev goblin<br>"1@om-expt-dev-fss-1.goblin-dev.stratus.qa.ebay.com:50055,2@om-expt-dev-fss-2.goblin-dev.stratus.qa.ebay.com:50055,3@om-expt-dev-fss-3.goblin-dev.stratus.qa.ebay.com:50055,4@om-expt-dev-fss-4.goblin-dev.stratus.qa.ebay.com:50055,5@om-expt-dev-fss-5.goblin-dev.stratus.qa.ebay.com:50055" |
| clusterInfoResolver       | The implementation of _ClusterInfoResolver_ interface. It is used to resolve dns of the clusterInfo to a list of Goblin servers' ip addresses.                                                                                                                                                                                                                                                                                                                              |
| tlsEnabled                | _**true**_: enable TLS in the connection between the client and Goblin server.<br>_**false**_: disable TLS in the connection between the client and Goblin server.                                                                                                                                                                                                                                                                                                          |
| sslContext                | io.grpc.netty.shaded.io.netty.handler.ssl.SslContext object for TLS connection. Mandatory if tlsEnabled is **_true_**.                                                                                                                                                                                                                                                                                                                                                      |
| poolType                  | The configuration of read mode.<br/>`LeaderOnly`: always found leader to connect.<br/>`RoundRobin`: round robin to pick a client.<br/>`InSyncReplica`: round robin to pick a client but it should be closed to leader enough. In-sync means the gap of the committed index between follower and leader is less than a pre-defined threshold. If you want strong consistency read, use `LeaderOnly` <br/>Default value: `LeaderOnly`                                         |                                  
| ISRValidMinGap            | The threshold of the gap of committed index between follower and leader. <br>If the real gap is less than this threshold, the follower is considered as ISR follower, and can serve read traffic, otherwise the follower can't serve read traffic. <br>It takes effective only if poolType is configured as `InSyncReplica`.<br/>Default value: 100                                                                                                                         |
| ISRRefreshTime            | Time interval to refresh ISR list. <br/>Default value: 120000 (2min)                                                                                                                                                                                                                                                                                                                                                                                                        |
| ISRLeaseTime              | Lease for an ISR List becomes invalid. <br/>Default value: 600000 (10min)                                                                                                                                                                                                                                                                                                                                                                                                   |
| intervalPerRound          | The backoff delay between gRPC call retry attempts <br/>Default value: 5(ms)                                                                                                                                                                                                                                                                                                                                                                                                |

# Initialize the GoblinClient
_GoblinClient_ is **thread safe**. **Only one _GoblinClient_ is needed** for all API calls with a specific Goblin cluster. For the complete documents of Goblin APIs, please refer to [Client API Reference for Java](client_api_reference.md)
```java
    RaftClusterClientConfig config = RaftClusterClientConfig.builder()
        .clusterInfo("1@om-expt-dev-fss-1.goblin-dev.stratus.qa.ebay.com:50055,2@om-expt-dev-fss-2.goblin-dev.stratus.qa.ebay.com:50055,3@om-expt-dev-fss-3.goblin-dev.stratus.qa.ebay.com:50055,4@om-expt-dev-fss-4.goblin-dev.stratus.qa.ebay.com:50055,5@om-expt-dev-fss-5.goblin-dev.stratus.qa.ebay.com:50055")
        .timeoutInMilliSeconds(10000L)
        .tlsEnabled(false)
        .poolType(PoolType.LeaderOnly)
        .build();

    // build a Goblin client
    GoblinClient goblinClient = null;
    try {
        // build the client
        goblinClient = builder.build();
    } catch (GoblinException e) {
        // handle the exception here.
    } finally {
        if(goblinClient != null) {
            goblinClient.close();
        }
    }
```
_GoblinClient_ implements the _AutoCloseable_ interface. So the above sample could also be
```java
try (GoblinClient client = builder.build()) {
    // call APIs
    // ... ...
} catch (GoblinException e) {
    // handle the exception here.
}
```