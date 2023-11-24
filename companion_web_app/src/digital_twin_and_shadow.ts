// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import { GLTFLoader } from 'three/examples/jsm/loaders/GLTFLoader.js';
import { GUI } from 'lil-gui';
import { QRious } from 'qrious';
import * as bootstrap from 'bootstrap'

import { getEnv } from './env';
import { getMqttClient } from './mqtt';
import { mqtt5 } from 'aws-iot-device-sdk-v2';

let mouse_drag = false;

let ledsFolder: GUI;
let sensorFolder: GUI;
let gui: GUI;

let camera: THREE.Camera;
let scene: THREE.Scene;
let renderer: THREE.Renderer;
let model: THREE.Group;
let controls: OrbitControls;
let loader: GLTFLoader;
let display_qr_code: THREE.Mesh;
let display_picture: THREE.Mesh;

let renderRequested = false;

let desired = {};

const PICTURES = [
    'bear',
    'day1',
    'employees',
    'toy',
    'aws_logo',
    'none',
]

const LED_1_COLOR = 0xD2033E;
const LED_2_COLOR = 0xF8C40D;
const LED_3_COLOR = 0xF0961A;

let buttons = {
    button_1: { last_pressed: 0, visibility: 0, model: null },
    button_2: { last_pressed: 0, visibility: 0, model: null },
    button_3: { last_pressed: 0, visibility: 0, model: null },
    button_4: { last_pressed: 0, visibility: 0, model: null },
}

const fadeOutButtonIndicatorDelay = 50;

let ledMaterials = {
    led_1: null,
    led_2: null,
    led_3: null,
}
let ledLights = {
    led_1: null,
    led_2: null,
    led_3: null,
    user_led: null,
}

// names have to match the reported keys in the shadow document
let guiValues = {
    send_sensor_data: { value: false },
    model_rotation: { value: false },
    user_led: { value: 'off' },
    led_1: { value: LED_1_COLOR },
    led_2: { value: LED_2_COLOR },
    led_3: { value: LED_3_COLOR },
    led_brightness: { value: 15 },
    display_brightness: { value: 100 },
    qr_code: { value: '(no URL)' },
    picture: { value: 'bear' },
    temperature: { value: '(no data yet)' },
    humidity: { value: '(no data yet)' },
    ambient_light: { value: '(no data yet)' },
    acceleration_x: { value: '(no data yet)' },
    acceleration_y: { value: '(no data yet)' },
    acceleration_z: { value: '(no data yet)' },
    angular_velocity_x: { value: '(no data yet)' },
    angular_velocity_y: { value: '(no data yet)' },
    angular_velocity_z: { value: '(no data yet)' },
    button_1: { value: '(no data yet)' },
    button_2: { value: '(no data yet)' },
    button_3: { value: '(no data yet)' },
    button_4: { value: '(no data yet)' },
}

const SHADOW_DESIRED_DEBOUNCE = 750;
let shadowDesiredStateTimerIds = {
    qr_code: null,
    user_led: null,
    led_1: null,
    led_2: null,
    led_3: null,
    led_brightness: null,
    display_brightness: null,
}

export async function initDigitalTwinAndShadow(writeToSerialPort) {
    document.getElementById('run_digital_twin_and_shadow')?.addEventListener('click', async () => {
        writeToSerialPort(`\r\nworkshop run digital_twin_and_shadow\r\n`);
    });

    document.getElementById('open_digital_twin_and_shadow')?.addEventListener('click', async () => {
        const digitalTwinAndShadowModal = new bootstrap.Modal('#digital_twin_and_shadow_modal', {});
        digitalTwinAndShadowModal.show();
        setTimeout(requestRenderIfNotRequested, 500);
    });

    initScene();
    render();
}

export function digital_twin_and_shadow_messageReceived(eventData: mqtt5.MessageReceivedEvent) {
    const topic = eventData.message.topicName;
    if (topic == `$aws/things/${getEnv().thing_name}/shadow/get/accepted`) {
        const p = JSON.parse(eventData.message.payload!.toString());
        handleIncomingShadowUpdate(p); // successful response
        if (!('reported' in p.state)) {
            setDefaultDesiredState();
        } else {
            if (!('display_brightness' in p.state.reported)) {
                updateShadow('display_brightness', guiValues.display_brightness.value);
            }
            if (!('picture' in p.state.reported)) {
                updateShadow('picture', guiValues.picture.value);
            }
            if (!('user_led' in p.state.reported)) {
                updateShadow('user_led', guiValues.user_led.value);
            }
        }
    } else if (topic == `$aws/things/${getEnv().thing_name}/shadow/get/rejected`) {
        const p = JSON.parse(eventData.message.payload!.toString());
        if (p.code == 404) {
            console.log("Shadow not found, setting default desired state.");
            setDefaultDesiredState();
        } else {
            console.log("shadow get rejected:", eventData);
        }
    } else if (topic == `$aws/things/${getEnv().thing_name}/shadow/update/accepted`) {
        const p = JSON.parse(eventData.message.payload!.toString());
        try {
            handleIncomingShadowUpdate(p);
        } catch (e) {
            console.log(e);
        }
    } else if (topic == `$aws/things/${getEnv().thing_name}/shadow/update/rejected`) {
        const p = JSON.parse(eventData.message.payload!.toString());
        console.log("shadow update rejected:", p);
    } else if (topic == 'demo_badge/sensors') {
        const p = JSON.parse(eventData.message.payload!.toString());
        try {
            handleSensorData(p);
        } catch (e) {
            console.log(e);
        }
    }
}

export async function getInitialShadowDoc() {
    await getMqttClient().publish({
        topicName: `$aws/things/${getEnv().thing_name}/shadow/get`,
        qos: mqtt5.QoS.AtLeastOnce,
        payload: "",
    });
}

async function setDefaultDesiredState() {
    await getMqttClient().publish({
        topicName: `$aws/things/${getEnv().thing_name}/shadow/update`,
        qos: mqtt5.QoS.AtLeastOnce,
        payload: JSON.stringify({
            state: {
                desired: {
                    picture: guiValues.picture.value,
                    user_led: guiValues.user_led.value,
                    led_brightness: guiValues.led_brightness.value,
                    display_brightness: guiValues.display_brightness.value,
                    led_1: guiValues.led_1.value,
                    led_2: guiValues.led_2.value,
                    led_3: guiValues.led_3.value,
                },
            }
        }),
    });
}

async function updateShadow(short_name: string, value: string | boolean | number) {
    console.log(`update shadow with new desired state: ${short_name} = ${value}`);

    let payload = {
        state: {
            desired: {}
        }
    };
    payload.state.desired[short_name] = value;
    desired[short_name] = value;

    await getMqttClient().publish({
        topicName: `$aws/things/${getEnv().thing_name}/shadow/update`,
        qos: mqtt5.QoS.AtLeastOnce,
        payload: JSON.stringify(payload),
    });
}

function handleIncomingShadowUpdate(payload) {
    if (!('state' in payload) || !('reported' in payload.state)) {
        return;
    }

    Object.keys(buttons).forEach(b => {
        if (payload.state.reported.hasOwnProperty(b)) {
            const timestamp = payload.state.reported[b];
            if (timestamp > buttons[b].last_pressed) {
                buttons[b].last_pressed = timestamp;
                buttons[b].visibility = 40;
                buttons[b].model.material.opacity = 1.0;
                buttons[b].model.visible = true;
                setTimeout(fadeOutButtonIndicator, fadeOutButtonIndicatorDelay, buttons[b]);
            } else if (payload.state.reported[b] == 0) {
                buttons[b].last_pressed = 0;
                buttons[b].visibility = 0;
                buttons[b].model.visible = false;
            }
        }
    });

    let valueChanged = false;
    Object.keys(guiValues).forEach(k => {
        if (!(k in payload.state.reported)) {
            return; // skip updating GUI if no value reported
        }

        let newValue = '';
        newValue = payload.state.reported[k];

        if (k == 'led_1' || k == 'led_2' || k == 'led_3') {
            updateLEDColors(k, newValue);
        }

        if (k in desired) {
            if (desired[k] != payload.state.reported[k]) {
                return; // skip updating GUI with pending desired values
            } else {
                delete desired[k];
                if (k == "picture") {
                    updateGuiDisplayPicture(newValue);
                } else if (k == "qr_code") {
                    updateGuiDisplayQRCode(newValue);
                }
            }
        }
        if (k == 'picture' && !display_picture.visible) {
            updateGuiDisplayPicture(newValue);
        }

        // raw sensor value that cannot be changed from the cloud
        if (newValue != guiValues[k].value) {
            guiValues[k].value = newValue;
            valueChanged = true;
        }
    });

    // render blinking locally and reliably
    if (ledLights.user_led != null && 'setMode' in ledLights.user_led) {
        ledLights.user_led.setMode(guiValues.user_led.value);
    }

    if (valueChanged) {
        requestRenderIfNotRequested();
    }
}

function handleSensorData(payload) {
    const gravity = 9.80665;

    if (payload.hasOwnProperty('temperature')) {
        let c = payload.temperature;
        let f = c * 9 / 5 + 32;
        const v = c.toFixed(1) + " °C | " + f.toFixed(0) + " °F";
        guiValues.temperature.value = v
    }
    if (payload.hasOwnProperty('humidity')) {
        guiValues.humidity.value = payload.humidity.toFixed(0) + "%";
    }
    if (payload.hasOwnProperty('light')) {
        guiValues.ambient_light.value = payload.light.toFixed(0);
    }
    if (payload.hasOwnProperty('acceleration_x')) {
        const acc = payload.acceleration_x;
        const v = (acc / gravity).toFixed(1) + " g | " + acc.toFixed(1) + " m/s^2";
        guiValues.acceleration_x.value = v;
    }
    if (payload.hasOwnProperty('acceleration_y')) {
        const acc = payload.acceleration_y;
        const v = (acc / gravity).toFixed(1) + " g | " + acc.toFixed(1) + " m/s^2";
        guiValues.acceleration_y.value = v;
    }
    if (payload.hasOwnProperty('acceleration_z')) {
        const acc = payload.acceleration_z;
        const v = (acc / gravity).toFixed(1) + " g | " + acc.toFixed(1) + " m/s^2";
        guiValues.acceleration_z.value = v;
    }
    if (payload.hasOwnProperty('angular_velocity_x')) {
        const v = payload.angular_velocity_x.toFixed(3) + " deg/s";
        guiValues.angular_velocity_x.value = v;
    }
    if (payload.hasOwnProperty('angular_velocity_y')) {
        const v = payload.angular_velocity_y.toFixed(3) + " deg/s";
        guiValues.angular_velocity_y.value = v;
    }
    if (payload.hasOwnProperty('angular_velocity_z')) {
        const v = payload.angular_velocity_z.toFixed(3) + " deg/s";
        guiValues.angular_velocity_z.value = v;
    }

    if (guiValues.model_rotation.value) {
        const x = payload.acceleration_x;
        const y = payload.acceleration_y;
        const z = payload.acceleration_z;
        const roll = Math.atan2(y, z);
        const pitch = Math.atan2((- x), Math.sqrt(y * y + z * z));
        model.rotation.x = -(roll + Math.PI / 2);
        model.rotation.y = -pitch + Math.PI;
        model.rotation.z = Math.PI;
    }

    requestRenderIfNotRequested();
}

function initScene() {
    scene = new THREE.Scene();
    scene.background = new THREE.Color('rgb(160, 160, 160)');

    camera = new THREE.PerspectiveCamera(60, window.innerWidth / window.innerHeight, 0.1, 1e10);
    // camera.position.set(30, 30, -90);
    camera.position.set(0, 0, -90);
    scene.add(camera);

    // light

    const hemiLight = new THREE.HemisphereLight(0xffffff, 0x000000, 2);
    scene.add(hemiLight);

    const dirLight = new THREE.DirectionalLight(0xffffff, 1.5);
    dirLight.position.set(0, 0, 1000);
    scene.add(dirLight);

    const backDirLight = new THREE.DirectionalLight(0xffffff, 1.5);
    backDirLight.position.set(0, 0, -1000);
    scene.add(backDirLight);

    // models

    loader = new GLTFLoader();
    loader.load("model.glb", function (gltf) {
        model = new THREE.Group();
        model.add(gltf.scene);
        model.position.set(0, 0, 0);
        scene.add(model);
        augmentModel();
        buildGUI();
        requestRenderIfNotRequested();
        document.getElementById("loading")!.textContent = "";
    },
        (progress) => {
            if (progress.loaded == progress.total) {
                document.getElementById("loading")!.textContent = "Rendering...";
            } else if (progress.loaded < progress.total) {
                let v = (progress.loaded / progress.total) * 100;
                let n = v.toFixed(0);
                document.getElementById("loading")!.textContent = "Loading... " + n + "%";
            }
        });

    // renderer

    const container : HTMLElement = document.querySelector('#digital_twin_and_shadow_modal .modal-body')!;

    renderer = new THREE.WebGLRenderer();
    renderer.setPixelRatio(window.devicePixelRatio);
    container.appendChild(renderer.domElement);
    resizeRendererToDisplaySize();

    // controls

    controls = new OrbitControls(camera, renderer.domElement);
    controls.minDistance = 15;
    controls.maxDistance = 140;
    controls.enableDamping = true;
    controls.addEventListener('change', requestRenderIfNotRequested);
    controls.update();

    container.addEventListener('mousedown', () => {
        mouse_drag = false;
    });
    container.addEventListener('mousemove', () => {
        mouse_drag = true;
    });
    container.addEventListener('mouseup', () => {
        if (!mouse_drag) {
            controls.reset();
        }
    });

    window.addEventListener('resize', () => {
        requestRenderIfNotRequested();
    });

    render();
}

function augmentModel() {
    function buildSmartLED(short_name: string, initialColor: THREE.ColorRepresentation, x: number, y: number, z: number) {
        ledMaterials[short_name] = new THREE.MeshBasicMaterial({ color: initialColor });
        const geometry = new THREE.SphereGeometry(2, 10, 10);
        const smartLED = new THREE.Mesh(geometry, ledMaterials[short_name]);
        smartLED.position.set(x, y, z);
        model.add(smartLED);

        const light = new THREE.PointLight(initialColor, 1);
        light.position.set(x, y, z);
        model.add(light);
        ledLights[short_name] = light;
    }
    buildSmartLED('led_1', LED_3_COLOR, -29.5, -8, -2.5);
    buildSmartLED('led_2', LED_2_COLOR, -25.5, -12, -2.5);
    buildSmartLED('led_3', LED_1_COLOR, -21.5, -16, -2.5);

    const userLEDposition = { x: 3.5, y: -25, z: -1.5 };
    const userLED = new THREE.Mesh(
        new THREE.SphereGeometry(1, 10, 10),
        new THREE.MeshBasicMaterial({ color: 0x00FF00, opacity: 0.5 })
    );
    userLED.position.set(userLEDposition.x, userLEDposition.y, userLEDposition.z);
    model.add(userLED);

    const userLEDintensity = 2;
    const userLEDlight = new THREE.PointLight(0x00FF00, userLEDintensity);
    userLEDlight.position.set(userLEDposition.x, userLEDposition.y, userLEDposition.z);
    model.add(userLEDlight);
    ledLights['user_led'] = userLEDlight;

    userLEDlight.mode = '';
    userLEDlight.setMode = function (mode: string) {
        if (mode == 'on' && userLEDlight.mode != 'on') {
            clearInterval(userLEDlight.blinkIntervalId);
            userLEDlight.mode = 'on';
            userLEDlight.intensity = userLEDintensity;
            userLED.visible = true;
        } else if (mode == 'blinking' && userLEDlight.mode != 'blinking') {
            userLEDlight.mode = 'blinking';
            userLEDlight.blinkIntervalId = setInterval(function () {
                if (guiValues.user_led.value == 'blinking') {
                    if (userLED.visible) {
                        userLEDlight.intensity = 0;
                        userLED.visible = false;
                    } else {
                        userLEDlight.intensity = userLEDintensity;
                        userLED.visible = true;
                    }
                    requestRenderIfNotRequested();
                }
            }, 250);
        } else if (mode == 'off' && userLEDlight.mode != 'off') {
            clearInterval(userLEDlight.blinkIntervalId);
            userLEDlight.mode = 'off';
            userLEDlight.intensity = 0;
            userLED.visible = false;
        }
        requestRenderIfNotRequested();
    }

    function buildButton(short_name: string, x: number, y: number, z: number) {
        buttons[short_name].model = new THREE.Mesh(
            new THREE.SphereGeometry(3, 10, 10),
            new THREE.MeshBasicMaterial({ color: 0xFF9900, transparent: true, opacity: 0.5 })
        );
        buttons[short_name].model.position.set(x, y, z);
        buttons[short_name].model.visible = false;
        model.add(buttons[short_name].model);
    }
    buildButton('button_1', -1, -25.5, -2);
    buildButton('button_2', -7.5, -29.5, -2);
    buildButton('button_3', -1, -33.5, -2);
    buildButton('button_4', 5, -29.5, -2);

    display_qr_code = new THREE.Mesh(new THREE.PlaneGeometry(26, 26), new THREE.MeshBasicMaterial());
    display_qr_code.position.set(-1, 20, -3.3);
    display_qr_code.rotation.y = Math.PI;
    display_qr_code.visible = false;
    model.add(display_qr_code);

    display_picture = new THREE.Mesh(new THREE.PlaneGeometry(26, 26), new THREE.MeshBasicMaterial());
    display_picture.position.set(-1, 20, -3.3);
    display_picture.rotation.y = Math.PI;
    display_picture.visible = false;
    model.add(display_picture);
}

function buildGUI() {
    const container: HTMLElement = document.querySelector('#digital_twin_and_shadow_modal .modal-body')!;
    gui = new GUI({ autoPlace: false, container: container });

    ledsFolder = gui.addFolder('RGB Smart LEDs');

    ledsFolder.add(guiValues.led_brightness, 'value', 0, 100, 5).name('Brightness').onFinishChange(function (value) {
        clearTimeout(shadowDesiredStateTimerIds.led_brightness);
        shadowDesiredStateTimerIds.led_brightness = setTimeout(updateShadow, SHADOW_DESIRED_DEBOUNCE, 'led_brightness', value);

        ledLights.led_1.intensity = value / 10.0;
        ledLights.led_2.intensity = value / 10.0;
        ledLights.led_3.intensity = value / 10.0;
        requestRenderIfNotRequested();
    });

    for (let i = 1; i < 4; i++) {
        const short_name = `led_${i}`;
        const name = `LED ${i}`;
        ledsFolder.addColor(guiValues[short_name], 'value').name(name).onChange(function (color) {
            clearTimeout(shadowDesiredStateTimerIds[short_name]);
            shadowDesiredStateTimerIds[short_name] = setTimeout(updateShadow, SHADOW_DESIRED_DEBOUNCE, short_name, color);
            updateLEDColors(short_name, color);
        });
    }

    const userLEDFolder = gui.addFolder('User LED');
    userLEDFolder.add(guiValues.user_led, 'value', ['on', 'blinking', 'off']).name('User LED').onChange(function (value) {
        clearTimeout(shadowDesiredStateTimerIds.user_led);
        shadowDesiredStateTimerIds.user_led = setTimeout(updateShadow, SHADOW_DESIRED_DEBOUNCE, 'user_led', value);
        requestRenderIfNotRequested();
    });

    const displayFolder = gui.addFolder('Display');
    displayFolder.add(guiValues.qr_code, 'value').name('QR Code').onFinishChange(function (value) {
        clearTimeout(shadowDesiredStateTimerIds.qr_code);
        shadowDesiredStateTimerIds.qr_code = setTimeout(updateShadow, SHADOW_DESIRED_DEBOUNCE, 'qr_code', value);
        requestRenderIfNotRequested();
    });
    displayFolder.add(guiValues.picture, 'value', PICTURES).name('Sample Picture').onFinishChange(function (value) {
        clearTimeout(shadowDesiredStateTimerIds.picture);
        shadowDesiredStateTimerIds.picture = setTimeout(updateShadow, SHADOW_DESIRED_DEBOUNCE, 'picture', value);
        requestRenderIfNotRequested();
    });
    displayFolder.add(guiValues.display_brightness, 'value', 0, 100, 5).name('Brightness').onFinishChange(function (value) {
        clearTimeout(shadowDesiredStateTimerIds.display_brightness);
        shadowDesiredStateTimerIds.display_brightness = setTimeout(updateShadow, SHADOW_DESIRED_DEBOUNCE, 'display_brightness', value);
        requestRenderIfNotRequested();
    });

    sensorFolder = gui.addFolder('Sensors');
    sensorFolder.add(guiValues.model_rotation, 'value').name('Model Rotation').onFinishChange(function (value) {
        controls.reset();
        model.rotation.x = 0;
        model.rotation.y = 0;
        model.rotation.z = 0;
        if (value) {
            ledsFolder.close();
            userLEDFolder.close();
            displayFolder.close();

            guiValues.send_sensor_data.value = true;
            updateShadow('send_sensor_data', guiValues.send_sensor_data.value);
            updateSensorDataUI();
        }
    });
    sensorFolder.add(guiValues.send_sensor_data, 'value').name('Subscribe').onFinishChange(function (value) {
        updateShadow('send_sensor_data', value);
        updateSensorDataUI();
    });
    sensorFolder.add(guiValues.temperature, 'value').name('Temperature').hide();
    sensorFolder.add(guiValues.humidity, 'value').name('Humidity').hide();
    sensorFolder.add(guiValues.ambient_light, 'value').name('Ambient Light').hide();
    sensorFolder.add(guiValues.acceleration_x, 'value').name('Acceleration X').hide();
    sensorFolder.add(guiValues.acceleration_y, 'value').name('Acceleration Y').hide();
    sensorFolder.add(guiValues.acceleration_z, 'value').name('Acceleration Z').hide();
    sensorFolder.add(guiValues.angular_velocity_x, 'value').name('Angular Velocity X').hide();
    sensorFolder.add(guiValues.angular_velocity_y, 'value').name('Angular Velocity Y').hide();
    sensorFolder.add(guiValues.angular_velocity_z, 'value').name('Angular Velocity Z').hide();

    const buttonsFolder = gui.addFolder('Buttons');
    buttonsFolder.add(guiValues.button_1, 'value').name('Button 1');
    buttonsFolder.add(guiValues.button_2, 'value').name('Button 2');
    buttonsFolder.add(guiValues.button_3, 'value').name('Button 3');
    buttonsFolder.add(guiValues.button_4, 'value').name('Button 4');

    render()
}

function updateLEDColors(short_name: string, color: THREE.ColorRepresentation) {
    ledMaterials[short_name].color.set(color);
    ledLights[short_name].color.setHex(color);
    requestRenderIfNotRequested();
}

function updateSensorDataUI() {
    if (guiValues.send_sensor_data.value) {
        sensorFolder.controllers.forEach(element => {
            if (element != sensorFolder.children[0]) {
                element.show();
            }
        });
    } else {
        sensorFolder.controllers.forEach(element => {
            if (element != sensorFolder.children[0]) {
                element.hide();
            }
        });
    }
    requestRenderIfNotRequested();
}

function fadeOutButtonIndicator(button) {
    button.visibility -= 1;
    if (button.visibility <= 0) {
        button.model.visible = false;
    } else {
        button.model.material.opacity = button.visibility / 10.0;
        setTimeout(fadeOutButtonIndicator, fadeOutButtonIndicatorDelay, button);
    }
    requestRenderIfNotRequested();
}

function updateGuiDisplayQRCode(value: string) {
    if (value == '') {
        display_qr_code.visible = false;
    } else {
        const canvas = document.createElement('canvas');
        canvas.width = 240;
        canvas.height = 240;
        document.body.appendChild(canvas);

        const qr = new QRious();
        qr.value = value; //guiValues['qr_code'].value;
        qr.size = 240;

        const manager = new THREE.LoadingManager()
        const texture = new THREE.TextureLoader(manager).load(qr.toDataURL());
        display_qr_code.material = new THREE.MeshBasicMaterial({ map: texture });
        display_qr_code.visible = true;
        display_picture.visible = false;
    }
}

function updateGuiDisplayPicture(value: string) {
    if (value == 'none') {
        display_picture.visible = false;
    } else {
        const loader = new THREE.TextureLoader();
        const texture = loader.load(`pictures/${value}.bmp`);
        display_picture.material = new THREE.MeshBasicMaterial({ map: texture });
        display_picture.visible = true;
        display_qr_code.visible = false;
    }
}

function render() {
    renderRequested = false;

    if (resizeRendererToDisplaySize()) {
        const canvas = renderer.domElement;
        camera.aspect = canvas.clientWidth / canvas.clientHeight;
        camera.updateProjectionMatrix();
    }

    try {
        gui.controllersRecursive().forEach(controller => {
            controller.updateDisplay();
        });
    } catch {
        // ignore
    }

    controls.update(); // to support damping
    renderer.render(scene, camera);
}

export function requestRenderIfNotRequested() {
    if (!renderRequested) {
        renderRequested = true;
        requestAnimationFrame(render);
    }
}

function resizeRendererToDisplaySize() {
    const factor = window.devicePixelRatio;
    const canvas = renderer.domElement;
    let width = canvas.parentElement!.offsetWidth / factor;
    let height = canvas.parentElement!.offsetHeight / factor;
    const needResize = canvas.width !== width || canvas.height !== height;
    if (needResize) {
        renderer.setSize(width, height, false);
    }
    return needResize;
}
