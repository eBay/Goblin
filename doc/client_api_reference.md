# Client API Reference for Java

- [API References](#api-references)
    * [Common Types](#common-types)
        + [KeyType](#keytype)
        + [ValueType](#valuetype)
    * [Put Operations](#put-operations)
        + [Shortcut Put APIs](#shortcut-put-apis)
    * [CAS Operations](#cas-operations)
        + [Shortcut CAS APIs](#shortcut-cas-apis)
    * [Get Operations](#get-operations)
        + [Shortcut Get APIs](#shortcut-get-apis)
    * [Delete Operations](#delete-operations)
        + [Shortcut Delete APIs](#shortcut-delete-apis)
    * [Mini-Transaction Operations](#mini-transaction-operations)
    * [Distribution Lock Operations](#distributed-lock-operations)
        + [Shortcut Lock APIs](#shortcut-lock-apis)

Pure Java client for Goblin with very limited dependencies. It could be used in all Java 8+ applications.

# API References
All Goblin APIs are implemented in the com.ebay.goblin.client.api.GoblinClient interface.

## Common Types
### KeyType
KeyType is the abstraction of a key in Goblin. It contains a string type key.
To build a key
```java
KeyType keyType = KeyType.builder().key(keyInString).build();
```
To get the content of a KeyType object.
```java
KeyType keyType = KeyType.builder().key(keyInString).build();
String key = keyType.getKey(); // equals to keyInString
```
### ValueType
ValueType is the abstraction of the Goblin values. To build a value
```java
byte[] content = new byte[] {31, 32, 33};
ValueType valueType = ValueType.builder().content(content).build());
```
We can also build a ValueType from a partial byte array.
```java
byte[] content = new byte[] {31, 32, 33, 34, 35};
ValueType valueType = ValueType.builder().content(content).offset(1).size(2).build()); // byte[] {32, 33} in the value
```
To get the content of a ValueType object.
```java
byte[] content = valueType.getContent();
```

## Put Operations
The basic Put API is
```java
PutResponse put(PutRequest request) throws GoblinException;
```
PutRequest:

| Field     | Type      | Remark                                                                                                                                                   |
|-----------|-----------|----------------------------------------------------------------------------------------------------------------------------------------------------------|
| key	      | KeyType	  | The key to be put into Goblin                                                                                                                            | 
| value     | ValueType | The value to be put into Goblin with respect to key                                                                                                      |
| enableTTL | Boolean   | true: TTL (time to live) will be enabled for the key<br>false: no TTL will be enabled for the key and any existing TTL will be cleared                   |
| ttl       | Integer   | The TTL for the key (in seconds, should be larger than zero), if enableTTL is true.<br>This value could be null to reset the start time of the last TTL. |

PutResponse:

| Field   | Type   | Remark                                          |
|---------|--------|-------------------------------------------------|
| version | Long   | The version of the key that was put into Goblin |

### Shortcut Put APIs
Put without TTL
```java
PutResponse put(KeyType key, ValueType value) throws GoblinException;
```
Put with a defined TTL
```java
PutResponse putWithTTL(KeyType key, ValueType value, Integer ttl) throws GoblinException;
```
Put with the last TTL, reset the start time of the TTL
```java
PutResponse putWithTTL(KeyType key, ValueType value) throws GoblinException;
```
### Put Only If Not Exist
Put if not exist without TTL
```java
PutResponse putnx(KeyType key, ValueType value) throws GoblinException;
```
Put if not exist with a defined TTL
```java
PutResponse putnxWithTTL(KeyType key, ValueType value, Integer ttl) throws GoblinException;
```
Put if not exist with the last TTL, reset the start time of the TTL
```java
PutResponse putnxWithTTL(KeyType key, ValueType value) throws GoblinException;
```

## CAS Operations
The basic CAS (Compare And Swap) API is
```java
CasResponse cas(CasRequest request) throws GoblinException;
```
CasRequest:

| Field           | Type      | Remark                                                                                                                                                   |
|-----------------|-----------|----------------------------------------------------------------------------------------------------------------------------------------------------------|
| key             | KeyType   | The key to be put into Goblin                                                                                                                            |
| value           | ValueType | The value to be put into Goblin with respect to key                                                                                                      |
| enableTTL       | Boolean   | true: TTL (time to live) will be enabled for the key<br>false: no TTL will be enabled for the key                                                        |
| ttl             | Integer   | The TTL for the key (in seconds, should be larger than zero), if enableTTL is true.<br>This value could be null to reset the start time of the last TTL. |
| comparedVersion | Long      | Only update the value for the key if the latest version in Goblin equals to comparedVersion.<br>Otherwise, throw a GoblinConditionNotMetException.       |

CasResponse:

| Field   | Type   | Remark                                          |
|---------|--------|-------------------------------------------------|
| version | Long   | The version of the key that was put into Goblin |

### Shortcut CAS APIs
CAS without TTL
```java
CasResponse cas(KeyType key, ValueType value, long comparedVersion) throws GoblinException;
```
CAS with a defined TTL
```java
CasResponse casWithTTL(KeyType key, ValueType value, long comparedVersion, Integer ttl) throws GoblinException;
```
CAS with the last TTL, reset the start time of the TTL
```java
CasResponse casWithTTL(KeyType key, ValueType value, long comparedVersion) throws GoblinException;
```

## Get Operations
The basic Get API is
```java
GetResponse get(GetRequest request) throws GoblinException;
```
GetRequest:

| Field      | Type    | Remark                                                                                                                                                                                                                |
|------------|---------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| key        | KeyType | The key to be get from Goblin                                                                                                                                                                                         |
| version    | Long    | If this value is null, query the latest version from Goblin. Otherwise, query the largest version which is not larger than this version                                                                               |
| allowStale | boolean | Allow the client to query from a follower instance in the Goblin cluster.<br>If this parameter is true, the read performance will be better. However, the client might not get the latest version.<br> Default: false |
| metaReturn | boolean | Return the user defined meta of the key in the response if this value is true. <br> Default: false                                                                                                                    |

GetResponse:

| Field           | Type                  | Remark                                                                                                                                                                                             |
|-----------------|-----------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| value           | Optional\<ValueType\> | If value.isPresent() is true, the key exists in Goblin. The actual valueType queried from Goblin could be got by value.get(). <br>If value.isPresent() is false, the key does not exist in Goblin. |
| version         | Long                  | The version of the returned value                                                                                                                                                                  |
| userDefinedMeta | UserDefinedMeta       | The user defined meta of the key                                                                                                                                                                   |

### Shortcut Get APIs
Get the latest version, do not allow stale read
```java
GetResponse get(KeyType key) throws GoblinException;
```
Get a specific version, do not allow stale read
```java
GetResponse get(KeyType key, Long version) throws GoblinException;
```
Get the latest version, allow stale read
```java
GetResponse get(String key, boolean allowStale) throws GoblinException;
```
Get the latest version with meta
```java
GetResponse getWithMeta(KeyType key) throws GoblinException;
```

## Delete Operations
The basic Delete API is
```java
DeleteResponse delete(DeleteRequest request) throws GoblinException;
```
DeleteRequest:

| Field       | Type    | Remark                                                                                                                                                              |
|-------------|---------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| key         | KeyType | The key to be get from Goblin                                                                                                                                       |
| version     | Long    | If this value is null, delete the latest version from Goblin. Otherwise, delete the specified version only if this value is equal to the latest version in Goblin   |
| returnValue | boolean | Return the value of the deleted key in the response if this value is true                                                                                           |


DeleteResponse:

| Field   | Type                  | Remark                                                                                                                                                                                                                                                                   |
|---------|-----------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| value   | Optional\<ValueType\> | If value.isPresent() is true, the key exists in Goblin. The actual valueType deleted from Goblin could be got by value.get(). <br>If value.isPresent() is false, the key does not exist in Goblin. This value is available only when returnValue is true in the request. |
| version | Long                  | The version of the deleted value. This value is available only when returnValue is true in the request.                                                                                                                                                                  |

### Shortcut Delete APIs
Delete the latest version, return the deleted value
```java
DeleteResponse delete(KeyType key) throws GoblinException;
```
Delete with all provided parameters
```java
DeleteResponse delete(KeyType key, Long version, boolean returnValue) throws GoblinException;
```
## Mini-Transaction Operations
The basic MiniTransaction API is
```
MiniTransaction newTransaction() throws GoblinException;
TransResponse commitTransaction(MiniTransaction trans) throws GoblinException;
```
Type: Condition

| Category        | Remark                                                                                                    |
|-----------------|-----------------------------------------------------------------------------------------------------------|
| withVersionCond | check if the version is same as given when executing transaction                                          |
| withExistCond   | check if key exists when executing transaction                                                            |
| withUdfCond     | check if user defined meta of the key meets the pre-defined conditioning logic when executing transaction |

Type: Transaction

| Method   | Remark                          |
|----------|---------------------------------|
| put      | put a kv in this transaction    |
| get      | get a kv in this transaction    |
| delete   | delete a kv in this transaction |

Please refer to [Transaction](transaction.md) for more details.

## Distributed Lock Operations
The basic Lock API is
```java
boolean lock(LockRequest request) throws GoblinException;
boolean unlock(KeyType key) throws GoblinException;
```
LockRequest:

| Field | Type      | Remark                                                       |
|-------|-----------|--------------------------------------------------------------|
| key   | KeyType   | The key to be get from Goblin                                |
| value | ValueType | The value to be put into Goblin with respect to key          |
| ttl   | boolean   | The TTL for the key (in seconds, should be larger than zero) |

### Shortcut Lock APIs
try to acquire lock on the key with a random value, and it will be extended automatically. Return true if succeeded
```java
boolean lock(KeyType key) throws GoblinException;
```
try to acquire lock on the key with a random value, set ttl to be expired. Return true if succeeded
```java
boolean lock(KeyType key, Integer timeout) throws GoblinException;
```
try to acquire lock on the key with a value, set ttl to be expired. Return true if succeeded. You can put a value with the key, and then get the value by key through query API later before the ttl.
```java
boolean lock(KeyType key, ValueType value, Integer timeout) throws GoblinException;
```
try to release lock. Return true on success
```java
boolean unlock(String key) throws GoblinException;
```
Please refer to [Distributed Lock](distributed_lock.md) for more details.