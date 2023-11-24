# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0

import pathlib

from aws_cdk import (
    CfnOutput,
    aws_logs,
    aws_ec2,
    aws_ecs,
    aws_ecs_patterns,
    aws_iam,
    aws_cloudfront,
    aws_cloudfront_origins,
)
from constructs import Construct

from .workshop_studio import NestedWorkshopStudioStack


class GrafanaStack(NestedWorkshopStudioStack):

    def __init__(self, scope: Construct, construct_id: str, username: str, password: str, **kwargs) -> None:
        super().__init__(scope, construct_id, **kwargs)

        vpc = aws_ec2.Vpc(self, "GrafanaVPC", nat_gateways=1)

        cluster = aws_ecs.Cluster(self, "GrafanaCluster", vpc=vpc)

        task_definition = aws_ecs.FargateTaskDefinition(self, "GrafanaTaskDefinition",
            cpu=512,
            memory_limit_mib=2048,
        )
        task_definition.add_container('grafana',
            image=aws_ecs.ContainerImage.from_asset(str(pathlib.Path(__file__).parents[1].joinpath('grafana'))),
            essential=True,
            port_mappings=[aws_ecs.PortMapping(container_port=3000)],
            environment={
                "GF_SECURITY_ADMIN_USER": username,
                "GF_SECURITY_ADMIN_PASSWORD": password,
                "GF_INSTALL_PLUGINS": "grafana-timestream-datasource",
                "AWS_REGION": self.region,
            },
            logging=aws_ecs.LogDriver.aws_logs(stream_prefix="GrafanaService", log_retention=aws_logs.RetentionDays.ONE_WEEK),
        )
        task_definition.add_to_task_role_policy(aws_iam.PolicyStatement(
            effect=aws_iam.Effect.ALLOW,
            actions=[
                "timestream:CancelQuery",
                "timestream:DescribeDatabase",
                "timestream:DescribeEndpoints",
                "timestream:DescribeScheduledQuery",
                "timestream:DescribeTable",
                "timestream:ListDatabases",
                "timestream:ListMeasures",
                "timestream:ListScheduledQueries",
                "timestream:ListTables",
                "timestream:ListTagsForResource",
                "timestream:Select",
                "timestream:SelectValues",
            ],
            resources=["*"],
        ))

        service = aws_ecs_patterns.ApplicationLoadBalancedFargateService(self, "Service",
            cluster=cluster,
            desired_count=1,
            load_balancer_name="GrafanaALB",
            task_definition=task_definition,
        )
        service.target_group.configure_health_check(
            path="/api/health"
        )
        # remove unwanted Outputs
        try:
            service.node.try_remove_child("LoadBalancerDNS")
            service.node.try_remove_child("ServiceURL")
        except Exception as e:
            print("WARN: failed to clean CloudFormation Outputs:", e)

        distribution = aws_cloudfront.Distribution(self, "GrafanaDistribution",
            default_behavior=aws_cloudfront.BehaviorOptions(
                origin=aws_cloudfront_origins.LoadBalancerV2Origin(service.load_balancer, protocol_policy=aws_cloudfront.OriginProtocolPolicy.HTTP_ONLY),
                allowed_methods=aws_cloudfront.AllowedMethods.ALLOW_ALL,
                viewer_protocol_policy=aws_cloudfront.ViewerProtocolPolicy.REDIRECT_TO_HTTPS,
                origin_request_policy=aws_cloudfront.OriginRequestPolicy.ALL_VIEWER,
            )
        )
        self.grafana_url = CfnOutput(self, "GrafanaURL", value=f"https://{distribution.domain_name}")
