# This Dockerfile downloads all the external dependencies
# initial reference tag: hub.tess.io/magellan/trinidad:dependencies.v9
# copied from Magellan/Trinidad

# tag: hub.tess.io/magellan/grinkv:dependencies.v10
FROM hub.tess.io/tess/ubuntu-20.04:hardened
LABEL maintainer="yuwxu@ebay.com"
WORKDIR /usr/external
COPY scripts/downloadDependencies.sh /usr/external
ENV DEBIAN_FRONTEND noninteractive
RUN bash downloadDependencies.sh
