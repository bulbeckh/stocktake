#!/bin/bash

source ../setup.sh

./layout.py > out.sdf

gz sim -v4 out.sdf
