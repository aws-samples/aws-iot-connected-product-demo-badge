// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

import { S3Client, PutObjectCommand } from "@aws-sdk/client-s3";
import { getEnv, getClientCreds } from "./env";
import { CognitoIdentityClient } from "@aws-sdk/client-cognito-identity";
import { fromCognitoIdentityPool } from "@aws-sdk/credential-provider-cognito-identity";
import { getIoTClient } from "./aws_access";
import { CreateJobCommand, CreateStreamCommand, TargetSelection } from "@aws-sdk/client-iot";
import { v4 as uuid } from "uuid";

const imageHeight = 240;
const imageWidth = 240;
let imageData: Uint16Array;

let liveVideo: HTMLVideoElement;
let stillFrame: HTMLCanvasElement;

let startCameraButton: HTMLButtonElement;
let takeImageButton: HTMLButtonElement;
let uploadImageToS3Button: HTMLButtonElement;
let uploadImageToS3Spinner: HTMLElement;
let createOTAJobButton: HTMLButtonElement;
let createOTAJobSpinner: HTMLElement;
let startOverButton: HTMLButtonElement;

let uploadSuccessfulAlert: HTMLDivElement;
let jobCreatedAlert: HTMLDivElement;

export async function initImageTransfer(writeToSerialPort) {
    document.getElementById('run_image_transfer')?.addEventListener('click', async () => {
        writeToSerialPort(`\r\nworkshop run image_transfer\r\n`);
    });

    startCameraButton = document.getElementById('start_camera')! as HTMLButtonElement;
    takeImageButton = document.getElementById('take_image')! as HTMLButtonElement;
    uploadImageToS3Button = document.getElementById('upload_to_s3')! as HTMLButtonElement;
    uploadImageToS3Spinner = document.getElementById('upload_to_s3_spinner')! as HTMLElement;
    createOTAJobButton = document.getElementById('create_ota_job')! as HTMLButtonElement;
    createOTAJobSpinner = document.getElementById('create_ota_job_spinner')! as HTMLElement;
    startOverButton = document.getElementById('start_over_image_transfer')! as HTMLButtonElement;

    liveVideo = document.getElementById('live_video')! as HTMLVideoElement;
    stillFrame = document.getElementById('still_frame')! as HTMLCanvasElement;
    liveVideo.addEventListener('playing', () => {
        liveVideo.classList.remove('spinner-grow');
        takeImageButton.disabled = false;
    })

    uploadSuccessfulAlert = document.getElementById('upload_successful')! as HTMLDivElement;
    jobCreatedAlert = document.getElementById('job_created')! as HTMLDivElement;

    takeImageButton.disabled = true;
    uploadImageToS3Button.disabled = true;
    createOTAJobButton.disabled = true;

    const cameraDeviceSelect = document.getElementById('camera_device')! as HTMLSelectElement;
    cameraDeviceSelect.addEventListener('change', () => {
        startCamera(cameraDeviceSelect);
    });
    await getCameraSelection(cameraDeviceSelect);

    startCameraButton.addEventListener('click', async () => {
        startCameraButton.disabled = true;
        document.getElementById('camera_source')?.classList.remove('d-none');
        liveVideo.classList.add('stream_active');
        liveVideo.classList.remove('d-none');
        liveVideo.play();
        startCamera(cameraDeviceSelect);
    });
    takeImageButton.addEventListener('click', async () => {
        takeImageButton.disabled = true;
        uploadImageToS3Button.disabled = false;
        takeImage(stillFrame, liveVideo)
    });
    uploadImageToS3Button.addEventListener('click', async () => {
        uploadImageToS3Button.disabled = true;
        uploadImageToS3Spinner.classList.remove('d-none');
        uploadImageToS3();
    });
    createOTAJobButton.addEventListener('click', async () => {
        createOTAJobButton.disabled = true;
        createOTAJobSpinner.classList.remove('d-none');

        await createOTAJob();

        createOTAJobSpinner.classList.add('d-none');
        uploadSuccessfulAlert.classList.add('d-none');
        jobCreatedAlert.classList.remove('d-none');
        startOverButton.classList.remove('d-none');
    });
    startOverButton.addEventListener('click', async () => {
        imageData = new Uint16Array();
        getEnv().image_s3_object_name = "";
        getEnv().image_s3_object_size = 0;

        startCameraButton.disabled = false;
        takeImageButton.disabled = true;
        uploadImageToS3Button.disabled = true;
        createOTAJobButton.disabled = true;

        createOTAJobButton.classList.add('d-none');
        startOverButton.classList.add('d-none');
        liveVideo.classList.add('d-none');
        liveVideo.classList.add('spinner-grow');
        liveVideo.classList.remove('stream_active');
        stillFrame.classList.add('d-none');
        uploadSuccessfulAlert.classList.add('d-none');
        jobCreatedAlert.classList.add('d-none');
    });
}

async function startCamera(cameraDeviceSelect: HTMLSelectElement) {
    const constraints = {
        video: {
            height: {
                ideal: imageHeight
            },
            width: {
                ideal: imageWidth
            },
            deviceId: cameraDeviceSelect.value != '' ? { exact: cameraDeviceSelect.value } : undefined,
        }
    };
    const stream = await navigator.mediaDevices.getUserMedia(constraints);
    liveVideo.srcObject = stream;
    getCameraSelection(cameraDeviceSelect);
}

async function getCameraSelection(cameraDeviceSelect: HTMLSelectElement) {
    try {
        const devices = await navigator.mediaDevices.enumerateDevices();
        const videoDevices = devices.filter(device => device.kind === 'videoinput');
        const options = videoDevices.map(videoDevice => {
            const e = document.createElement('option');
            e.value = videoDevice.deviceId;
            e.textContent = videoDevice.label;
            return e;
        });

        if (cameraDeviceSelect.options.length == 1 && cameraDeviceSelect.options[0].value == '') {
            cameraDeviceSelect.replaceChildren(...options);
        }
    } catch (error) {
        console.log("MediaDevices API not supported. Use a different browser or access over HTTPS!");
        alert("MediaDevices API not supported. Please use a different browser or device!");
    }
};

async function takeImage(stillFrame: HTMLCanvasElement, video: HTMLVideoElement) {
    const frameContext = stillFrame.getContext('2d')!;
    stillFrame.width = video.videoWidth;
    stillFrame.height = video.videoHeight;
    stillFrame.classList.remove('d-none');
    frameContext.drawImage(video, 0, 0);

    video.classList.add('d-none');
    try {
        video.srcObject!.getTracks().forEach(function (track) {
            track.stop();
        });
    } catch (err) {
        console.log("Video doesn't have tracks:", err);
    }

    const imgd = frameContext.getImageData(0, 0, imageWidth, imageHeight);
    frameContext.putImageData(imgd, 1, 1);

    imageData = convertToRGB565(imgd);
}

function convertToRGB565(imgd: ImageData) {
    const pix = imgd.data;
    let rgb565 = new Uint16Array(imageHeight * imageWidth);
    let j = 0;

    for (var i = 0, n = pix.length; i < n; i += 4) {
        let val = (pix[i] >> 3) << 11 | (pix[i + 1] >> 2) << 5 | (pix[i + 2] >> 3)
        let pix_MSB = (val & 0xFF) << 8;
        let pix_LSB = (val >> 8) & 0xFF;
        rgb565[j++] = pix_MSB | pix_LSB
    }

    return rgb565;
}

async function uploadImageToS3() {
    if (imageData === null) {
        console.error("imageData is null!");
        return;
    }

    const blob = new Blob([new Uint16Array(imageData)], { type: "application/octet-stream" });

    getEnv().image_s3_object_name = 'MyImage.bin';
    getEnv().image_s3_object_size = blob.size;

    const client = new S3Client({
        region: getEnv().AWSRegion,
        credentials: fromCognitoIdentityPool({
            client: new CognitoIdentityClient({ region: getEnv().AWSRegion }),
            identityPoolId: getEnv().AWSCognitoIdentityPoolId,
            logins: getClientCreds(),
        }),
    });
    const input = {
        Bucket: getEnv().ImageS3Bucket,
        Key: getEnv().image_s3_object_name,
        Body: blob,
    }
    const command = new PutObjectCommand(input);

    try {
        const response = await client.send(command);
        console.log("PutObject", response);
    } catch (err) {
        console.error(err);
        return;
    }

    uploadImageToS3Spinner.classList.add('d-none');
    createOTAJobButton.disabled = false;
    createOTAJobButton.classList.remove('d-none');
    uploadSuccessfulAlert.classList.remove('d-none');
}

async function createOTAJob() {
    const iotClient = await getIoTClient(getEnv());

    const fileId = 0;
    const streamId = uuid();
    const streamInput = {
        streamId: streamId,
        files: [{
            fileId: fileId,
            s3Location: {
                bucket: getEnv().ImageS3Bucket,
                key: getEnv().image_s3_object_name,
            }
        }],
        roleArn: getEnv().OTARoleARN,
    };
    try {
        const response = await iotClient.send(new CreateStreamCommand(streamInput));
        console.log("CreateStream", response);
    } catch (err) {
        console.error(err);
        return;
    }

    const jobId = uuid();
    const jobInput = {
        jobId: `image-transfer-${jobId}`,
        targets: [
            `arn:aws:iot:${getEnv().AWSRegion}:${getEnv().AWSAccountId}:thing/${getEnv().thing_name}`
        ],
        targetSelection: TargetSelection.SNAPSHOT,
        document: JSON.stringify({
            afr_ota: {
                protocols: ["MQTT"],
                streamname: streamId,
                files: [{
                    "fileid": fileId,
                    "filesize": getEnv().image_s3_object_size,
                    "fileType": 202, // ExpressLink OTA file type code: Host update
                    "filepath": "dummy",
                    "certfile": "dummy",
                    "sig-sha1-rsa": "dummy",
                    "auth_scheme": null,
                    "update_data_url": null
                }]
            }
        })
    };
    try {
        const response = await iotClient.send(new CreateJobCommand(jobInput));
        console.log("CreateJob", response);
    } catch (err) {
        console.error(err);
        return;
    }
}
