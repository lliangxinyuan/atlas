/*
 * Copyright (c) 2020.Huawei Technologies Co., Ltd. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/time.h>
#include "AclProcess.h"
#include "ResourceManager/ResourceManager.h"
#include "CommonDataType/CommonDataType.h"
#include "Log/Log.h"
#include "ErrorCode/ErrorCode.h"
#include "CommandParser/CommandParser.h"
#include "ConfigParser/ConfigParser.h"

/*
 * @description: Parse input parameters of command line and check the validity of them
 * @param argc  Number of input arguments
 * @param argv  Input arguments pointer
 * @param options  Used to store the parsed parameter pairs
 * @return: APP_ERR_OK success
 * @return: Other values failure
 * @attention: The program will exit when parameter parsed error
 */
APP_ERROR ParseAndCheckArgs(int argc, const char *argv[], CommandParser &options)
{
    // Construct the command parser
    options.AddOption("-i", "./data/test.jpg", "Optional. Specify the input image, default: ./data/test.jpg");
    options.AddOption("-t", "0", "Model type. 0: YoloV3 Caffe, 1: YoloV3 Tensorflow");
    options.ParseArgs(argc, argv);
    std::string fileName = options.GetStringOption("-i"); // Parse the input image file
    // check the validity of input file
    // access function return 0 or -1, when the path does not exist or some other error occurred, -1 is returned
    if (access(fileName.c_str(), R_OK) == -1) {
        LogError << "Input file " << fileName << " doesn't exist or read failed!";
        return APP_ERR_COMM_NO_EXIST;
    }

    return APP_ERR_OK;
}

/*
 * @description: Get model-related information from configure data
 * @param  configData  Config data obtained from the configuration file setup.config
 * @param  modelInfos  Used to store model-related information obtained from configData
 * @return: APP_ERR_OK success
 * @return: Other values failure
 */
APP_ERROR ReadModelConfig(ConfigParser& configData, std::vector<ModelInfo> &modelInfos)
{
    ModelInfo modelInfo;
    std::string tmp;
    APP_ERROR ret = configData.GetStringValue("model_path", tmp); // Get the model path from config file
    if (ret != APP_ERR_OK) {
        LogError << "Model path in the config file is invalid!";
        return ret;
    }
    char absPath[PATH_MAX];
    // Get the absolute path of model file
    if (realpath(tmp.c_str(), absPath) == nullptr) {
        LogError << "Failed to get the real path of " << tmp.c_str();
        return APP_ERR_COMM_NO_EXIST;
    }
    // Check the validity of model path
    int folderExist = access(absPath, R_OK);
    if (folderExist == -1) {
        LogError << "ModelPath " << absPath << " doesn't exist or read failed!";
        return APP_ERR_COMM_NO_EXIST;
    }
    modelInfo.modelPath = std::string(absPath); // Set the absolute path of model file
    modelInfo.method = LOAD_FROM_FILE_WITH_MEM; // Set the ModelLoadMethod
    int tmpNum;
    ret = configData.GetIntValue("model_width", tmpNum); // Get the input width of model from config file
    if (ret != APP_ERR_OK) {
        LogError << "model_width in the config file is invalid !!";
        return APP_ERR_COMM_INVALID_PARAM;
    }
    modelInfo.modelWidth = tmpNum;

    ret = configData.GetIntValue("model_height", tmpNum); // Get the input height of model from config file
    if (ret != APP_ERR_OK) {
        LogError << "model_height in the config file is invalid !!";
        return APP_ERR_COMM_INVALID_PARAM;
    }
    modelInfo.modelHeight = tmpNum;
    modelInfo.modelName = "yolov3";
    modelInfos.push_back(std::move(modelInfo));
    return APP_ERR_OK;
}

/*
 * @description: Save model-related information and device id obtained through the configuration file to resourceInfo
 * @param  configData  Config data obtained from the configuration file setup.config
 * @param  resourceInfo  Used to store model-related information and device id
 * @return: APP_ERR_OK success
 * @return: Other values failure
 */
APP_ERROR ReadConfigFromFile(ConfigParser& configData, ResourceInfo &resourceInfo)
{
    std::string tmp;
    // Get the device id used by application from config file
    int deviceId;
    APP_ERROR ret = configData.GetIntValue("device_id", deviceId);
    // Convert from string to digital number
    if (ret != APP_ERR_OK) {
        LogError << "deviceId is not digit, please check, ret = " << ret;
        return APP_ERR_COMM_INVALID_PARAM;
    }
    // Check validity of device id
    if (deviceId < 0) {
        LogError << "deviceId:" << deviceId << " is less than 0, not valid, please check!";
        return APP_ERR_COMM_INVALID_PARAM;
    }
    resourceInfo.deviceIds.insert(deviceId);
    std::vector<ModelInfo> modelInfos;
    ret = ReadModelConfig(configData, modelInfos); // Read model config from the config file
    if (ret != APP_ERR_OK) {
        LogError << "Failed to get model info from config file, ret = " << ret;
        return APP_ERR_COMM_READ_FAIL;
    }
    DeviceResInfo deviceResInfo;
    deviceResInfo.modelInfos = std::move(modelInfos);
    resourceInfo.deviceResInfos[deviceId] = std::move(deviceResInfo);

    return APP_ERR_OK;
}

/*
 * @description: Entry function for calling function in class AclProcess to detect object in input image
 * @param  resourceInfo  Used to store model-related information and device id
 * @param  file  Absolute path of the input image
 * @param  modelType  Infer model type, 0 is Caffe, 1 is TF
 * @return: APP_ERR_OK success
 * @return: Other values failure
 */
APP_ERROR Process(ResourceInfo &resourceInfo, const std::string &file, const int modelType)
{
    std::shared_ptr<ResourceManager> instance = ResourceManager::GetInstance();
    int deviceId = *(resourceInfo.deviceIds.begin());
    aclrtContext context = instance->GetContext(deviceId);
    // Initialize an AclProcess module
    AclProcess aclProcess(deviceId, resourceInfo.deviceResInfos[deviceId].modelInfos[0],
        resourceInfo.singleOpFolderPath, context); // Initialize an AclProcess module
    // InitResource of AclProcess class will set current context and creat stream for thread
    APP_ERROR ret = aclProcess.InitResource();
    if (ret != APP_ERR_OK) {
        aclProcess.Release();
        return ret;
    }
    struct timeval begin = {0};
    struct timeval end = {0};
    gettimeofday(&begin, nullptr);
    ret = aclProcess.Process(file, modelType);
    if (ret != APP_ERR_OK) {
        aclProcess.Release();
        return ret;
    }
    gettimeofday(&end, nullptr);
    // Calculate the time cost of the whole process
    double costMs = SEC2MS * (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec) / SEC2MS;
    double fps = SEC2MS / costMs;
    LogInfo << "[Process Delay] cost: " << costMs << "ms\tfps: " << fps;
    aclProcess.Release();
    return APP_ERR_OK;
}

int main(int argc, const char* argv[])
{
    // Print the log with the level of info and above
    AtlasAscendLog::Log::LogInfoOn();
    CommandParser options;
    // Parse and check arguments
    APP_ERROR ret = ParseAndCheckArgs(argc, argv, options);
    if (ret != APP_ERR_OK) {
        return ret;
    }
    int modelType = options.GetIntOption("-t");
    if (modelType != YOLOV3_CAFFE && modelType != YOLOV3_TF) {
        LogError << "Model type is not correct, please check";
        return APP_ERR_INFER_FIND_MODEL_ID_FAIL;
    }
    std::string fileName = options.GetStringOption("-i");
    // get the absolute path of input file
    char absPath[PATH_MAX];
    if (realpath(fileName.c_str(), absPath) == nullptr) {
        LogError << "Failed to get the real path of " << fileName.c_str();
        return APP_ERR_COMM_NO_EXIST;
    }
    // The string pointed to by argv[0] represents the program name
    std::string path(argv[0], argv[0] + strlen(argv[0]));
    ret = ChangeDir(path.c_str()); // Change the directory to the excute file
    if (ret != APP_ERR_OK) {
        LogError << "Failed to change directory to " << path.c_str();
        return ret;
    }
    ConfigParser config;
    // Initialize the config parser module
    if (config.ParseConfig("./data/config/setup.config") != APP_ERR_OK) {
        return APP_ERR_COMM_INIT_FAIL;
    }
    ResourceInfo resourceInfo;
    // The relevant information of dump data can be configured in acl.json,
    // it is an empty file in this sample because we don't need.
    // For details, please refer to the description of aclInit function
    resourceInfo.aclConfigPath = "./data/config/acl.json";
    ret =  ReadConfigFromFile(config, resourceInfo); // Read config data from file
    if (ret != APP_ERR_OK) {
        return ret;
    }
    // instance is a singleton pointer of ResourceManager class, used for resource management
    // InitResource will create context for device
    std::shared_ptr<ResourceManager> instance = ResourceManager::GetInstance();
    ret = instance->InitResource(resourceInfo);
    if (ret != APP_ERR_OK) {
        return ret;
    }
    std::string file(absPath);
    ret = Process(resourceInfo, file, modelType);
    if (ret != APP_ERR_OK) {
        instance->Release();
        return ret;
    }
    instance->Release();
    return 0;
}
