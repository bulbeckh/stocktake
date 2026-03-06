#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <filepath>" >&2
    exit 2
fi

FILEPATH="$1"

if [[ ! -f "$FILEPATH" ]]; then
    echo "Error: file not found: $FILEPATH" >&2
    exit 2
fi

trim() {
    local s="$1"
    s="${s#"${s%%[![:space:]]*}"}"
    s="${s%"${s##*[![:space:]]}"}"
    printf '%s' "$s"
}

tags=""
line_num=0
num_re='^[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)$'

while IFS=',' read -r x y z; do
    line_num=$((line_num + 1))

    x="$(trim "$x")"
    y="$(trim "$y")"
    z="$(trim "$z")"

    if [[ -z "$x" || -z "$y" || -z "$z" ]]; then
        echo "Error processing line $line_num: expected 3 comma-separated values" >&2
        exit 1
    fi

    if [[ ! "$x" =~ $num_re || ! "$y" =~ $num_re || ! "$z" =~ $num_re ]]; then
        echo "Error processing line $line_num: expected numeric x,y,z values" >&2
        exit 1
    fi

    tag="{uid:\"uid${line_num}\",data:\"\",pose:{position:{x:${x},y:${y},z:${z}}}}"

    if [[ -n "$tags" ]]; then
        tags+=","
    fi
    tags+="$tag"
done < "$FILEPATH"

cmd="gz service -s /rfid_tag_create --reqtype gz.custom_msgs.RFIDTagList --reptype gz.msgs.Boolean -r 'tags:[$tags]'"

#echo "$cmd"

# Actual execution disabled for now
bash -lc "$cmd"
