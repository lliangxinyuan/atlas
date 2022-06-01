#!/bin/bash
path_cur=$(cd `dirname $0`; pwd)
build_type="Release"

function preparePath() {
    rm -rf $1
    mkdir -p $1
    cd $1
}

function buildA300() {
    if [ ! "${ARCH_PATTERN}" ]; then
        # set ARCH_PATTERN to acllib when it was not specified by user
        export ARCH_PATTERN=acllib
        echo "ARCH_PATTERN is set to the default value: ${ARCH_PATTERN}"
    else
        echo "ARCH_PATTERN is set to ${ARCH_PATTERN} by user, reset it to ${ARCH_PATTERN}/acllib"
        export ARCH_PATTERN=${ARCH_PATTERN}/acllib
    fi

    path_build=$path_cur/build
    preparePath $path_build
    cmake -DCMAKE_BUILD_TYPE=$build_type ..
    make -j
    ret=$?
    cd ..
    return ${ret}
}

# build program which can run on A500
function buildA500() {
    # need to set ARCH_PATTERN to aarch64
    version_path="${ASCEND_HOME}/${ASCEND_VERSION}"
    arch_pattern_path=$(ls ${version_path} |grep "^arm64-linux")
    if [ $? -ne 0 ] || [ ! -d "${version_path}/${arch_pattern_path}" ]; then
        echo "Thers is no aarch64 acllib under ${version_path}, please install the Ascend-Toolkit package with aarch64 pattern."
        return 1
    fi
    export ARCH_PATTERN="${arch_pattern_path}/acllib"
    echo "export ARCH_PATTERN=${arch_pattern_path}/acllib"
    path_build=$path_cur/build
    preparePath $path_build
    cmake -DCMAKE_BUILD_TYPE=$build_type -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ ..
    make -j
    ret=$?
    cd ..
    return ${ret}
}

# set ASCEND_VERSION to ascend-toolkit/latest when it was not specified by user
if [ ! "${ASCEND_VERSION}" ]; then
    export ASCEND_VERSION=ascend-toolkit/latest
    echo "Set ASCEND_VERSION to the default value: ${ASCEND_VERSION}"
else
    echo "ASCEND_VERSION is set to ${ASCEND_VERSION} by user"
fi
# build with different according to the parameter, default is A300
if [ "$1" == "A500" ]; then
    buildA500
else
    buildA300
fi
if [ $? -ne 0 ]; then
    exit 1
fi

# copy config file and model into dist
if [ ! -d dist ]; then
    echo "Build failed, dist directory does not exist."
    exit 1
fi
rm -rf ./dist/data
mkdir ./dist/data
cp -r ./data/config ./dist/data/

mkdir -p ./dist/data/models/resnet
cp ./data/models/resnet/imagenet1000_clsidx_to_labels.txt ./dist/data/models/resnet

ls ./data/models/resnet/*.om >/dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "[Warning] No .om file in ./data/models/resnet, please convert the model to .om file first."
else
    cp ./data/models/resnet/*.om ./dist/data/models/resnet
fi

mkdir -p ./dist/data/models/single_op

ls ./data/models/single_op/*.om >/dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "[Warning] No .om file in ./data/models/single_op, please convert the model to .om file first."
else
    cp ./data/models/single_op/*.om ./dist/data/models/single_op
fi