FROM ros:jazzy

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

# Default shell
CMD ["bash"]
