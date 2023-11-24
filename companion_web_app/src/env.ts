// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

import { ListThingsCommand } from "@aws-sdk/client-iot";
import { fetchIoTCoreEndpoint, getIoTClient } from "./aws_access";
import { showToast } from "./utils";
import { Logins } from "@aws-sdk/credential-provider-cognito-identity";

export let env = {
    // populated by front-end
    endpoint: "",
    thing_name: "",
    certificate: "",
    image_s3_object_name: "",
    image_s3_object_size: 0,

    // populated by cloud-based config file
    WiFiSSID: "",
    WiFiPassphrase: "",
    AWSAccountId: "",
    AWSRegion: "",
    AWSCognitoIdentityPoolId: "",
    CognitoRedirect: "",
    UserPoolID: "",
    CognitoIoTPolicyName: "",
    CompanionWebAppURL: "",
    GrafanaURL: "",
    SidewalkProvisioningLambdaFunctionName: "",
    ImageS3Bucket: "",
    OTARoleARN: "",
    RuleActionTimestreamRoleARN: "",
    TimestreamDatabase: "",
    TimestreamTable: "",
};

let id_token: string | undefined = undefined;

export function getEnv() {
    return env;
}

export async function initEnv() {
    try {
        const response = await fetch("/env.json");
        env = await response.json();
    } catch (err) {
        alert("Failed to read config file! Companion Web App will not work!");
        document.body.textContent = "";
        return;
    }

    await getClientCreds();

    env.endpoint = await fetchIoTCoreEndpoint(env);
    console.log("IoT Endpoint:", env.endpoint);

    const queryParams = (new URL(document.location.toString())).searchParams;
    env.thing_name = queryParams.get('thing_name')!;
    if (env.thing_name === null || env.thing_name == '') {
        console.warn("Thing Name missing! Trying to fetch first Thing in AWS IoT device registry...");
        await fetchIoTThings(true);
    }

    console.log("Env:", env);
}

async function fetchIoTThings(firstCheck: boolean) {
    const iotClient = await getIoTClient(env);
    const command = new ListThingsCommand({
        maxResults: 10,
    });
    try {
        const response = await iotClient.send(command);
        if (response.things?.length == 1) {
            env.thing_name = response.things!.at(0)!.thingName!;
            if (firstCheck) {
                showToast("AWS IoT thing found!", `Using thing name found in AWS IoT device registry:<br><strong>${env.thing_name}</strong>`);
            }
            return;
        } else if (response.things!.length! > 1) {
            alert("More than one Thing detected! Please delete unneeded Things and reload this page - or pass thing_name query parameter!");
        }
    } catch (err) {
        console.error(err);
    }
    setTimeout(async () => fetchIoTThings(false), 5000);
}

export function getClientCreds(): Logins {
    if (id_token === undefined) {
        // Cognito Hosted UI redirects back to the Companion Web App using hash parameters
        let hashParams = window.location.hash
        if (hashParams.startsWith('#')) {
            hashParams = hashParams.substring(1);
        }

        const params = hashParams.split('&').reduce(function (res, item) {
            const parts = item.split('=', 2);
            if (parts.length == 2) {
                const key = parts[0];
                const value = parts[1];
                res[key] = value;
            }
            return res;
        }, {});
        id_token = params['id_token'];

        if (id_token !== undefined) {
            console.log("Found Cognito Hosted UI id_token as hash parameter!");

            // if valid id_token found in hash parameters, store it in a cookie
            const d = new Date();
            d.setTime(d.getTime() + (24*60*60*1000));
            document.cookie = `id_token=${id_token}; expires=${d.toUTCString()}; path=/`;
            console.log("Stored Cognito Hosted UI id_token as cookie!");

            // hide the hash params from the browser URL bar
            history.pushState({}, "", "/");
        }
    }

    if (id_token === undefined) {
        // check if we have stored the id_token in a cookie
        const cookies = document.cookie.split(';');
        for (let i = 0; i < cookies.length; i++) {
            const element = cookies[i];
            if (element.startsWith('id_token=')) {
                id_token = element.split('=')[1];
                console.log("Found Cognito Hosted UI id_token as cookie!");
                break;
            }
        }
    }

    if (id_token === undefined) {
        // if no token is present, then redirect to the Hosted UI
        console.log("No Cognito Hosted UI id_token found - redirecting to Cognito Hosted UI login page!");

        // Cognito Hosted UI only accepts HTTPS callback URLs, so we need to force https for local development servers.
        // If you local development server does not listen on HTTPS, you can simply remove the `s` in the browser URL bar and the redirect will continue.
        const redirectUrl = `https://${location.host}`;
        const url = getEnv().CognitoRedirect + `&redirect_uri=${redirectUrl}`;
        window.location.replace(url);
    }

    const COGNITO_ID = `cognito-idp.${getEnv().AWSRegion}.amazonaws.com/${getEnv().UserPoolID}`;
    return { [COGNITO_ID]: id_token };
}
