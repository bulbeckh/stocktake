FROM osrf/ros:jazzy-desktop

# Prevent interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Basic development tools
RUN apt update && apt install -y \
    build-essential \
    cmake \
    git \
    python3-pip \
    python3-colcon-common-extensions \
    python3-rosdep \
    python3-vcstool \
    python3-venv \
    && rm -rf /var/lib/apt/lists/*

# Copy source-install scripts into the image
COPY docker/scripts/ /tmp/install-scripts/

# Run source installs
RUN chmod +x /tmp/install-scripts/*.sh && \
    /tmp/install-scripts/install-nvidia-swagger.sh && \
    /tmp/install-scripts/install-stella-vslam.sh && \
    echo "/usr/local/lib" > /etc/ld.so.conf.d/local.conf && \
    ldconfig && \
    rm -rf /tmp/install-scripts

# Initialize rosdep
RUN rosdep init || true
RUN rosdep update

# Copy package manifests
COPY docker/deps /tmp/deps

## Install ros dependencies
RUN rosdep install --from-paths /tmp/deps --ignore-src -y -r

## Install stella_vslam_ros -- NOTE move this to either a sub-module, a vcs import, or part of the install-stella-vlsam script
RUN git clone --recursive -b ros2 --depth 1 https://github.com/stella-cv/stella_vslam_ros.git /workspaces/stocktake-vslam/src

## Install stella-vslam-ros dependencies (NOTE should merge this into one when we do proper dependency re-write)
RUN rosdep install --from-paths /workspaces/stocktake-vslam/src --ignore-src -y -r --skip-keys=stella_vslam

COPY docker/patches/stella-vslam-ros2.patch /tmp/stella-vslam-ros2.patch
RUN cd /workspaces/stocktake-vslam/src && git apply /tmp/stella-vslam-ros2.patch

## Download the fbow to /tmp/orb_vocab.fbow
RUN curl -sL "https://github.com/stella-cv/FBoW_orb_vocab/raw/main/orb_vocab.fbow" -o /tmp/orb_vocab.fbow

## Remove repo - will be mounted during docker compose
RUN rm -rf /tmp/deps /tmp/stella-vslam-ros2.patch

# Default shell
CMD ["bash"]
