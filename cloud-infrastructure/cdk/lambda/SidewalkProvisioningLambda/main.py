# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0

import base64
import json
import os
import random
import re
import string

import boto3

from run import AttrDict, SidMfgAwsJson, SidMfgOutBin, valid_yaml_file

chip_config = AttrDict(valid_yaml_file("nordic.yaml"))
iotwireless = boto3.client("iotwireless")

# config
destination_role_arn = os.environ.get("DESTINATION_ROLE_ARN")
destination_mqtt_topic = os.environ.get("DESTINATION_MQTT_TOPIC")


def lambda_handler(event, context):
    if not (event["path"] == "/sidewalk_provision" and event["httpMethod"] == "POST"):
        return {"statusCode": 400, "body": "Operation not permitted"}

    if not event["body"]:
        return {
            "statusCode": 400,
            "body": "Missing body",
            "headers": {"Access-Control-Allow-Origin": "*"},
        }
    body = json.loads(event["body"])

    destination_name = body.get("destination_name")  # could be None
    device_profile_id = body.get("device_profile_id")
    wireless_device_id = body.get("wireless_device_id")

    try:
        device_profile_json, wireless_device_json = get_profile_and_device(
            destination_name, device_profile_id, wireless_device_id
        )
    except ValidationError as e:
        return {
            "statusCode": 400,
            "body": str(e),
            "headers": {"Access-Control-Allow-Origin": "*"},
        }

    payload = generate_bin(device_profile_json, wireless_device_json)
    body = base64.b64encode(payload).decode("utf-8")

    return {
        "headers": {
            "Content-Type": "application/octet-stream",
            "Access-Control-Allow-Origin": "*",
        },
        "statusCode": 200,
        "body": body,
        "isBase64Encoded": True,
    }


def get_profile_and_device(
    destination_name: str, device_profile_id: str, wireless_device_id: str
) -> tuple:
    print(f"Input: {destination_name=}; {device_profile_id=}; {wireless_device_id=}")

    destination_name = apply_destination_name(destination_name)
    device_profile_id = apply_device_profile_id(device_profile_id)
    wireless_device_id = apply_wireless_device_id(
        wireless_device_id, destination_name, device_profile_id
    )

    print(
        f"After validation, working with: {destination_name=}; {device_profile_id=}; {wireless_device_id=}"
    )

    device_profile = iotwireless.get_device_profile(Id=device_profile_id)

    wireless_device = iotwireless.get_wireless_device(
        Identifier=wireless_device_id, IdentifierType="WirelessDeviceId"
    )

    return device_profile, wireless_device


# takes a destination name and returns a validated existing one
def apply_destination_name(destination_name: str) -> str:
    if destination_name is None:
        # create new destination
        destination_name = random_resource_name()
        iotwireless.create_destination(
            Name=destination_name,
            ExpressionType="MqttTopic",
            Expression=destination_mqtt_topic,
            RoleArn=destination_role_arn,
        )
        return destination_name

    # if they sent a name, make sure it is correct
    destinations = [
        destination["Name"]
        for destination in iotwireless.list_destinations()["DestinationList"]
    ]

    if destination_name in destinations:
        return destination_name

    if re.sub("[^a-zA-Z0-9-_]+", "", destination_name) in destinations:
        return re.sub(
            "[^a-zA-Z0-9-_]+", "", destination_name
        )  # accidental special chars?

    raise ValidationError("Destination not found")


# takes a device profile ID and returns a validated existing one
def apply_device_profile_id(device_profile_id: str) -> str:
    if device_profile_id is None:
        # create new device profile
        profile_name = random_resource_name()
        device_profile = iotwireless.create_device_profile(
            Sidewalk={}, Name=profile_name
        )
        return device_profile["Id"]

    # if they sent an ID, make sure it is correct
    device_profile_ids = [
        profile["Id"]
        for profile in iotwireless.list_device_profiles()["DeviceProfileList"]
    ]
    if device_profile_id in device_profile_ids:
        return device_profile_id

    if re.sub("[^a-z0-9-]+", "", device_profile_id) in device_profile_ids:
        return re.sub("[^a-z0-9-]+", "", device_profile_id)  # accidental special chars?

    raise ValidationError("Device Profile ID not found")


# takes a device ID and destination and returns a validated existing device ID (note that destination is assumed to exist)
def apply_wireless_device_id(
    wireless_device_id: str, destination_name: str, device_profile_id: str
) -> str:
    if wireless_device_id is None:
        # create new device
        device = iotwireless.create_wireless_device(
            Type="Sidewalk",
            DestinationName=destination_name,
            Sidewalk={"DeviceProfileId": device_profile_id},
        )
        return device["Id"]

    # if they sent an ID, make sure it is correct
    device_ids = [
        device["Id"]
        for device in iotwireless.list_wireless_devices(
            DeviceProfileId=device_profile_id,
        )["WirelessDeviceList"]
        if (
            device["DestinationName"] == destination_name
            and device.get("Sidewalk") is not None
        )  # can only filter by one attribute in list_wireless_devices directly
    ]
    if wireless_device_id in device_ids:
        return wireless_device_id

    if re.sub("[^a-z0-9-]+", "", wireless_device_id) in device_ids:
        return re.sub(
            "[^a-z0-9-]+", "", wireless_device_id
        )  # accidental special chars?

    raise ValidationError(
        "Wireless device ID not found. Did you pass the right destination name and profile ID?"
    )


class ValidationError(Exception):
    pass


def random_resource_name() -> str:
    return "demo_badge_" + "".join(
        random.choice(string.ascii_lowercase) for i in range(10)
    )


def generate_bin(device_profile_json, wireless_device_json) -> bytearray:
    input = SidMfgAwsJson(
        aws_wireless_device_json=wireless_device_json,
        aws_device_profile_json=device_profile_json,
        config=chip_config,
    )

    output_writer = SidMfgOutBin(config=chip_config, file_name=None)

    output_writer._resize_encoded()  # array of the right size, initialized with \xff
    output_writer.write(input)
    return output_writer._encoded
