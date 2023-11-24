#!/usr/bin/env bash

cd "$(dirname ${BASH_SOURCE})"

npm install

gunzip < public/model.glb.gz > public/model.glb

npx vite build --outDir dist

rm -f dist/env.json

date +%Y-%m-%dT%H%M%S > dist/VERSION.txt
