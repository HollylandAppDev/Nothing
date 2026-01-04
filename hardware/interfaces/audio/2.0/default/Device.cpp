/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * QCS8250 Audio HAL 2.0 Device Implementation
 */

#define LOG_TAG "AudioHAL_Device"

#include "Device.h"

#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <cutils/properties.h>
#include <hardware/audio.h>
#include <log/log.h>

#include <algorithm>
#include <cstring>

namespace android {
namespace hardware {
namespace audio {
namespace V2_0 {
namespace implementation {

using ::android::base::StringPrintf;

// 支持的蓝牙编解码器列表
static const char* kSupportedBluetoothCodecs[] = {
    "sbc", "aac", "aptx", "aptx_hd", "ldac"
};

Device::Device(audio_hw_device_t* device)
    : mDevice(device),
      mBluetoothEnabled(false),
      mBluetoothCodec("sbc"),
      mBluetoothVolume(1.0f),
      mBluetoothDeviceAddress(""),
      mBluetoothDeviceName(""),
      mBluetoothConnected(false),
      mBluetoothScoEnabled(false) {
    ALOGD("Device::Device() created");
}

Device::~Device() {
    ALOGD("Device::~Device() destroying");
    if (mDevice != nullptr) {
        int status = audio_hw_device_close(mDevice);
        if (status != 0) {
            ALOGE("Failed to close audio hw device: %d", status);
        }
        mDevice = nullptr;
    }
}

Result Device::analyzeStatus(const char* funcName, int status) {
    if (status == 0) {
        return Result::OK;
    }
    ALOGE("%s failed with status %d", funcName, status);
    switch (status) {
        case -EINVAL:
            return Result::INVALID_ARGUMENTS;
        case -ENODATA:
            return Result::INVALID_STATE;
        case -ENODEV:
            return Result::NOT_INITIALIZED;
        case -ENOSYS:
            return Result::NOT_SUPPORTED;
        default:
            return Result::INVALID_STATE;
    }
}

Result Device::setParameterImpl(const char* keyValue) {
    if (mDevice->set_parameters == nullptr) {
        ALOGW("set_parameters is null, not supported");
        return Result::NOT_SUPPORTED;
    }
    int status = mDevice->set_parameters(mDevice, keyValue);
    return analyzeStatus("set_parameters", status);
}

bool Device::isValidBluetoothCodec(const std::string& codec) const {
    for (const char* supported : kSupportedBluetoothCodecs) {
        if (codec == supported) {
            return true;
        }
    }
    return false;
}

Return<bool> Device::supportsAudioPatches() {
    return mDevice->create_audio_patch != nullptr;
}

Return<Result> Device::initCheck() {
    int status = mDevice->init_check(mDevice);
    return analyzeStatus("init_check", status);
}

Return<Result> Device::setVoiceVolume(float volume) {
    if (mDevice->set_voice_volume == nullptr) {
        return Result::NOT_SUPPORTED;
    }
    if (volume < 0.0f || volume > 1.0f) {
        ALOGW("setVoiceVolume: invalid volume %f", volume);
        return Result::INVALID_ARGUMENTS;
    }
    int status = mDevice->set_voice_volume(mDevice, volume);
    return analyzeStatus("set_voice_volume", status);
}

Return<Result> Device::setMasterVolume(float volume) {
    if (mDevice->set_master_volume == nullptr) {
        return Result::NOT_SUPPORTED;
    }
    if (volume < 0.0f || volume > 1.0f) {
        return Result::INVALID_ARGUMENTS;
    }
    int status = mDevice->set_master_volume(mDevice, volume);
    return analyzeStatus("set_master_volume", status);
}

Return<void> Device::getMasterVolume(getMasterVolume_cb _hidl_cb) {
    Result retval = Result::NOT_SUPPORTED;
    float volume = 0.0f;
    if (mDevice->get_master_volume != nullptr) {
        int status = mDevice->get_master_volume(mDevice, &volume);
        retval = analyzeStatus("get_master_volume", status);
    }
    _hidl_cb(retval, volume);
    return Void();
}

Return<Result> Device::setMasterMute(bool mute) {
    if (mDevice->set_master_mute == nullptr) {
        return Result::NOT_SUPPORTED;
    }
    int status = mDevice->set_master_mute(mDevice, mute);
    return analyzeStatus("set_master_mute", status);
}

Return<void> Device::getMasterMute(getMasterMute_cb _hidl_cb) {
    Result retval = Result::NOT_SUPPORTED;
    bool mute = false;
    if (mDevice->get_master_mute != nullptr) {
        int status = mDevice->get_master_mute(mDevice, &mute);
        retval = analyzeStatus("get_master_mute", status);
    }
    _hidl_cb(retval, mute);
    return Void();
}

Return<void> Device::getInputBufferSize(
        const AudioConfig& config,
        getInputBufferSize_cb _hidl_cb) {
    audio_config_t halConfig;
    memset(&halConfig, 0, sizeof(halConfig));
    halConfig.sample_rate = config.sampleRateHz;
    halConfig.channel_mask = static_cast<audio_channel_mask_t>(config.channelMask);
    halConfig.format = static_cast<audio_format_t>(config.format);

    size_t bufferSize = mDevice->get_input_buffer_size(mDevice, &halConfig);
    Result retval = (bufferSize > 0) ? Result::OK : Result::NOT_SUPPORTED;
    _hidl_cb(retval, bufferSize);
    return Void();
}

Return<void> Device::openOutputStream(
        AudioIoHandle ioHandle,
        const DeviceAddress& device,
        const AudioConfig& config,
        AudioOutputFlag flags,
        openOutputStream_cb _hidl_cb) {
    // 实际实现需要创建 StreamOut 对象
    // 这里提供框架代码
    ALOGD("openOutputStream: ioHandle=%d, flags=%d", ioHandle, static_cast<int>(flags));

    AudioConfig suggestedConfig = config;
    _hidl_cb(Result::NOT_SUPPORTED, nullptr, suggestedConfig);
    return Void();
}

Return<void> Device::openInputStream(
        AudioIoHandle ioHandle,
        const DeviceAddress& device,
        const AudioConfig& config,
        AudioInputFlag flags,
        AudioSource source,
        openInputStream_cb _hidl_cb) {
    // 实际实现需要创建 StreamIn 对象
    ALOGD("openInputStream: ioHandle=%d, flags=%d", ioHandle, static_cast<int>(flags));

    AudioConfig suggestedConfig = config;
    _hidl_cb(Result::NOT_SUPPORTED, nullptr, suggestedConfig);
    return Void();
}

Return<Result> Device::setParameters(
        const hidl_vec<ParameterValue>& parameters) {
    std::string params;
    for (size_t i = 0; i < parameters.size(); ++i) {
        if (i > 0) {
            params += ";";
        }
        params += parameters[i].key;
        params += "=";
        params += parameters[i].value;
    }
    ALOGD("setParameters: %s", params.c_str());
    return setParameterImpl(params.c_str());
}

Return<void> Device::getParameters(
        const hidl_vec<hidl_string>& keys,
        getParameters_cb _hidl_cb) {
    hidl_vec<ParameterValue> parameters;
    Result retval = Result::NOT_SUPPORTED;

    if (mDevice->get_parameters != nullptr) {
        std::string keysStr;
        for (size_t i = 0; i < keys.size(); ++i) {
            if (i > 0) keysStr += ";";
            keysStr += keys[i];
        }

        char* result = mDevice->get_parameters(mDevice, keysStr.c_str());
        if (result != nullptr) {
            // 解析返回值... (简化实现)
            free(result);
            retval = Result::OK;
        }
    }

    _hidl_cb(retval, parameters);
    return Void();
}

Return<void> Device::getMicrophones(getMicrophones_cb _hidl_cb) {
    hidl_vec<MicrophoneInfo> microphones;
    _hidl_cb(Result::NOT_SUPPORTED, microphones);
    return Void();
}

Return<Result> Device::setScreenState(bool turnedOn) {
    std::string param = StringPrintf("screen_state=%s", turnedOn ? "on" : "off");
    return setParameterImpl(param.c_str());
}

Return<void> Device::debugDump(const hidl_handle& fd) {
    if (fd.getNativeHandle() != nullptr && fd->numFds >= 1) {
        int fdNum = fd->data[0];
        if (mDevice->dump != nullptr) {
            mDevice->dump(mDevice, fdNum);
        }
    }
    return Void();
}

/*
 * ============================================================
 * QCS8250 Bluetooth Audio Extension - 自定义接口实现
 * ============================================================
 */

Return<Result> Device::openBluetooth(bool enable) {
    std::lock_guard<std::mutex> lock(mBluetoothMutex);

    ALOGI("openBluetooth: enable=%d", enable);

    // 设置蓝牙使能参数到底层 HAL
    std::string param = StringPrintf("bluetooth_enabled=%d", enable ? 1 : 0);
    Result result = setParameterImpl(param.c_str());

    if (result == Result::OK) {
        mBluetoothEnabled = enable;
        ALOGI("Bluetooth audio %s successfully", enable ? "enabled" : "disabled");
    } else {
        ALOGE("Failed to %s bluetooth audio", enable ? "enable" : "disable");
    }

    return result;
}

Return<Result> Device::setBluetoothCodec(const hidl_string& codec) {
    std::lock_guard<std::mutex> lock(mBluetoothMutex);

    std::string codecStr = codec;
    ALOGI("setBluetoothCodec: codec=%s", codecStr.c_str());

    // 验证编解码器
    if (!isValidBluetoothCodec(codecStr)) {
        ALOGE("Invalid bluetooth codec: %s", codecStr.c_str());
        return Result::INVALID_ARGUMENTS;
    }

    // 设置编解码器参数
    std::string param = StringPrintf("bt_a2dp_codec=%s", codecStr.c_str());
    Result result = setParameterImpl(param.c_str());

    if (result == Result::OK) {
        mBluetoothCodec = codecStr;
        ALOGI("Bluetooth codec set to %s", codecStr.c_str());
    }

    return result;
}

Return<void> Device::getBluetoothStatus(getBluetoothStatus_cb _hidl_cb) {
    std::lock_guard<std::mutex> lock(mBluetoothMutex);

    ALOGD("getBluetoothStatus: enabled=%d, codec=%s",
          mBluetoothEnabled, mBluetoothCodec.c_str());

    _hidl_cb(Result::OK, mBluetoothEnabled, mBluetoothCodec);
    return Void();
}

Return<Result> Device::setBluetoothVolume(float volume) {
    std::lock_guard<std::mutex> lock(mBluetoothMutex);

    ALOGI("setBluetoothVolume: volume=%f", volume);

    if (volume < 0.0f || volume > 1.0f) {
        ALOGE("Invalid bluetooth volume: %f", volume);
        return Result::INVALID_ARGUMENTS;
    }

    // 设置蓝牙音量参数
    // QCS8250 使用 0-127 的音量范围
    int volumeIndex = static_cast<int>(volume * 127);
    std::string param = StringPrintf("bt_a2dp_volume=%d", volumeIndex);
    Result result = setParameterImpl(param.c_str());

    if (result == Result::OK) {
        mBluetoothVolume = volume;
        ALOGI("Bluetooth volume set to %f (index=%d)", volume, volumeIndex);
    }

    return result;
}

Return<Result> Device::setBluetoothA2dpDevice(const hidl_string& address) {
    std::lock_guard<std::mutex> lock(mBluetoothMutex);

    std::string addrStr = address;
    ALOGI("setBluetoothA2dpDevice: address=%s", addrStr.c_str());

    // 验证 MAC 地址格式 (XX:XX:XX:XX:XX:XX)
    if (addrStr.length() != 17) {
        ALOGE("Invalid bluetooth address format: %s", addrStr.c_str());
        return Result::INVALID_ARGUMENTS;
    }

    // 设置 A2DP 设备地址
    std::string param = StringPrintf("bt_a2dp_device=%s", addrStr.c_str());
    Result result = setParameterImpl(param.c_str());

    if (result == Result::OK) {
        mBluetoothDeviceAddress = addrStr;
        mBluetoothConnected = true;
        ALOGI("Bluetooth A2DP device set to %s", addrStr.c_str());
    }

    return result;
}

Return<Result> Device::disconnectBluetoothA2dp() {
    std::lock_guard<std::mutex> lock(mBluetoothMutex);

    ALOGI("disconnectBluetoothA2dp");

    // 断开 A2DP 连接
    std::string param = "bt_a2dp_disconnect=1";
    Result result = setParameterImpl(param.c_str());

    if (result == Result::OK) {
        mBluetoothConnected = false;
        mBluetoothDeviceAddress = "";
        mBluetoothDeviceName = "";
        ALOGI("Bluetooth A2DP disconnected");
    }

    return result;
}

Return<Result> Device::setBluetoothScoMode(bool enable) {
    std::lock_guard<std::mutex> lock(mBluetoothMutex);

    ALOGI("setBluetoothScoMode: enable=%d", enable);

    // 设置 SCO 模式参数
    std::string param = StringPrintf("bt_sco_enable=%d", enable ? 1 : 0);
    Result result = setParameterImpl(param.c_str());

    if (result == Result::OK) {
        mBluetoothScoEnabled = enable;
        ALOGI("Bluetooth SCO mode %s", enable ? "enabled" : "disabled");
    }

    return result;
}

Return<void> Device::getBluetoothDeviceInfo(getBluetoothDeviceInfo_cb _hidl_cb) {
    std::lock_guard<std::mutex> lock(mBluetoothMutex);

    ALOGD("getBluetoothDeviceInfo: name=%s, address=%s, connected=%d",
          mBluetoothDeviceName.c_str(),
          mBluetoothDeviceAddress.c_str(),
          mBluetoothConnected);

    _hidl_cb(Result::OK,
             mBluetoothDeviceName,
             mBluetoothDeviceAddress,
             mBluetoothConnected);
    return Void();
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace audio
}  // namespace hardware
}  // namespace android
