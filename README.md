
# Introduction
Goblin is an infrastructure that provides flexible ways of processing and storing key-value patterns of data, where keys and values are arbitrary byte streams.

Goblin is built on top of [Gringofts](https://github.com/eBay/Gringofts), an open-source project which provides Raft consensus for replication. It aims to provide key-value storing service with strong consistency, high reliability, high availability, scalability and performance.

# Functional Features
1. Support [basic kv operations (put/get/delete)](./doc/kv.md)
2. Support [CAS (compare-and-swap) operation](./doc/cas.md)
3. Support [key expiration mechanism](./doc/key_expiration.md)
4. Support [distributed lock](./doc/distributed_lock.md)
5. Support [transaction](./doc/transaction.md)
6. Support [sharding](./doc/sharding.md)
7. Support [UDM (user-define meta)](./doc/udm.md)
8. Support [ISR (In-sync Replica Read)](./doc/isr.md)

# Non-functional Features
## High availability
1. Goblin is built on top of Raft consensus. It can tolerate the failure of minority nodes.
2. Goblin can be deployed in 3 data centers with 5 nodes, and it can tolerate one data center outage.

## Consistency
1. Goblin provides strong consistency for writes since only the leader processes requests. And for multiple threads in the leader, Goblin uses lock to the same key to avoid race condition.
2. For reads, Goblin provides both strong consistency and eventual consistency. Strong consistency is provided by the leader, and eventual consistency is provided by the follower.

## High reliability
1. Goblin provides high reliability by replicating data to multiple nodes and can guarantee no data loss.
2. Goblin replies to Clients only after the data is persisted to the disk and replicated to the majority nodes.
3. So if Goblin tells you that your request is processed, then you can be sure that your data is safe.

## High performance
Goblin provide stable, sustained high performance. Two factors define performance: latency and throughput. Latency is the time taken to complete an operation. Throughput is the total operations completed within some time period.

Here are the rough performance metrics based on the 1KB payload size for one cluster. For detailed metrics, please see [Benchmark](./doc/BENCHMARK.md).

Furthermore, all the performance data for Goblin provided here is based on testing a single cluster. It is important to emphasize that Goblin supports [sharding](./doc/sharding.md), allowing for horizontal scaling by deploying multiple clusters to achieve greater throughput performance. With the provision of additional machine resources, achieving a million-level TPS (transactions per second) is also feasible.

### Throughput for Single Cluster
| Operation | Max TPS/QPS                    |
| --- |--------------------------------|
| Write | 23k                            |
| Read | 26k for one node               |
| Read | 100k+ for 5 nodes in a cluster |

### Latency
The total latency from client side will be affected by client-server latency, thus following latency statistics are collected from server side without considering client-server latency.
#### Server Deployment in One DataCenter
| Operation | P99 Latency| P50 Latency |
| --------- | --------- |--------------|
| Write | 21ms | 8ms |
| Leader Read | < 1ms | < 1ms|
| Follower Read | < 1ms | < 1ms|

#### Server Deployment in Distributed DataCenters
| Operation | P99 Latency | P50 Latency|
| --------- | --------- |--------------|
| Write | 26ms | 16ms |
| Leader Read | < 1ms | < 1ms|
| Follower Read | < 1ms | < 1ms|


## 100% State Reproducibility
Goblin is in event-sourcing mode, which means that all state changes are recorded as events. So Goblin can be restored to any state by replaying the events.

This feature is very useful for followers to catch up with the leader. When a follower is down for a long time, it can restore the state by replaying the events from the leader. Thus, if the follower is promoted to be the leader, it can serve the requests immediately.

This feature is also useful for testing and debugging. For example, if you want to reproduce a bug, you can replay the events to restore the state when the bug occurred, and then you can debug it.

# Get Started
## Supported Platforms
- Ubuntu 20.04
## Server Setup
### Install Dependency
```
cd server && sudo bash ./scripts/setupDevEnvironment.sh
```

### Pull Third-Party & Submodule Codes
```
cd server && bash ./scripts/addSubmodules.sh
```
### Local Host Run
#### Build
```bash
cd server
./scripts/build.sh
```
#### Run
##### Start object manager (OM)
Object manager is a cluster that stores addresses of all object store clusters. Clients should config OM address and query OM to get OS address after starting up.
```bash
cd server
./example/object-manager-three-nodes/runCluster.sh
```
##### Start object store (OS)
Object store is data storage clusters, and it servers put/get/delete... traffics from clients.
```bash
cd server
./example/object-store-three-nodes/cluster1/runCluster.sh
```
##### Register OS in OM
```bash
cd server
./example/object-store-three-nodes/cluster1/addCluster.sh
```
After steps above, the Goblin server should be set up. OM can be accessed via the endpoints from the client:
```
1@0.0.0.0:50055,2@0.0.0.0:50056,3@0.0.0.0:50057
```
#### Run UnitTest
```bash
cd server/build && ./TestRunner
```

## Client Setup
### Supported Languages
A Maven based Java project is provided in Goblin/client and can be used as a dependency in a Java based application or other JVM based languages such as Groovy, Scala etc.

### Getting Started
clone this repo and build
```bash
cd client/goblin-java-client
mvn clean install
```

### Application
Most Goblin users will be creating a java application that will be reading and writing data to and from Goblin server.
* [Sample App using Goblin Java Client](./doc/sample_app.md)

### Sample Usage
All interactions with this library can be performed using the GoblinClient class. ``GoblinClient client = GoblinClient.newInstance(config)``

The [SampleGoblinClient.java](./client/goblin-java-client/src/main/java/com/ebay/goblin/sample/SampleGoblinClient.java) file includes instances of using the client. To run it as an application, you can easily execute the SampleGoblinClient.main() function.
Kindly be aware that the default cluster info is set to ``1@0.0.0.0:50055,2@0.0.0.0:50056,3@0.0.0.0:50057``. An update may be necessary to use the server endpoints set up above.
#### Put
```java
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
```
#### Get
```java
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
```
#### Delete
```java
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
```
To learn more about the different operations with the client, see the [Client API Reference for Java](./doc/client_api_reference.md)

### Grpcurl
If you don't want to use the java client, you could also use grpcurl to mock requests from grpc client and fill requests in json format. The grpc service and request/response are defined at Goblin/protocol/service.proto.
Since service.proto have imported other proto files in Goblin/protocols/, we should add "--import-path ./" into grpcurl parameters. Besides, we can use "-cacert server.crt" in grpcurl parameters to declaim trusted certificate files or "-insecure" to disable certificate verification.

#### Generate base64 key-value pair
We need to generate and fill base64 string in json for protobuf bytes fields
```bash
echo "test_key_1" | base64
dGVzdF9rZXlfMQo=
echo "test_value_1" | base64
dGVzdF92YWx1ZV8xCg==
```

#### Put
```bash
cat put.json
{
   "header":{
      "routeVersion":1
   },
   "entry":{
      "key":"dGVzdF9rZXlfMQo=",
      "value":"dGVzdF92YWx1ZV8xCg=="
   }
}
```

```bash
cd protocols
grpcurl -plaintext  -d @ --import-path ./ --proto service.proto 0.0.0.0:60057 goblin.proto.KVStore/Put < put.json
{
  "header": {
    "code": "OK",
    "message": "Success",
    "latestVersion": "1"
  },
  "result": {
    "code": "OK",
    "version": "1"
  }
}
```

#### Get
```bash
cat get.json
{
   "header":{
      "routeVersion":1
   },
   "entry":{
      "key":"dGVzdF9rZXlfMQo="
   }
}
```

```bash
cd protocols
grpcurl -plaintext -d @ --import-path ./ --proto service.proto 0.0.0.0:60057 goblin.proto.KVStore/Get < get.json
{
  "header": {
    "code": "OK",
    "message": "Success",
    "latestVersion": "116"
  },
  "result": {
    "code": "OK",
    "version": "116",
    "value": "dGVzdF92YWx1ZV8xCg=="
  }
}
```

# Contributing to This Project
We welcome contributions. If you find any bugs, potential flaws and edge cases, improvements, new feature suggestions or discussions, please submit issues or pull requests.
# Core Developers
- Yuwei (Crystal) Xu (yuwxu@ebay.com)
- Jingyi Chen (jingyichen@ebay.com)
- Dongbin Cheng (docheng@ebay.com)
- Jingwen Yin (jinyin@ebay.com)
- Yang Han (yanhan@ebay.com)
- Yong Jiao (yjiao@ebay.com)
- Qiawu (Chandler) Cai

Please see [here](./doc/CONTRIBUTORS.md) for all contributors.

# Contact
- Yong Jiao (yjiao@ebay.com)
- Yuwei (Crystal) Xu (yuwxu@ebay.com)
- Dongbin Cheng (docheng@ebay.com)

# Acknowledgements
Special thanks to [people](./doc/ACKNOWLEDGEMENTS.md) who give your support on this project.

# License Information
Copyright 2019-2020 eBay Inc.

Authors/Developers: Bin (Glen) Geng, Qi (Jacky) Jia

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at

https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.

# Use of 3rd Party Code
Some parts of this software include 3rd party code licensed under open source licenses:

1. Gringofts<br/>
   URL: https://github.com/eBay/Gringofts/<br/>
   License: https://github.com/eBay/Gringofts/blob/master/LICENSE.md<br/>
   Apache 2.0 license selected.

1. OpenSSL<br/>
   URL: https://www.openssl.org/<br/>
   License: https://www.openssl.org/source/license.html<br/>
   Originally licensed under the Apache 2.0 license.

1. RocksDB<br/>
   URL: https://github.com/facebook/rocksdb<br/>
   License: https://github.com/facebook/rocksdb/blob/master/LICENSE.Apache<br/>
   Apache 2.0 license selected.

1. SQLite<br/>
   https://www.sqlite.org/index.html<br/>
   License: https://www.sqlite.org/copyright.html<br/>
   SQLite Is Public Domain

1. abseil-cpp<br/>
   URL: https://github.com/abseil/abseil-cpp<br/>
   License: https://github.com/abseil/abseil-cpp/blob/master/LICENSE<br/>
   Originally licensed under the Apache 2.0 license.

1. cpplint<br/>
   URL: https://github.com/google/styleguide<br/>
   License: https://github.com/google/styleguide/blob/gh-pages/LICENSE<br/>
   Originally licensed under the Apache 2.0 license.

1. inih<br/>
   URL: https://github.com/benhoyt/inih<br/>
   License: https://github.com/benhoyt/inih/blob/master/LICENSE.txt
   Originally licensed under the New BSD license.

1. gRPC<br/>
   URL: https://github.com/grpc/grpc<br/>
   License: https://github.com/grpc/grpc/blob/master/LICENSE<br/>
   Originally licensed under the Apache 2.0 license.

1. googletest<br/>
   URL: https://github.com/google/googletest<br/>
   License: https://github.com/google/googletest/blob/master/LICENSE<br/>
   Originally licensed under the BSD 3-Clause "New" or "Revised" license.

1. prometheus-cpp<br/>
   URL: https://github.com/jupp0r/prometheus-cpp<br/>
   License: https://github.com/jupp0r/prometheus-cpp/blob/master/LICENSE
   Originally licensed under the MIT license.

1. spdlog<br/>
   URL: https://github.com/gabime/spdlog<br/>
   License: https://github.com/gabime/spdlog/blob/master/LICENSE<br/>
   Originally licensed under the MIT license.

1. boost<br/>
   URL: https://www.boost.org/<br/>
   License: https://www.boost.org/users/license.html<br/>
   Boost Software License - Version 1.0 - August 17th, 2003.

1. crypto++<br/>
   URL: https://www.cryptopp.com/<br/>
   License: https://www.cryptopp.com/License.txt<br/>
   Boost Software License - Version 1.0 - August 17th, 2003.
