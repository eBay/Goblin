# Sharding
## Introduction
Sharding is a mechanism that allows you to scale out Goblin clusters.
One cluster has capacity limitation. If you want more capacity, you need more clusters with a sharding strategy.

Currently, we support static sharding, which means the number of clusters is determined when creating Goblin clusters.
Clusters can't be scaled out dynamically after creation. We will support dynamic sharding in the future.
