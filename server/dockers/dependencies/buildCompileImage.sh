#!/bin/bash

docker build -f dockers/dependencies/install.Dockerfile -t hub.tess.io/magellan/grinkv:compile.v10 .
docker push hub.tess.io/magellan/grinkv:compile.v10