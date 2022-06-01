#!/bin/bash

CurrentFolder="$( cd "$(dirname "$0")" ; pwd -P )"

SampleFolder=(
  InferClassification
  DecodeVideo
  EncodeJpeg
  DecodeImage
  CompileDemo
  DvppCrop
  InferObjectDetection
  InferOfflineVideo
)

#compile the sample
errFlag=0
for SamplePtr in ${SampleFolder[@]};do
    cd ${CurrentFolder}/${SamplePtr}
    echo -e building "\033[0;31m$PWD\033[0m"
    bash build.sh
    if [ $? -ne 0 ]; then
        echo -e "Failed to build ${SamplePtr}"
        errFlag=1
    fi
    rm -rf build  
done

if [ ${errFlag} -eq 1 ]; then
    exit 1
fi