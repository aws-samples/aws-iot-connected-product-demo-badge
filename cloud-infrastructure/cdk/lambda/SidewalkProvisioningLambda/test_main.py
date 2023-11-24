# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0

import os
from unittest import TestCase

from botocore.stub import Stubber

os.environ["DESTINATION_ROLE_ARN"] = "testtesttesttesttesttesttesttesttesttesttesttest"

from main import (
    apply_destination_name,
    generate_bin,
    get_profile_and_device,
    iotwireless,
)

# Stripped down versions of actual API responses. These are the attributes that are needed
device_profile_json = {
    "Sidewalk": {
        "ApplicationServerPublicKey": "1111111111111111111111111111111111111111111111111111111111111111",
        "DakCertificateMetadata": [
            {
                "DeviceTypeId": "2222222222222",
            }
        ],
    },
}

wireless_device_json = {
    "Sidewalk": {
        "SidewalkManufacturingSn": "3333333333333333333333333333333333333333333333333333333333333333",
        "DeviceCertificates": [
            {
                "SigningAlg": "Ed25519",
                "Value": "444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444",
            },
            {
                "SigningAlg": "P256r1",
                "Value": "5555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555",
            },
        ],
        "PrivateKeys": [
            {
                "SigningAlg": "Ed25519",
                "Value": "6666666666666666666666666666666666666666666666666666666666666666",
            },
            {
                "SigningAlg": "P256r1",
                "Value": "7777777777777777777777777777777777777777777777777777777777777777",
            },
        ],
    },
}

expected_bin = bytearray(
    b"SID0\x00\x00\x00\x07\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff333333333333333333333333333333332222\xff\xff\xff\xff\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11ffffffffffffffffffffffffffffffff8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8ewwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwy\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e88\xe3\x8e8y\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9e\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\xe3\x8e8\xe3y\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e\x8e8\xe3\x8ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9e\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e88\xe3\x8e8y\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9e\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8\xe3\x8e8y\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xe7\x9ey\xff\xff\xff\xff"
)

iotwirelessmock = Stubber(iotwireless)


class TestMain(TestCase):
    def test_generate_bin(self):
        bin = generate_bin(device_profile_json, wireless_device_json)
        self.assertEqual(len(bin), 1420)
        self.assertEqual(bin, expected_bin)

    def test_empty_input(self):
        # given
        new_device_profile_id = "myid"
        new_wireless_device_id = "mydevice"
        expected_device_profile_json = {"Id": new_device_profile_id}
        expected_wireless_device_json = {"Id": new_wireless_device_id}
        iotwirelessmock.add_response("create_destination", {})
        iotwirelessmock.add_response(
            "create_device_profile", {"Id": new_device_profile_id}
        )
        iotwirelessmock.add_response(
            "create_wireless_device",
            {"Id": new_wireless_device_id},
        )

        iotwirelessmock.add_response(
            "get_device_profile",
            expected_device_profile_json,
            expected_params={"Id": new_device_profile_id},
        )
        iotwirelessmock.add_response(
            "get_wireless_device",
            expected_wireless_device_json,
            expected_params={
                "Identifier": new_wireless_device_id,
                "IdentifierType": "WirelessDeviceId",
            },
        )

        # when
        with iotwirelessmock:
            profile, device = get_profile_and_device(
                None, None, None
            )  # all params left empty by caller

        # then
        iotwirelessmock.assert_no_pending_responses()
        self.assertEqual(profile, expected_device_profile_json)
        self.assertEqual(device, expected_wireless_device_json)

    def test_dont_create_if_exists(self):
        # given
        destination = "mydest"
        device_profile_id = "myid"
        wireless_device_id = "mydevice"

        expected_device_profile_json = {"Id": device_profile_id}
        expected_wireless_device_json = {"Id": wireless_device_id}

        iotwirelessmock.add_response(
            "list_destinations", {"DestinationList": [{"Name": destination}]}
        )
        iotwirelessmock.add_response(
            "list_device_profiles", {"DeviceProfileList": [{"Id": device_profile_id}]}
        )
        iotwirelessmock.add_response(
            "list_wireless_devices",
            {
                "WirelessDeviceList": [
                    {
                        "Id": wireless_device_id,
                        "DestinationName": destination,
                        "Sidewalk": {},
                    }
                ]
            },
        )

        iotwirelessmock.add_response(
            "get_device_profile",
            expected_device_profile_json,
            expected_params={"Id": device_profile_id},
        )
        iotwirelessmock.add_response(
            "get_wireless_device",
            expected_wireless_device_json,
            expected_params={
                "Identifier": wireless_device_id,
                "IdentifierType": "WirelessDeviceId",
            },
        )

        # when
        with iotwirelessmock:
            profile, device = get_profile_and_device(
                destination, device_profile_id, wireless_device_id
            )  # all params set

        # then
        iotwirelessmock.assert_no_pending_responses()
        self.assertEqual(profile, expected_device_profile_json)
        self.assertEqual(device, expected_wireless_device_json)

    def test_ignore_special_chars(self):
        destination = "mydest"
        iotwirelessmock.add_response(
            "list_destinations", {"DestinationList": [{"Name": destination}]}
        )

        with iotwirelessmock:
            applied_destination = apply_destination_name(f" /**{destination} \\{{")

        self.assertEqual(destination, applied_destination)
