// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

import { CreateTopicRuleCommand, DeleteTopicRuleCommand } from "@aws-sdk/client-iot";
import { getIoTClient } from "./aws_access";
import { getEnv } from "./env";

export async function initSensorDataIngestion(writeToSerialPort) {
    document.getElementById('run_sensor_data_ingestion')?.addEventListener('click', async () => {
        writeToSerialPort(`\r\nworkshop run sensor_data_ingestion\r\n`);
    });
    (document.getElementById('open_grafana') as HTMLAnchorElement).href = getEnv().GrafanaURL;
    document.getElementById('create_iot_rule_sensor_data_ingestion')?.addEventListener('click', async () => {
        const spinner = document.getElementById('create_iot_rule_sensor_data_ingestion_spinner')!
        spinner.classList.remove('d-none');
        await createIoTRule();
        spinner.classList.add('d-none');
    });
}

async function createIoTRule() {
    const client = await getIoTClient(getEnv());

    const ruleName = 'demo_badge_sensors';

    try {
        console.log("DeleteTopicRuleCommand", await client.send(new DeleteTopicRuleCommand({
            ruleName: ruleName,
        })));
    } catch (error) {
        console.warn(error);
    }

    try {
        console.log("CreateTopicRuleCommand", await client.send(new CreateTopicRuleCommand({
            ruleName: ruleName,
            topicRulePayload: {
                awsIotSqlVersion: '2015-10-08',
                sql: `SELECT data.temperature, data.humidity, data.light, data.source FROM '$aws/rules/${ruleName}'`,
                actions: [
                    {
                        timestream: {
                            databaseName: getEnv().TimestreamDatabase,
                            tableName: getEnv().TimestreamTable,
                            dimensions: [
                                {
                                    name: 'source',
                                    value: '${data.source}',
                                }
                            ],
                            roleArn: getEnv().RuleActionTimestreamRoleARN,
                        }
                    }
                ],
            }
        })));
    } catch (error) {
        console.warn(error);
    }
}
