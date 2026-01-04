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

#ifndef ANDROID_HARDWARE_AUDIO_V2_0_DEVICE_H
#define ANDROID_HARDWARE_AUDIO_V2_0_DEVICE_H

#include <android/hardware/audio/2.0/IDevice.h>
#include <android/hardware/audio/2.0/IStreamIn.h>
#include <android/hardware/audio/2.0/IStreamOut.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>

#include <hardware/audio.h>

#include <memory>
#include <mutex>
#include <string>

namespace android {
namespace hardware {
namespace audio {
namespace V2_0 {
namespace implementation {

using ::android::hardware::hidl_array;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::sp;

struct Device : public IDevice {
    explicit Device(audio_hw_device_t* device);
    ~Device();

    // IDevice interface methods
    Return<bool> supportsAudioPatches() override;
    Return<Result> initCheck() override;
    Return<Result> setVoiceVolume(float volume) override;
    Return<Result> setMasterVolume(float volume) override;
    Return<void> getMasterVolume(getMasterVolume_cb _hidl_cb) override;
    Return<Result> setMasterMute(bool mute) override;
    Return<void> getMasterMute(getMasterMute_cb _hidl_cb) override;
    Return<void> getInputBufferSize(
        const AudioConfig& config,
        getInputBufferSize_cb _hidl_cb) override;

    Return<void> openOutputStream(
        AudioIoHandle ioHandle,
        const DeviceAddress& device,
        const AudioConfig& config,
        AudioOutputFlag flags,
        openOutputStream_cb _hidl_cb) override;

    Return<void> openInputStream(
        AudioIoHandle ioHandle,
        const DeviceAddress& device,
        const AudioConfig& config,
        AudioInputFlag flags,
        AudioSource source,
        openInputStream_cb _hidl_cb) override;

    Return<Result> setParameters(
        const hidl_vec<ParameterValue>& parameters) override;
    Return<void> getParameters(
        const hidl_vec<hidl_string>& keys,
        getParameters_cb _hidl_cb) override;

    Return<void> getMicrophones(getMicrophones_cb _hidl_cb) override;
    Return<Result> setScreenState(bool turnedOn) override;
    Return<void> debugDump(const hidl_handle& fd) override;

    /*
     * ============================================================
     * QCS8250 Bluetooth Audio Extension - 自定义接口实现
     * ============================================================
     */

    /**
     * 开启/关闭蓝牙音频通路
     */
    Return<Result> openBluetooth(bool enable) override;

    /**
     * 设置蓝牙音频编解码器
     */
    Return<Result> setBluetoothCodec(const hidl_string& codec) override;

    /**
     * 获取当前蓝牙音频状态
     */
    Return<void> getBluetoothStatus(getBluetoothStatus_cb _hidl_cb) override;

    /**
     * 设置蓝牙音频音量
     */
    Return<Result> setBluetoothVolume(float volume) override;

    /**
     * 设置蓝牙 A2DP 设备地址
     */
    Return<Result> setBluetoothA2dpDevice(const hidl_string& address) override;

    /**
     * 断开蓝牙 A2DP 设备
     */
    Return<Result> disconnectBluetoothA2dp() override;

    /**
     * 设置蓝牙 SCO 模式
     */
    Return<Result> setBluetoothScoMode(bool enable) override;

    /**
     * 获取蓝牙设备信息
     */
    Return<void> getBluetoothDeviceInfo(getBluetoothDeviceInfo_cb _hidl_cb) override;

    // 获取底层设备句柄
    audio_hw_device_t* device() const { return mDevice; }

private:
    audio_hw_device_t* mDevice;

    // 蓝牙状态管理
    std::mutex mBluetoothMutex;
    bool mBluetoothEnabled;
    std::string mBluetoothCodec;
    float mBluetoothVolume;
    std::string mBluetoothDeviceAddress;
    std::string mBluetoothDeviceName;
    bool mBluetoothConnected;
    bool mBluetoothScoEnabled;

    // 辅助方法
    Result setParameterImpl(const char* keyValue);
    Result analyzeStatus(const char* funcName, int status);
    bool isValidBluetoothCodec(const std::string& codec) const;
};

}  // namespace implementation
}  // namespace V2_0
}  // namespace audio
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_AUDIO_V2_0_DEVICE_H
