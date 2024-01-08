# This Dockerfile installs all the external dependencies
# tag: hub.tess.io/magellan/grinkv:compile.v10
FROM hub.tess.io/magellan/grinkv:dependencies.v10
LABEL maintainer="yuwxu@ebay.com"
WORKDIR /usr/external
ENV DEBIAN_FRONTEND noninteractive
COPY scripts/installDependencies.sh /usr/external
RUN bash installDependencies.sh
