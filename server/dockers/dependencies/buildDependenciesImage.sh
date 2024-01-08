#!/bin/bash

docker build -f dockers/dependencies/download.Dockerfile -t hub.tess.io/magellan/grinkv:dependencies.v10 .
docker push hub.tess.io/magellan/grinkv:dependencies.v10