## High performance
Goblin provide stable, sustained high performance. Two factors define performance: latency and throughput. Latency is the time taken to complete an operation. Throughput is the total operations completed within some time period.

### Physical Node
| Node SKU | Memory | Processor Count | NIC Count | NIC Throughput |Disk |
|--------|---------|------------------|----------|-----------------|----|
| P3G6 | 384GB | 48 | 2 | 10Gb/s (1GB/s) | 2T * 3 SSD |


### Container Resource
| CPU | Memory | Disk |
| --- | -------|------|
| 15-10 | 150G-100G | PVC - local dynamic, SSD 500G |


### Goblin Server Parameters

| Vital parameters                | Value|
|---------------------------------|------|
| CPL threads                     | 16|
| Receive threads                 | 8| 
|  Reply threads                   | 5 read, 5 write, 10 metrics| 
|  LRU Capacity                    | 15G| 
|  Persist Queue Max Batch Size    | 100| 
|  Persist Queue Max Batch Latency | 5ms| 
| Raft AE Max Delay Time          | 10ms| 
| Raft AE max payload             | 50M| 
| Raft AE max size                | 2000| 

Performance metrics below are based on payload size 1k.
### Extreme Performance
|Test Case | QPS | P99 Latency from Client | P50 Latency from Client|
|----------|-----|-------------------------|------------------------|
|Leader Write | 23k | 81ms | 34ms|
|Leader Read | 26k | 23.3ms | 3.7ms|
|Follower Read | 10.3w | 5ms | 0.78ms|
|Leader Read Exist & Write | Write: 3.8k Read: 24.4k | Write: 50ms Read: 53ms | Write: 29ms Read: 15ms|
|Leader Read Nonexist & Write | Write: 3.3k Read: 24k | Write: 66ms Read: 52ms | Write: 31ms Read: 14.8ms|
|Leader Delete | 25k | 52ms | 33ms|

### Low Pressure Performance
| Test Case                    | QPS | P99 Latency from Client | P50 Latency from Client|
|------------------------------|-----|-------------------------|------------------------|
| Leader Write                 | 216 | 39ms | 23ms| 
|  Leader Read                  | 1909 | 13.8ms | 0.75ms| 
|  Follower Read                | 5242 | 2.8ms | 0.6ms| 
|  Leader Read Exist & Write    | Write: 228 Read: 1915 | Write: 37ms Read: 13ms | Write: 23ms Read: 0.73ms| 
|  Leader Read Nonexist & Write | Write: 226 Read: 1905 | Write: 37ms Read: 13ms | Write: 23ms Read: 0.73ms| 
| Leader Delete                | 235 | 36ms | 22ms| 
