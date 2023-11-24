# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0

import base64
import struct

def lambda_handler(event, context):
    base64_payload = event["raw_payload"]
    print("Base64 payload:", len(base64_payload))

    hex_payload = base64.b64decode(base64_payload).decode()
    binary_payload = bytes.fromhex(hex_payload)
    print("Binary payload:", len(binary_payload), binary_payload)

    message_type = binary_payload[0]
    print("Message type:", message_type)

    if message_type == 0x42:
        _, temperature, humidity, light = struct.unpack("< c f f h", binary_payload)
        return {
            "source": "sidewalk",
            "temperature": temperature,
            "humidity": humidity,
            "light": light,
        }
    else:
        print("Unknown message type!", binary_payload, hex_payload, binary_payload)
        return {}
