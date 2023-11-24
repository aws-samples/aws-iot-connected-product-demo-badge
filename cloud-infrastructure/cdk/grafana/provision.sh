#!/usr/bin/env bash

# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0

(
    /opt/bitnami/scripts/grafana/entrypoint.sh /opt/bitnami/scripts/grafana/run.sh
) &

set -u

export PATH=/app/bin:$PATH

GRAFANA_URL=${GRAFANA_URL:-http://localhost:3000}

sleep 15

for i in {1..100}; do
    if http -vv --check-status --ignore-stdin --timeout=5 \
        -a ${GF_SECURITY_ADMIN_USER}:${GF_SECURITY_ADMIN_PASSWORD} --print b --output /tmp/key.json \
        POST ${GRAFANA_URL}/api/auth/keys \
        name=myKey-$(date +%s) role=Admin secondsToLive:=300 ;
    then
        printf "\nGrafana Provisioning: successfully created API key.\n"
        break
    fi

    printf "\nGrafana Provisioning: API key not yet created...\n"
    sleep 5
done

key=$(cat /tmp/key.json | jq -r '.key')
rm /tmp/key.json

for i in {1..10}; do
    if http -vv --check-status --ignore-stdin --timeout=5 \
        POST ${GRAFANA_URL}/api/datasources \
        Authorization:"Bearer ${key}" \
        name="Amazon Timestream" isDefault:=true type="grafana-timestream-datasource" access=proxy \
        jsonData:='{"defaultRegion": "'"${AWS_REGION}"'","defaultDatabase":"\"SensorData\""}'
    then
        printf "\nGrafana Provisioning: successfully created Timestream data source.\n"
        break
    fi
    printf "\nGrafana Provisioning: Timestream data source not yet created...\n"
    sleep 5
done

for i in {1..10}; do
    if http -vv --check-status --ignore-stdin --timeout=5 --print b --output /tmp/dashboard.json \
        POST $GRAFANA_URL/api/dashboards/db \
        Authorization:"Bearer ${key}" \
        dashboard:=@/app/sidewalk_dashboard.json
    then
        printf "\nGrafana Provisioning: successfully imported sidewalk_dashboard.\n"
        break
    fi
    printf "\nGrafana Provisioning: sidewalk_dashboard not yet imported...\n"
    sleep 5
done

for i in {1..10}; do
    if http -vv --check-status --ignore-stdin --timeout=5 --print b --output /tmp/dashboard.json \
        POST $GRAFANA_URL/api/dashboards/db \
        Authorization:"Bearer ${key}" \
        dashboard:=@/app/demo_badge_dashboard.json
    then
        printf "\nGrafana Provisioning: successfully imported demo_badge_dashboard.\n"
        break
    fi
    printf "\nGrafana Provisioning: demo_badge_dashboard not yet imported...\n"
    sleep 5
done

dashboard_id=$(cat /tmp/dashboard.json | jq -r '.id')

for i in {1..10}; do
    if http -vv --check-status --ignore-stdin --timeout=5 \
        PATCH $GRAFANA_URL/api/org/preferences \
        Authorization:"Bearer ${key}" \
        homeDashboardId:="${dashboard_id}"
    then
        printf "\nGrafana Provisioning: successfully set new home dashboard.\n"
        break
    fi
    printf "\nGrafana Provisioning: new home dashboard not yet set...\n"
    sleep 5
done

printf "\nGrafana Provisioning: complete.\n"

wait
