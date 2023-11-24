// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

import * as bootstrap from 'bootstrap'

export function showToast(title: string, body: string) {
    document.getElementById('toast_title')!.innerHTML = title;
    document.getElementById('toast_body')!.innerHTML = body;
    const toastLiveExample = document.getElementById('liveToast');
    bootstrap.Toast.getOrCreateInstance(toastLiveExample!).show();
}
