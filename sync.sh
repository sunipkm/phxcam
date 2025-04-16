#!/bin/bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
#sync files to flight computer
rsync -av --exclude-from=$SCRIPT_DIR/excludes.txt $SCRIPT_DIR picture@picture: