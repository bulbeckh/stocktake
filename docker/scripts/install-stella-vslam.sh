#!/usr/bin/env bash
set -euo pipefail

# Save current directory
ROOT_DIR="$(pwd)"

PREFIX="/usr/local"
EIGEN_VERSION="3.3.7"
EIGEN_HEADER="${PREFIX}/include/eigen3/Eigen/Core"
EIGEN_CMAKE_CONFIG="${PREFIX}/share/eigen3/cmake/Eigen3Config.cmake"

FORCE=false

if [[ "${1:-}" == "--force" ]]; then
  FORCE=true
fi

# Install system dependencies non-interactively (g2o, opencv, eigen, iridescence deps)
sudo DEBIAN_FRONTEND=noninteractive apt-get update -y
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
	build-essential \
	pkg-config \
	cmake \
	git \
	wget \
	curl \
	unzip \
	libatlas-base-dev \
	libsuitesparse-dev \
	libgtk-3-dev \
	ffmpeg \
	libavcodec-dev \
	libavformat-dev \
	libavutil-dev \
	libswscale-dev \
	libtbb-dev \
	gfortran \
	libyaml-cpp-dev \
	libgflags-dev \
	sqlite3 \
	libsqlite3-dev
	#libavresample-dev \

## Iridescence deps
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
	libglm-dev \
	libglfw3-dev \
	libpng-dev \
	libjpeg-dev \
	libeigen3-dev \
	libboost-filesystem-dev \
	libboost-program-options-dev

## libeigen install
if [ ! -d "/usr/local/include/eigen3/Eigen" ] || [ "$FORCE" = true ]; then
	cd /tmp
	wget -q https://gitlab.com/libeigen/eigen/-/archive/3.3.7/eigen-3.3.7.tar.bz2

	tar xf eigen-3.3.7.tar.bz2
	rm -f eigen-3.3.7.tar.bz2

	cd eigen-3.3.7
	mkdir -p build
	cd build

	cmake \
	    -DCMAKE_BUILD_TYPE=Release \
	    -DCMAKE_INSTALL_PREFIX=/usr/local \
	    ..

	make -j"$(nproc)"
	make install
else
	echo "Eigen already installed. Skipping install."
fi

## opencv install (note, missing uninstall before re-install, also 4.13.0 is not the version from the docs)
if [ ! -d "/usr/local/include/opencv4/opencv2" ] || [ "$FORCE" = true ]; then
	cd /tmp
	wget -q https://github.com/opencv/opencv/archive/refs/tags/4.13.0.zip
	unzip -q 4.13.0.zip && rm -rf 4.13.0.zip

	# Build and install OpenCV
	cd opencv-4.13.0
	mkdir -p build && cd build
	cmake \
	    -DCMAKE_BUILD_TYPE=Release \
	    -DCMAKE_INSTALL_PREFIX=/usr/local \
	    -DBUILD_DOCS=OFF \
	    -DBUILD_EXAMPLES=OFF \
	    -DBUILD_JASPER=OFF \
	    -DBUILD_OPENEXR=OFF \
	    -DBUILD_PERF_TESTS=OFF \
	    -DBUILD_TESTS=OFF \
	    -DBUILD_PROTOBUF=OFF \
	    -DBUILD_opencv_apps=OFF \
	    -DBUILD_opencv_dnn=OFF \
	    -DBUILD_opencv_ml=OFF \
	    -DBUILD_opencv_python_bindings_generator=OFF \
	    -DENABLE_CXX11=ON \
	    -DENABLE_FAST_MATH=ON \
	    -DWITH_EIGEN=ON \
	    -DWITH_FFMPEG=ON \
	    -DWITH_TBB=ON \
	    -DWITH_OPENMP=ON \
	    ..

	make -j"$(nproc)"
	make install
else
	echo "OpenCV already installed. Skipping install."
fi

## fbow install
if [ ! -d "/usr/local/include/fbow" ] || [ "$FORCE" = true ]; then
	cd /tmp
	git clone https://github.com/stella-cv/FBoW.git
	cd FBoW
	mkdir -p build && cd build
	cmake \
	    -DCMAKE_BUILD_TYPE=Release \
	    -DCMAKE_INSTALL_PREFIX=/usr/local \
	    ..
	make -j"$(nproc)" && make install
else
	echo "FBOW already installed. Skipping install."
fi

if [ ! -d "/usr/local/include/g2o" ] || [ "$FORCE" = true ]; then
	cd /tmp
	git clone https://github.com/RainerKuemmerle/g2o.git
	cd g2o
	git checkout 20230223_git
	mkdir build && cd build
	cmake \
	    -DCMAKE_BUILD_TYPE=Release \
	    -DCMAKE_INSTALL_PREFIX=/usr/local \
	    -DBUILD_SHARED_LIBS=ON \
	    -DBUILD_UNITTESTS=OFF \
	    -DG2O_USE_CHOLMOD=OFF \
	    -DG2O_USE_CSPARSE=ON \
	    -DG2O_USE_OPENGL=OFF \
	    -DG2O_USE_OPENMP=OFF \
	    -DG2O_BUILD_APPS=OFF \
	    -DG2O_BUILD_EXAMPLES=OFF \
	    -DG2O_BUILD_LINKED_APPS=OFF \
	    ..
	make -j"$(nproc)" && make install
else
	echo "g2o already installed. Skipping install."
fi

## iridescence install
if [ ! -d "/usr/local/include/iridescence" ] || [ "$FORCE" = true ]; then
	cd /tmp
	git clone https://github.com/koide3/iridescence.git
	cd iridescence
	git checkout 085322e0c949f75b67d24d361784e85ad7f197ab
	git submodule update --init --recursive
	mkdir -p build && cd build
	cmake \
	    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
	    ..
	make -j"$(nproc)" && make install
else
	echo "Iridescence already installed. Skipping install."
fi

## stella-vslam install
if [ ! -d "/root/lib/stella_vslam" ] || [ "$FORCE" = true ]; then
	mkdir -p ~/lib
	cd ~/lib
	git clone --recursive https://github.com/stella-cv/stella_vslam.git
	cd stella_vslam
	mkdir build && cd build
	cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
	make -j"$(nproc)" && make install
else
	echo "stella-vslam already installed. Skipping install."
fi

## stella iridescence viewer install
if [ ! -d "/root/lib/iridescence_viewer" ] || [ "$FORCE" = true ]; then
	cd ~/lib
	git clone --recursive https://github.com/stella-cv/iridescence_viewer.git
	mkdir -p iridescence_viewer/build
	cd iridescence_viewer/build
	cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
	make -j"$(nproc)" && make install
else
	echo "iridescence-viewer already installed. Skipping install."
fi

## install stella-vslam executables (for testing)
if [ ! -d "/root/lib/stella_vslam_examples" ] || [ "$FORCE" = true ]; then
	cd ~/lib
	git clone --recursive https://github.com/stella-cv/stella_vslam_examples.git
	mkdir -p stella_vslam_examples/build
	cd stella_vslam_examples/build
	cmake \
	    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
	    -DUSE_STACK_TRACE_LOGGER=OFF \
	    ..
	make -j"$(nproc)"
else
	echo "stella-vslam examples already installed. Skipping install."
fi

