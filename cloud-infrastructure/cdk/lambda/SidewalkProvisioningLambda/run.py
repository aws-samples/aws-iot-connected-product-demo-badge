#!/usr/bin/env python3

# file taken from:
# https://github.com/aws-samples/aws-iot-core-for-amazon-sidewalk-sample-app/blob/b1427892b2979eacd6cdea2a36158d69388f2f8e/EdgeDeviceProvisioning/tools/provision/sid_provision/run.py
# and shortened

# Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0

from __future__ import annotations

import argparse
import base64
import binascii
import json
import sys
import traceback
from ctypes import Structure, c_ubyte
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Any, Iterator, List, Optional, Union

import yaml
from intelhex import IntelHex

PROVISION_MFG_STORE_VERSION = 7

try:
    from rich import print
except ImportError:
    pass

SMSN_SIZE: int = 32
SERIAL_SIZE: int = 4
PRK_SIZE: int = 32
ED25519_PUB_SIZE: int = 32
P256R1_PUB_SIZE: int = 64
SIG_SIZE: int = 64

# pylint: disable=C0114,C0115,C0116


class AttrDict(dict):
    """
    A class to convert a nested Dictionary into an object with key-values
    that are accessible using attribute notation (AttrDict.attribute) instead of
    key notation (Dict["key"]). This class recursively sets Dicts to objects,
    allowing you to recurse down nested dicts (like: AttrDict.attr.attr)
    """

    # Inspired by:
    # http://stackoverflow.com/a/14620633/1551810
    # http://databio.org/posts/python_AttributeDict.html

    def __init__(self, iterable, **kwargs):
        super(AttrDict, self).__init__(iterable, **kwargs)
        for key, value in iterable.items():
            if isinstance(value, dict):
                self.__dict__[key] = AttrDict(value)
            else:
                self.__dict__[key] = value


def print_subprocess_results(result, subprocess_name="", withAssert=True):
    def check_error_in_line(line):
        return "error" in line.lower()

    for line in result.stdout.decode().splitlines():
        print(line)
        if withAssert:
            assert not check_error_in_line(
                line
            ), f"Something went wrong after calling subprocess {subprocess_name}"

    for line in result.stderr.decode().splitlines():
        print(line, file=sys.stderr)
        if withAssert:
            assert not check_error_in_line(
                line
            ), f"Something went wrong after calling subprocess {subprocess_name}"


class SidMfgValueId(Enum):
    """
    Please note that these values have to be in sync at alls times with
    projects/sid/sal/common/public/sid_pal_ifc/mfg_store/sid_pal_mfg_store_ifc.h
    sid_pal_mfg_store_value_t
    """

    """
    Format
    SID_PAL_MFG_STORE_XXXX = (<VALUE>, <SIZE>)
    """

    SID_PAL_MFG_STORE_MAGIC = (0, 4)
    SID_PAL_MFG_STORE_DEVID = (1, 5)
    SID_PAL_MFG_STORE_VERSION = (2, 4)
    SID_PAL_MFG_STORE_SERIAL_NUM = (3, 17)
    SID_PAL_MFG_STORE_SMSN = (4, 32)
    SID_PAL_MFG_STORE_APP_PUB_ED25519 = (5, 32)
    SID_PAL_MFG_STORE_DEVICE_PRIV_ED25519 = (6, 32)
    SID_PAL_MFG_STORE_DEVICE_PUB_ED25519 = (7, 32)
    SID_PAL_MFG_STORE_DEVICE_PUB_ED25519_SIGNATURE = (8, 64)
    SID_PAL_MFG_STORE_DEVICE_PRIV_P256R1 = (9, 32)
    SID_PAL_MFG_STORE_DEVICE_PUB_P256R1 = (10, 64)
    SID_PAL_MFG_STORE_DEVICE_PUB_P256R1_SIGNATURE = (11, 64)
    SID_PAL_MFG_STORE_DAK_PUB_ED25519 = (12, 32)
    SID_PAL_MFG_STORE_DAK_PUB_ED25519_SIGNATURE = (13, 64)
    SID_PAL_MFG_STORE_DAK_ED25519_SERIAL = (14, 4)
    SID_PAL_MFG_STORE_DAK_PUB_P256R1 = (15, 64)
    SID_PAL_MFG_STORE_DAK_PUB_P256R1_SIGNATURE = (16, 64)
    SID_PAL_MFG_STORE_DAK_P256R1_SERIAL = (17, 4)
    SID_PAL_MFG_STORE_PRODUCT_PUB_ED25519 = (18, 32)
    SID_PAL_MFG_STORE_PRODUCT_PUB_ED25519_SIGNATURE = (19, 64)
    SID_PAL_MFG_STORE_PRODUCT_ED25519_SERIAL = (20, 4)
    SID_PAL_MFG_STORE_PRODUCT_PUB_P256R1 = (21, 64)
    SID_PAL_MFG_STORE_PRODUCT_PUB_P256R1_SIGNATURE = (22, 64)
    SID_PAL_MFG_STORE_PRODUCT_P256R1_SERIAL = (23, 4)
    SID_PAL_MFG_STORE_MAN_PUB_ED25519 = (24, 32)
    SID_PAL_MFG_STORE_MAN_PUB_ED25519_SIGNATURE = (25, 64)
    SID_PAL_MFG_STORE_MAN_ED25519_SERIAL = (26, 4)
    SID_PAL_MFG_STORE_MAN_PUB_P256R1 = (27, 64)
    SID_PAL_MFG_STORE_MAN_PUB_P256R1_SIGNATURE = (28, 64)
    SID_PAL_MFG_STORE_MAN_P256R1_SERIAL = (29, 4)
    SID_PAL_MFG_STORE_SW_PUB_ED25519 = (30, 32)
    SID_PAL_MFG_STORE_SW_PUB_ED25519_SIGNATURE = (31, 64)
    SID_PAL_MFG_STORE_SW_ED25519_SERIAL = (32, 4)
    SID_PAL_MFG_STORE_SW_PUB_P256R1 = (33, 64)
    SID_PAL_MFG_STORE_SW_PUB_P256R1_SIGNATURE = (34, 64)
    SID_PAL_MFG_STORE_SW_P256R1_SERIAL = (35, 4)
    SID_PAL_MFG_STORE_AMZN_PUB_ED25519 = (36, 32)
    SID_PAL_MFG_STORE_AMZN_PUB_P256R1 = (37, 64)
    SID_PAL_MFG_STORE_APID = (38, 4)
    SID_PAL_MFG_STORE_CORE_VALUE_MAX = (4000, None)

    def __init__(self, value: int, size: int) -> None:
        # Overload the value so that the enum value corresponds to the
        # Mfg value
        self._value_ = value
        self.size = size


class SidSupportedPlatform(Enum):
    """
    These are the supported sidewalk platforms
    """

    NORDIC = (0, "nordic")
    TI = (1, "ti")
    SILABS = (2, "silabs")
    GENERIC = (3, "generic")

    def __init__(self, value: int, str_name: str) -> None:
        # Overload the value so that the enum value corresponds to the
        # Mfg value
        self._value_ = value
        self.str_name = str_name


@dataclass
class SidArgument:
    name: str
    help: str
    ext: str = ""
    const: str = ""
    handle_class: Any = None
    default: Any = None
    actual_default: Any = None
    required: Any = False
    intype: Any = None
    choices: Any = None
    additional_help: Any = None
    action: str = "store"

    @property
    def arg_name(self) -> str:
        return self.name[2:]


@dataclass
class SidInputGroup:
    name: str
    help: str
    handle_class: Any
    arguments: List[SidArgument]
    common_arguments: List[SidArgument] = field(default_factory=list)


@dataclass
class SidChipAddr:
    name: str
    offset_addr: int
    full_name: str = ""
    mem: int = 0
    default: bool = False

    @property
    def help_str(self) -> str:
        help_str = f"{self.name}"
        if self.full_name:
            help_str += f":{self.full_name}"
        if self.mem:
            help_str += f" mem:{self.mem}"
        if self.offset_addr:
            help_str += f" address: {hex(self.offset_addr)}"
        return help_str


@dataclass
class SidPlatformArgs:
    platform: SidSupportedPlatform
    input_groups: list[SidInputGroup]
    addtional_input_args: list[SidArgument] = field(default_factory=list)
    output_args: list[SidArgument] = field(default_factory=list)
    config_file: Any = None
    chips: list[SidChipAddr] = field(default_factory=list)

    def get_chip_from_name_mem(self, name: str, mem: int) -> Union[SidChipAddr, None]:
        for _ in self.chips:
            if _.name == name and _.mem == mem:
                return _
        return None


@dataclass
class SidArgOutContainer:
    platform: SidPlatformArgs
    input: SidInputGroup
    arg: SidArgument
    chip: SidChipAddr


class StructureHelper:
    def __repr__(self) -> str:
        repr_str = f"{self.__class__.__name__}\n"
        # type: ignore
        repr_str += f" device_prk-{binascii.hexlify(self.device_prk).upper()}\n"
        for _ in self.__class__._fields_:  # type: ignore
            field_name = _[0]
            # type: ignore
            repr_str += f" {field_name}: {binascii.hexlify(getattr(self, field_name)).upper()}\n"
        return repr_str


class SidCertMfgP256R1Chain(Structure, StructureHelper):
    # pylint: disable=C0326
    _fields_ = [
        ("smsn", c_ubyte * SMSN_SIZE),
        ("device_pub", c_ubyte * P256R1_PUB_SIZE),
        ("device_sig", c_ubyte * SIG_SIZE),
        ("dak_serial", c_ubyte * SERIAL_SIZE),
        ("dak_pub", c_ubyte * P256R1_PUB_SIZE),
        ("dak_sig", c_ubyte * SIG_SIZE),
        ("product_serial", c_ubyte * SERIAL_SIZE),
        ("product_pub", c_ubyte * P256R1_PUB_SIZE),
        ("product_sig", c_ubyte * SIG_SIZE),
        ("man_serial", c_ubyte * SERIAL_SIZE),
        ("man_pub", c_ubyte * P256R1_PUB_SIZE),
        ("man_sig", c_ubyte * SIG_SIZE),
        ("sw_serial", c_ubyte * SERIAL_SIZE),
        ("sw_pub", c_ubyte * P256R1_PUB_SIZE),
        ("sw_sig", c_ubyte * SIG_SIZE),
        ("root_serial", c_ubyte * SERIAL_SIZE),
        ("root_pub", c_ubyte * P256R1_PUB_SIZE),
        ("root_sig", c_ubyte * SIG_SIZE),
    ]

    def __new__(cls, cert_buffer: bytes, priv: bytes):  # type: ignore
        return cls.from_buffer_copy(cert_buffer)

    def __init__(self: SidCertMfgP256R1Chain, cert_buffer: bytes, priv: bytes) -> None:
        self._cert_buffer = cert_buffer
        _device_prk = bytearray(binascii.unhexlify(priv))
        """
        Sometimes cloud generates p256r1 private key with an invalid preceding
        00, handle that case
        """
        if len(_device_prk) == PRK_SIZE + 1 and _device_prk[0] == 00:
            print(f"P256R1 private key size is {PRK_SIZE+1}, truncate to {PRK_SIZE}")
            del _device_prk[0]

        self.device_prk = bytes(_device_prk)

        assert (
            len(self.device_prk) == PRK_SIZE
        ), "Invalid P256R1 private key size -{} Expected Size -{}".format(
            len(self.device_prk), PRK_SIZE
        )


class SidCertMfgED25519Chain(Structure, StructureHelper):
    # pylint: disable=C0326
    _fields_ = [
        ("smsn", c_ubyte * SMSN_SIZE),
        ("device_pub", c_ubyte * ED25519_PUB_SIZE),
        ("device_sig", c_ubyte * SIG_SIZE),
        ("dak_serial", c_ubyte * SERIAL_SIZE),
        ("dak_pub", c_ubyte * ED25519_PUB_SIZE),
        ("dak_sig", c_ubyte * SIG_SIZE),
        ("product_serial", c_ubyte * SERIAL_SIZE),
        ("product_pub", c_ubyte * ED25519_PUB_SIZE),
        ("product_sig", c_ubyte * SIG_SIZE),
        ("man_serial", c_ubyte * SERIAL_SIZE),
        ("man_pub", c_ubyte * ED25519_PUB_SIZE),
        ("man_sig", c_ubyte * SIG_SIZE),
        ("sw_serial", c_ubyte * SERIAL_SIZE),
        ("sw_pub", c_ubyte * ED25519_PUB_SIZE),
        ("sw_sig", c_ubyte * SIG_SIZE),
        ("root_serial", c_ubyte * SERIAL_SIZE),
        ("root_pub", c_ubyte * ED25519_PUB_SIZE),
        ("root_sig", c_ubyte * SIG_SIZE),
    ]

    def __new__(cls, cert_buffer: bytes, priv: bytes):  # type: ignore
        return cls.from_buffer_copy(cert_buffer)

    def __init__(self: SidCertMfgED25519Chain, cert_buffer: bytes, priv: bytes):
        self._cert_buffer = cert_buffer
        self.device_prk = binascii.unhexlify(priv)
        assert (
            len(self.device_prk) == PRK_SIZE
        ), "Invalid ED25519 private key size -{} Expected Size -{}".format(
            len(self.device_prk), PRK_SIZE
        )


class SidCertMfgCert:
    @staticmethod
    def from_base64(
        cert: bytes, priv: bytes, is_p256r1: bool = False
    ) -> Union[SidCertMfgP256R1Chain, SidCertMfgED25519Chain]:
        if is_p256r1:
            return SidCertMfgP256R1Chain(base64.b64decode(cert), priv)
        return SidCertMfgED25519Chain(base64.b64decode(cert), priv)


class SidMfgObj:
    def __init__(
        self: SidMfgObj,
        mfg_enum: SidMfgValueId,
        value: Any,
        info: dict[str, int],
        skip: bool = False,
        word_size: int = 4,
        is_network_order: bool = True,
    ):
        assert isinstance(word_size, int)
        assert (word_size > 0 and info) or (word_size == 0 and not info)

        _info = AttrDict(info) if isinstance(info, dict) else info

        self._name: str = mfg_enum.name
        self._value: Any = value
        self._start: int = 0 if not _info else _info.start
        self._end: int = 0 if not _info else _info.end
        self._word_size: int = word_size
        self._id_val: int = mfg_enum.value
        self._skip: bool = skip

        if info:
            assert (
                self._start < self._end
            ), "Invalid {}  end offset: {} < start offset: {}".format(
                self._name, self._end, self._start
            )
            byte_len = self.end - self.start
        else:
            byte_len = len(value)

        self._encoded: bytes = bytes(bytearray())
        if isinstance(self._value, int):
            self._encoded = (self._value).to_bytes(
                byte_len, byteorder="big" if is_network_order else "little"
            )
        elif isinstance(self._value, bytes):
            self._encoded = self._value
        elif isinstance(self._value, bytearray):
            self._encoded = bytes(self._value)
        elif isinstance(self._value, str):
            self._encoded = bytes(self._value, "ascii")
        else:
            try:
                self._encoded = bytes(self._value)
            except TypeError as ex:
                raise ValueError(
                    "{} Cannot convert value {} to bytes".format(
                        self._name, self._value
                    )
                ) from ex

        if len(self._encoded) < byte_len:
            self._encoded = self._encoded.ljust(byte_len, b"\x00")

        if len(self._encoded) != byte_len:
            ex_str = (
                "Field {} value {} len {} mismatch expected field value len {}".format(
                    self._name, self._value, len(self._encoded), byte_len
                )
            )
            raise ValueError(ex_str)

        if byte_len != mfg_enum.size:
            print(f"{self} has incorrect size {byte_len} expected {mfg_enum.size}")

    @property
    def name(self: SidMfgObj) -> str:
        return self._name

    @property
    def start(self: SidMfgObj) -> int:
        return self._start * self._word_size

    @property
    def end(self: SidMfgObj) -> int:
        return self._end * self._word_size

    @property
    def encoded(self: SidMfgObj) -> bytes:
        return self._encoded

    @property
    def id_val(self: SidMfgObj) -> int:
        return self._id_val

    @property
    def skip(self: SidMfgObj) -> bool:
        return self._skip

    def __repr__(self: SidMfgObj) -> str:
        val: Any = None
        if isinstance(self._value, str):
            val = self._value
        elif isinstance(self._value, int):
            val = self._value
        else:
            val = binascii.hexlify(self._encoded).upper()

        return f"{self._name}[{self.start}:{self.end}] : {val}"


class SidMfg:
    def __init__(
        self: SidMfg, app_pub: Union[None, bytes], config: Any, is_network_order: bool
    ) -> None:
        self._config = config
        self._app_pub: Optional[bytes] = app_pub
        self._apid: Optional[str] = None
        self._is_network_order: bool = is_network_order
        self._mfg_objs: List[SidMfgObj] = []
        self._word_size: int = 0 if not self._config else self._config.offset_size

    def __iter__(self: SidMfg) -> Iterator[SidMfgObj]:
        return iter(sorted(self._mfg_objs, key=lambda mfg_obj: mfg_obj.id_val))

    def __repr__(self: SidMfg) -> str:
        # type: ignore
        value = f"{str(self._ed25519)} \n{str(self._p256r1)} \n"
        value += "SID Values\n"
        value += "\n".join([f" {str(_)}" for _ in self._mfg_objs])
        value += "\n"
        return value

    def append(
        self: SidMfg, mfg_enum: SidMfgValueId, value: Any, can_skip: bool = False
    ) -> None:
        try:
            offset_config = (
                0 if not self._config else self._config.mfg_offsets[mfg_enum.name]
            )
            mfg_obj = SidMfgObj(
                mfg_enum,
                value,
                offset_config,
                skip=can_skip,
                word_size=self._word_size,
                is_network_order=self._is_network_order,
            )
            self._mfg_objs.append(mfg_obj)
        except KeyError as ex:
            if can_skip:
                print("Skipping {}".format(mfg_enum.name))
            else:
                raise ex
        except Exception:
            traceback.print_exc()
            exit(1)

    @property
    def mfg_version(self):
        return PROVISION_MFG_STORE_VERSION.to_bytes(
            4, byteorder="big" if self._is_network_order else "little"
        )

    @classmethod
    def from_args(cls, __args__: argparse.Namespace, __pa__) -> None:
        print(f"{cls} is not supported")
        sys.exit(1)


class SidMfgAwsJson(SidMfg):
    def __init__(
        self: SidMfgAwsJson,
        aws_wireless_device_json: Any,
        aws_device_profile_json: Any,
        config: Any,
        is_network_order: bool = True,
    ) -> None:
        super().__init__(app_pub=None, config=config, is_network_order=is_network_order)

        _aws_wireless_device_json = AttrDict(aws_wireless_device_json)
        _aws_device_profile_json = AttrDict(aws_device_profile_json)

        def get_value(crypt_keys: Any, key_type: str) -> Any:
            for _ in crypt_keys:
                _ = AttrDict(_)
                if _.SigningAlg == key_type:
                    return _.Value
            return None

        def unhex(unhex_val: str) -> bytes:
            return binascii.unhexlify(unhex_val)

        if _aws_wireless_device_json and _aws_device_profile_json:
            self._ed25519 = SidCertMfgCert.from_base64(
                get_value(
                    _aws_wireless_device_json.Sidewalk.DeviceCertificates, "Ed25519"
                ),
                get_value(_aws_wireless_device_json.Sidewalk.PrivateKeys, "Ed25519"),
            )
            self._p256r1 = SidCertMfgCert.from_base64(
                get_value(
                    _aws_wireless_device_json.Sidewalk.DeviceCertificates, "P256r1"
                ),
                get_value(_aws_wireless_device_json.Sidewalk.PrivateKeys, "P256r1"),
                is_p256r1=True,
            )

            _apid = self._get_apid_from_aws_device_profile_json(
                _aws_device_profile_json
            )
            if _apid is None:
                print(f"ApId or DeviceTypeId is not found in device profile")
                sys.exit(1)
            else:
                self._apid = _apid

            self._smsn = unhex(
                _aws_wireless_device_json.Sidewalk.SidewalkManufacturingSn
            )
            self._app_pub = unhex(
                _aws_device_profile_json.Sidewalk.ApplicationServerPublicKey
            )
        else:
            print("Error path should not have come here")
            sys.exit()

        self.append(SidMfgValueId.SID_PAL_MFG_STORE_MAGIC, "SID0", can_skip=True)
        self.append(SidMfgValueId.SID_PAL_MFG_STORE_VERSION, self.mfg_version)
        self.append(SidMfgValueId.SID_PAL_MFG_STORE_SMSN, self._smsn)
        self.append(SidMfgValueId.SID_PAL_MFG_STORE_APID, self._apid)
        self.append(SidMfgValueId.SID_PAL_MFG_STORE_APP_PUB_ED25519, self._app_pub)

        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_DEVICE_PRIV_ED25519,
            self._ed25519.device_prk,
        )
        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_DEVICE_PUB_ED25519, self._ed25519.device_pub
        )
        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_DEVICE_PUB_ED25519_SIGNATURE,
            self._ed25519.device_sig,
        )
        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_DEVICE_PRIV_P256R1, self._p256r1.device_prk
        )
        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_DEVICE_PUB_P256R1, self._p256r1.device_pub
        )
        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_DEVICE_PUB_P256R1_SIGNATURE,
            self._p256r1.device_sig,
        )

        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_DAK_PUB_ED25519, self._ed25519.dak_pub
        )
        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_DAK_PUB_ED25519_SIGNATURE,
            self._ed25519.dak_sig,
        )
        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_DAK_ED25519_SERIAL, self._ed25519.dak_serial
        )
        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_DAK_PUB_P256R1, self._p256r1.dak_pub
        )
        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_DAK_PUB_P256R1_SIGNATURE,
            self._p256r1.dak_sig,
        )
        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_DAK_P256R1_SERIAL, self._p256r1.dak_serial
        )

        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_PRODUCT_PUB_ED25519,
            self._ed25519.product_pub,
        )
        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_PRODUCT_PUB_ED25519_SIGNATURE,
            self._ed25519.product_sig,
        )
        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_PRODUCT_ED25519_SERIAL,
            self._ed25519.product_serial,
        )
        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_PRODUCT_PUB_P256R1, self._p256r1.product_pub
        )
        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_PRODUCT_PUB_P256R1_SIGNATURE,
            self._p256r1.product_sig,
        )
        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_PRODUCT_P256R1_SERIAL,
            self._p256r1.product_serial,
        )

        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_MAN_PUB_ED25519, self._ed25519.man_pub
        )
        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_MAN_PUB_ED25519_SIGNATURE,
            self._ed25519.man_sig,
        )
        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_MAN_ED25519_SERIAL, self._ed25519.man_serial
        )
        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_MAN_PUB_P256R1, self._p256r1.man_pub
        )
        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_MAN_PUB_P256R1_SIGNATURE,
            self._p256r1.man_sig,
        )
        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_MAN_P256R1_SERIAL, self._p256r1.man_serial
        )

        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_SW_PUB_ED25519, self._ed25519.sw_pub
        )
        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_SW_PUB_ED25519_SIGNATURE,
            self._ed25519.sw_sig,
        )
        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_SW_ED25519_SERIAL, self._ed25519.sw_serial
        )
        self.append(SidMfgValueId.SID_PAL_MFG_STORE_SW_PUB_P256R1, self._p256r1.sw_pub)
        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_SW_PUB_P256R1_SIGNATURE, self._p256r1.sw_sig
        )
        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_SW_P256R1_SERIAL, self._p256r1.sw_serial
        )

        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_AMZN_PUB_ED25519, self._ed25519.root_pub
        )
        self.append(
            SidMfgValueId.SID_PAL_MFG_STORE_AMZN_PUB_P256R1, self._p256r1.root_pub
        )

    def _get_apid_from_aws_device_profile_json(self, _aws_device_profile_json):
        def _get_device_type_id_from_dak(_aws_device_profile_json):
            search_dak = _aws_device_profile_json.Sidewalk.get(
                "DakCertificateMetadata", []
            )
            search_dak += _aws_device_profile_json.Sidewalk.get("DAKCertificate", [])
            for _ in search_dak:
                _device_type_id = _.get("DeviceTypeId", None)
                if _device_type_id:
                    return _device_type_id
            return None

        # Find deviceTypeId in dak_certificate
        _device_type_id = _get_device_type_id_from_dak(_aws_device_profile_json)
        if _device_type_id:
            print(f"DeviceTypeId found in device profile")
            # Get last 4 bytes
            return _device_type_id[-4:]

        # If not maybe older certificate
        _apid = _aws_device_profile_json.Sidewalk.get("ApId", None)
        if _apid:
            print(f"ApId found in device profile")
            return _apid
        return None

    @classmethod
    def from_args(cls, args, pa) -> SidMfgAwsJson:
        config = AttrDict(vars(args).get("config", {}))
        if (args.wireless_device_json and not args.device_profile_json) or (
            args.device_profile_json and not args.wireless_device_json
        ):
            pa.error("Provide both --wireless_device_json and --device_profile_json")

        if (
            not (args.wireless_device_json and args.device_profile_json)
            and not args.certificate_json
        ):
            pa.error(
                "Provide either --wireless_device_json and --device_profile_json or --certificate_json"
            )

        return SidMfgAwsJson(
            aws_wireless_device_json=args.wireless_device_json,
            aws_device_profile_json=args.device_profile_json,
            config=config,
        )


class SidMfgOutBin:
    def __init__(self: SidMfgOutBin, file_name: str, config: Any) -> None:
        self._file_name = file_name
        self._config = config
        self._encoded = bytearray()
        self._resize_encoded()

    def _resize_encoded(self: SidMfgOutBin):
        _encoded_size = self._config.mfg_page_size * self._config.offset_size
        if len(self._encoded) < _encoded_size:
            self._encoded.extend(
                bytearray(b"\xff") * (_encoded_size - len(self._encoded))
            )

    def __enter__(self: SidMfgOutBin) -> SidMfgOutBin:
        path = Path(self._file_name)
        self._file = (
            open(self._file_name, "rb+")
            if path.is_file()
            else open(self._file_name, "wb+")
        )
        self._encoded = bytearray(self._file.read())
        self._resize_encoded()
        return self

    def __exit__(self: SidMfgOutBin, type: Any, value: Any, traceback: Any) -> None:
        self._file.seek(0)
        self._file.write(self._encoded)
        self._file.close()

    @property
    def file_name(self):
        return self._file_name

    def write(self: SidMfgOutBin, sid_mfg: SidMfg) -> None:
        encoded_len = len(self._encoded)
        for _ in sid_mfg:
            if encoded_len <= _.end:
                ex_str = "Cannot fit Field-{} in mfg page, mfg_page_size has to be at least {}".format(
                    _.name, int(_.end / self._config.offset_size) + 1
                )
                raise Exception(ex_str)
            self._encoded[_.start : _.end] = _.encoded

            if encoded_len != len(self._encoded):
                raise Exception("Encoded Length Changed")

    def get_output_bin(self: SidMfgOutBin) -> bytes:
        return self._encoded

    @classmethod
    def from_args(cls, __arg_container__: SidArgOutContainer, args, __pa__):
        return cls(
            config=AttrDict(vars(args).get("config", {})),
            file_name=args.output_bin,
        )


class SidMfgOutHex:
    def __init__(
        self: SidMfgOutHex, file_name: str, config: Any, chip: SidChipAddr
    ) -> None:
        self._file_name = file_name
        self._config = config
        self._encoded = None
        self._chip = chip

    def __enter__(self: SidMfgOutHex) -> SidMfgOutHex:
        self._file = open(self._file_name, "w+")
        return self

    def __exit__(
        self: SidMfgOutHex, __type__: Any, __value__: Any, __traceback__: Any
    ) -> None:
        h = IntelHex()
        h.frombytes(self._encoded, self._chip.offset_addr)
        h.tofile(self._file, "hex")

    @property
    def file_name(self):
        return self._file_name

    def write(self: SidMfgOutHex, sid_mfg: SidMfg) -> None:
        bin = SidMfgOutBin("", self._config)
        bin.write(sid_mfg)
        self._encoded = bin.get_output_bin()

    @classmethod
    def from_args(
        cls, arg_container: SidArgOutContainer, args: argparse.Namespace, __pa__
    ):
        return cls(
            config=AttrDict(vars(args).get("config", {})),
            file_name=args.output_hex,
            chip=arg_container.chip,
        )


def get_default_config_file(
    platform: SidPlatformArgs, __group__: SidInputGroup, __argument__: SidArgument
):
    if platform.config_file:
        _ = Path(__file__).parent / platform.config_file
        return str(_)
    return ""


def get_default_output_file(
    platform: SidPlatformArgs, group: SidInputGroup, argument: SidArgument
):
    return Path.cwd() / Path(
        f"{platform.platform.name.lower()}_{group.name}_CHIP.{argument.ext}"
    )


def is_platform_chip_required(
    platform: SidPlatformArgs, __group__: SidInputGroup, __argument__: SidArgument
) -> bool:
    return len(platform.chips) != 1


def get_default_platform_chip(
    platform: SidPlatformArgs, __group__: SidInputGroup, __argument__: SidArgument
) -> str:
    _ = [_ for _ in platform.chips if _.default]
    if _:
        return _[0].name
    return platform.chips[0].name if platform.chips else "None"


def get_additional_addr_help(
    platform: SidPlatformArgs, __group__: SidInputGroup, __argument__: SidArgument
) -> str:
    test = ""
    for _ in platform.chips:
        test += f"[{_.help_str}]"
    return test


def get_platform_chip_choices(
    platform: SidPlatformArgs, __group__: SidInputGroup, __argument__: SidArgument
) -> list[str]:
    return sorted(list(set(_.name for _ in platform.chips)))


def get_memory_value_choices(
    platform: SidPlatformArgs, __group__: SidInputGroup, __argument__: SidArgument
) -> list[int]:
    return sorted(list(set(_.mem for _ in platform.chips)))


def get_default_memory_value(
    platform: SidPlatformArgs, __group__: SidInputGroup, __argument__: SidArgument
) -> int:
    _ = [_ for _ in platform.chips if _.default]
    if _:
        return _[0].mem
    return platform.chips[0].mem


def valid_json_file(val: str) -> dict:
    if val:
        try:
            json_file = open(val, "r")
        except:
            raise argparse.ArgumentTypeError(f"Opening json file {val} failed !")
        try:
            json_data = json.load(json_file)
            json_data["_SidewalkFileName"] = val
            return json_data
        except:
            raise argparse.ArgumentTypeError(f"Invalid json file {val}")
    else:
        return dict({"_SidewalkFileName": val})


def valid_yaml_file(val: str) -> dict:
    if val:
        try:
            yaml_file = open(val, "r")
        except:
            raise argparse.ArgumentTypeError(f"Opening yaml file {val} failed !")
        try:
            return yaml.safe_load(yaml_file)
        except:
            raise argparse.ArgumentTypeError(f"Invalid yaml file {val}")
    return dict({})


def is_file_or_hex(val):
    _ = Path(val)
    if _.is_file():
        with open(_, "rb") as bin_file:
            bin_data = bin_file.read()
    else:
        bin_data = binascii.unhexlify(val)
    if len(bin_data) != 32:
        raise argparse.ArgumentTypeError("32 byte bin data expected.")
    return bin_data


def auto_int(x) -> int:
    return int(x, 0)
