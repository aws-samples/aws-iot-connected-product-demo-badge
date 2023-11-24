// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

// Import our custom CSS
import './scss/styles.scss'

// Import all of Bootstrap's JS
import * as bootstrap from 'bootstrap'

import { Terminal } from 'xterm';
import { FitAddon } from 'xterm-addon-fit';
import { WebLinksAddon } from 'xterm-addon-web-links';
import 'xterm/css/xterm.css';

import { getEnv, initEnv } from './env';
import { initMqtt } from './mqtt';

import { initMqttDeviceOnboarding } from './mqtt_device_onboarding';
import { initMqttPubSub } from './mqtt_pub_sub';
import { initSensorDataIngestion } from './sensor_data_ingestion';
import { initDeviceLocation } from './device_location';
import { initDigitalTwinAndShadow } from './digital_twin_and_shadow';
import { initImageTransfer } from './image_transfer';
import { initSidewalk } from './sidewalk';

let serialAutoConnect = false;
let firstSerialConnect = true;

type SerialOutputParser = {
    ringBuffer: string,
    certificate?: string
}
const serialOutputParser: SerialOutputParser = { ringBuffer: "" };

let connectButton: HTMLButtonElement;

const bufferSize = 8 * 1024; // 8kB
const serialPortOptions: SerialOptions = {
    baudRate: 115200,
    dataBits: 8,
    parity: 'none',
    stopBits: 1,
    flowControl: 'none',
    bufferSize: bufferSize,

    // Prior to Chrome 86 these names were used.
    baudrate: 115200,
    databits: 8,
    stopbits: 1,
    rtscts: false,
};
let port: SerialPort | undefined;
let reader: ReadableStreamDefaultReader | ReadableStreamBYOBReader | undefined;
const term = new Terminal({
    scrollback: 10_000,
});

const fitAddon = new FitAddon();
term.loadAddon(fitAddon);
term.loadAddon(new WebLinksAddon());
term.onData((data) => writeToSerialPort(data));

const encoder = new TextEncoder();

function writeToSerialPort(data: any) {
    if (port?.writable == null) {
        console.warn(`unable to find writable port`);
        return;
    }

    const writer = port.writable.getWriter();
    writer.write(encoder.encode(data));
    writer.releaseLock();
}

function clearSerialConsole(): void {
    if (!term) {
        throw new Error('no serial console found');
    }

    if (term.rows === 0) {
        return;
    }

    term.clear();
}

const sections = {
    "<inf> primary_thread: Running mqtt_pub_sub...": "#collapseMQTTPubSub",
    "<inf> primary_thread: Running device_location...": "#collapseDeviceLocation",
    "<inf> primary_thread: Running sensor_data_ingestion...": "#collapseSensorDataIngestion",
    "<inf> primary_thread: Running digital_twin_and_shadow...": "#collapseDigitalTwinAndShadow",
    "<inf> primary_thread: Running image_transfer...": "#collapseImageTransfer",
    "<inf> primary_thread: Running sidewalk...": "#collapseSidewalk",
}
const maxSectionLength = Math.max(...Object.keys(sections).map((e) => { return e.length; }));

function openCollapseSection(section: string) {
    const button = document.querySelector(`button.collapsed[data-bs-target="${section}"]`) as HTMLButtonElement;
    if (button) {
        button.click();
    }
}

function parseSerialOutput(value: Uint8Array) {
    serialOutputParser.ringBuffer += new TextDecoder().decode(value);

    const BEGIN = "-----BEGIN CERTIFICATE-----";
    const END = "-----END CERTIFICATE-----";
    const start = serialOutputParser.ringBuffer.search(BEGIN);
    if (start >= 0) {
        const end = serialOutputParser.ringBuffer.search(END);
        if (end >= (start + BEGIN.length)) {
            getEnv().certificate = serialOutputParser.ringBuffer.substring(start, end + END.length);
            console.log("ExpressLink certificate found!");
            console.log(getEnv().certificate);
            serialOutputParser.ringBuffer = "";

            const a = document.createElement("a");
            a.href = "data:application/octet-stream;base64," + btoa(getEnv().certificate);
            a.download = "demo_badge_certificate.pem";
            a.click();

            document.getElementById('expresslink_certificate_spinner')?.classList.add('d-none');
        }
    } else if (serialOutputParser.ringBuffer.includes("\n") && serialOutputParser.ringBuffer.length > 256) {
        serialOutputParser.ringBuffer = serialOutputParser.ringBuffer.substring(serialOutputParser.ringBuffer.length - 256);
    }

    if (getEnv().thing_name === null || getEnv().thing_name.length == 0 || serialOutputParser.ringBuffer.includes("AT+CONF? ThingName")) {
        const pos = serialOutputParser.ringBuffer.lastIndexOf("AT+CONF? ThingName");
        const m = serialOutputParser.ringBuffer.substring(pos).match("OK ([a-z0-9]{8}-[a-z0-9]{4}-[a-z0-9]{4}-[a-z0-9]{4}-[a-z0-9]{12})");
        if (m && m?.length == 2) {
            getEnv().thing_name = m[1];
        }
    }

    if (serialOutputParser.ringBuffer.length > maxSectionLength*3) {
        const t = serialOutputParser.ringBuffer.substring(serialOutputParser.ringBuffer.length - maxSectionLength*3);
        for (const [key, value] of Object.entries(sections)) {
            if (t.includes(key)) {
                openCollapseSection(value);
                break;
            }
        }
    }
}

async function connectToSerialPort(): Promise<void> {
    fitAddon.fit();

    if (port === undefined) {
        try {
            port = await navigator.serial.requestPort({});
        } catch (e) {
            console.log(e);
            return;
        }
    }

    try {
        await port.open(serialPortOptions);
        markConnected();
    } catch (e) {
        console.error(e);
        if (e instanceof Error) {
            term.writeln(`<ERROR: ${e.message}>`);
        }
        markDisconnected();
        return;
    }

    while (port && port.readable) {
        try {
            try {
                reader = port.readable.getReader({ mode: 'byob' });
            } catch {
                reader = port.readable.getReader();
            }

            let buffer = null;
            for (; ;) {
                const { value, done } = await (async () => {
                    if (reader instanceof ReadableStreamBYOBReader) {
                        if (!buffer) {
                            buffer = new ArrayBuffer(bufferSize);
                        }
                        const { value, done } = await reader.read(new Uint8Array(buffer, 0, bufferSize));
                        buffer = value?.buffer;
                        return { value, done };
                    } else {
                        return await reader.read();
                    }
                })();

                if (value) {
                    parseSerialOutput(value);

                    await new Promise<void>((resolve) => {
                        term.write(value, resolve);
                    });
                }
                if (done) {
                    break;
                }
            }
        } catch (e) {
            console.error(e);
        } finally {
            if (reader) {
                reader.releaseLock();
                reader = undefined;
            }
        }
    }

    if (port) {
        try {
            await port.close();
        } catch (e) {
            console.error(e);
        }

        markDisconnected();
    }
}

async function disconnectFromSerialPort(): Promise<void> {
    const localPort = port;
    port = undefined;

    if (reader) {
        await reader.cancel();
    }

    if (localPort) {
        try {
            await localPort.close();
        } catch (e) {
            console.error(e);
            if (e instanceof Error) {
                term.writeln(`<ERROR: ${e.message}>`);
            }
        }
    }

    serialAutoConnect = false;

    markDisconnected();
}

function markConnected(): void {
    serialAutoConnect = true;

    if (firstSerialConnect) {
        firstSerialConnect = false;
        clearSerialConsole();
        writeToSerialPort('\r\ninfo\r\n');
    }

    term.writeln('<CONNECTED>');
    writeToSerialPort('\r\n');

    connectButton.textContent = 'Disconnect';
    connectButton.classList.remove('btn-primary');
    connectButton.classList.add('btn-danger');

    document.querySelectorAll('button.connection-dependant').forEach((e) => { (e as HTMLButtonElement).disabled = false })
}

function markDisconnected(): void {
    term.writeln('\r\n<DISCONNECTED>');
    port = undefined;

    document.querySelectorAll('button.connection-dependant').forEach((e) => { (e as HTMLButtonElement).disabled = true })

    connectButton.textContent = 'Connect Device...';
    connectButton.classList.remove('btn-danger');
    connectButton.classList.add('btn-primary');
}

async function initSerialConsole() {
    const terminalElement = document.getElementById('serial_console');
    if (terminalElement) {
        term.open(terminalElement);
        fitAddon.fit();
        term.writeln("\r\n--- Click on 'Connect device...' to get started! ---");

        window.addEventListener('resize', () => {
            fitAddon.fit();
        });
    }

    navigator.serial.addEventListener('connect', (event) => {
        if (serialAutoConnect) {
            port = event.target as SerialPort;
            connectToSerialPort(); // automatically connect if possible and `port` is still set from a previous connection
        }
    });

    document.getElementById('serial_console_clear')?.addEventListener('click', clearSerialConsole);

    connectButton = document.getElementById('serial_console_connect') as HTMLButtonElement;
    connectButton.addEventListener('click', () => {
        if (port) {
            disconnectFromSerialPort();
        } else {
            connectToSerialPort();
        }
    });
    markDisconnected();
}

function injectButtonIcons(className: string, iconName: string) {
    document.querySelectorAll(className).forEach((e) => {
        const icon = document.createElement('i');
        icon.classList.add('bi', iconName, 'me-1');
        e.prepend(icon);
    });

}

document.addEventListener('DOMContentLoaded', async () => {
    if (!('serial' in navigator)) {
        console.error("Web Serial is not available in this browser!");
        const browserNotSupportedModal = new bootstrap.Modal('#browser_not_supported_modal', {});
        browserNotSupportedModal.show();
        return;
    }

    const loadingModal = new bootstrap.Modal('#loading_modal', {});
    loadingModal.show();

    injectButtonIcons('.btn-device-command', 'bi-cpu');
    injectButtonIcons('.btn-cloud-api', 'bi-cloud');

    await initSerialConsole();

    await initEnv();

    await initMqtt();

    await initMqttDeviceOnboarding(writeToSerialPort);
    await initMqttPubSub(writeToSerialPort);
    await initSensorDataIngestion(writeToSerialPort);
    await initDeviceLocation(writeToSerialPort);
    await initImageTransfer(writeToSerialPort);
    await initDigitalTwinAndShadow(writeToSerialPort);
    await initSidewalk(writeToSerialPort);

    setTimeout(() => { loadingModal.hide(); }, 1000);
});
