#!/usr/bin/env bash

# Script to automate uploading code to arduino via arduino-cli
arduino-cli --config-file arduino-cli.yaml upload ./ -p $(arduino-cli board list --json | jq -c ".detected_ports | . [] | .port.address" --raw-output)