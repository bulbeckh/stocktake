#!/usr/bin/env bash
set -euo pipefail

# Save current directory
ROOT_DIR="/workspaces/stocktake"

# Create virtual environment locally
python3 -m venv "${ROOT_DIR}/.venv"

# Activate it
source "${ROOT_DIR}/.venv/bin/activate"

# Prevent colcon from traversing the venv
touch "${ROOT_DIR}/.venv/COLCON_IGNORE"

# Install system dependencies non-interactively
sudo DEBIAN_FRONTEND=noninteractive apt-get update
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
    libglib2.0-0 \
    #libgl1-mesa-glx \
    #git \
    #git-lfs \

# Initialize git-lfs
#git lfs install --skip-repo

# Clone SWAGGER into current directory
#if [ ! -d "${ROOT_DIR}/SWAGGER" ]; then
    #git clone https://github.com/nvidia-isaac/SWAGGER.git \
        #"${ROOT_DIR}/SWAGGER"
#fi

cd "/SWAGGER"

# Pull large files
#git lfs pull

# Upgrade pip tooling
pip install --upgrade pip setuptools wheel

# Install SWAGGER into the virtual environment
pip install -e .
