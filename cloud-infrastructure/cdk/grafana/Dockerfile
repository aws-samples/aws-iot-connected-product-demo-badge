# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0

FROM public.ecr.aws/bitnami/grafana:10

# needed to download and store new executables
USER root

RUN mkdir -p /app/bin/

RUN curl -L -o /app/bin/http \
    https://github.com/httpie/cli/releases/download/3.2.2/http && \
    chmod +x /app/bin/http

RUN curl -L -o /app/bin/jq \
    https://github.com/jqlang/jq/releases/download/jq-1.6/jq-linux64 && \
    chmod +x /app/bin/jq

COPY provision.sh /app
COPY demo_badge_dashboard.json /app
COPY sidewalk_dashboard.json /app

# as defined in upstream image
USER 1001

ENTRYPOINT [ "/app/provision.sh" ]
