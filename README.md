# QCS8250 Audio HAL 2.0 Bluetooth 客户端

这是一个完整的方案，允许你的自定义进程通过 HIDL 接口调用 Audio HAL 2.0 来控制蓝牙音频功能。

## 📁 项目结构

```
workspace/
├── hardware/interfaces/audio/2.0/
│   ├── IDevice.hal                              # HAL 接口定义 (添加蓝牙扩展)
│   └── default/
│       ├── Device.h                             # HAL 实现头文件
│       └── Device.cpp                           # HAL 实现源文件
├── vendor/qcom/bluetooth_audio_ctrl/
│   ├── Android.bp                               # 编译配置
│   ├── BluetoothAudioClient.cpp                 # 客户端主程序
│   ├── BluetoothAudioClientLib.cpp              # 库版本实现
│   ├── bluetooth_audio_ctrl.rc                  # init 服务配置
│   └── include/
│       └── BluetoothAudioClient.h               # 库头文件
└── device/qcom/qcs8250/sepolicy/
    ├── bluetooth_audio.te                       # SELinux 策略
    ├── file_contexts                            # 文件上下文
    ├── property_contexts                        # 属性上下文
    ├── property.te                              # 属性类型定义
    └── hwservice_contexts                       # HW 服务上下文
```

## 🔧 调用链路

```
你的进程 (bluetooth_audio_ctrl)
    │
    │ IDevicesFactory::getService()
    │ factory->openDevice("primary")
    │ device->openBluetooth(true)
    │
    ▼ (hwbinder IPC)
    
android.hardware.audio@2.0-service
    │
    │ Device::openBluetooth()
    │ mDevice->set_parameters("bluetooth_enabled=1")
    │
    ▼
    
audio_hw.c (底层 HAL 实现)
```

---

## 📄 完整代码

### 1. HAL 接口定义 - IDevice.hal

**文件路径**: `hardware/interfaces/audio/2.0/IDevice.hal`

在原有接口基础上添加以下蓝牙扩展接口:

```hal
/*
 * ============================================================
 * QCS8250 Bluetooth Audio Extension - 自定义接口
 * ============================================================
 */

/**
 * 开启/关闭蓝牙音频通路
 *
 * @param enable true 开启蓝牙音频, false 关闭
 * @return retval 操作结果
 */
openBluetooth(bool enable) generates (Result retval);

/**
 * 设置蓝牙音频编解码器
 *
 * @param codec 编解码器类型: "sbc", "aac", "aptx", "aptx_hd", "ldac"
 * @return retval 操作结果
 */
setBluetoothCodec(string codec) generates (Result retval);

/**
 * 获取当前蓝牙音频状态
 *
 * @return retval 操作结果
 * @return enabled 是否启用
 * @return codec 当前编解码器
 */
getBluetoothStatus() generates (Result retval, bool enabled, string codec);

/**
 * 设置蓝牙音频音量
 *
 * @param volume 音量 0.0f ~ 1.0f
 * @return retval 操作结果
 */
setBluetoothVolume(float volume) generates (Result retval);

/**
 * 设置蓝牙 A2DP 设备地址
 *
 * @param address 蓝牙 MAC 地址 (格式: "XX:XX:XX:XX:XX:XX")
 * @return retval 操作结果
 */
setBluetoothA2dpDevice(string address) generates (Result retval);

/**
 * 断开蓝牙 A2DP 设备
 *
 * @return retval 操作结果
 */
disconnectBluetoothA2dp() generates (Result retval);

/**
 * 设置蓝牙 SCO 模式 (用于通话)
 *
 * @param enable true 启用 SCO, false 禁用
 * @return retval 操作结果
 */
setBluetoothScoMode(bool enable) generates (Result retval);

/**
 * 获取蓝牙设备信息
 *
 * @return retval 操作结果
 * @return deviceName 设备名称
 * @return macAddress MAC 地址
 * @return connected 是否连接
 */
getBluetoothDeviceInfo() generates (
    Result retval,
    string deviceName,
    string macAddress,
    bool connected);
```

---

### 2. HAL 实现头文件 - Device.h

**文件路径**: `hardware/interfaces/audio/2.0/default/Device.h`

```cpp
/*
 * Copyright (C) 2016 The Android Open Source Project
 * QCS8250 Audio HAL 2.0 Device Implementation
 */

#ifndef ANDROID_HARDWARE_AUDIO_V2_0_DEVICE_H
#define ANDROID_HARDWARE_AUDIO_V2_0_DEVICE_H

#include <android/hardware/audio/2.0/IDevice.h>
#include <hidl/Status.h>
#include <hardware/audio.h>
#include <mutex>
#include <string>

namespace android {
namespace hardware {
namespace audio {
namespace V2_0 {
namespace implementation {

struct Device : public IDevice {
    explicit Device(audio_hw_device_t* device);
    ~Device();

    // ... 原有接口 ...

    /*
     * QCS8250 Bluetooth Audio Extension
     */
    Return<Result> openBluetooth(bool enable) override;
    Return<Result> setBluetoothCodec(const hidl_string& codec) override;
    Return<void> getBluetoothStatus(getBluetoothStatus_cb _hidl_cb) override;
    Return<Result> setBluetoothVolume(float volume) override;
    Return<Result> setBluetoothA2dpDevice(const hidl_string& address) override;
    Return<Result> disconnectBluetoothA2dp() override;
    Return<Result> setBluetoothScoMode(bool enable) override;
    Return<void> getBluetoothDeviceInfo(getBluetoothDeviceInfo_cb _hidl_cb) override;

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

    Result setParameterImpl(const char* keyValue);
    bool isValidBluetoothCodec(const std::string& codec) const;
};

}  // namespace implementation
}  // namespace V2_0
}  // namespace audio
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_AUDIO_V2_0_DEVICE_H
```

---

### 3. HAL 实现源文件 - Device.cpp

**文件路径**: `hardware/interfaces/audio/2.0/default/Device.cpp`

```cpp
/*
 * QCS8250 Audio HAL 2.0 Device Implementation
 */

#define LOG_TAG "AudioHAL_Device"

#include "Device.h"
#include <android-base/stringprintf.h>
#include <log/log.h>

namespace android {
namespace hardware {
namespace audio {
namespace V2_0 {
namespace implementation {

using ::android::base::StringPrintf;

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
}

Result Device::setParameterImpl(const char* keyValue) {
    if (mDevice->set_parameters == nullptr) {
        return Result::NOT_SUPPORTED;
    }
    int status = mDevice->set_parameters(mDevice, keyValue);
    return (status == 0) ? Result::OK : Result::INVALID_STATE;
}

bool Device::isValidBluetoothCodec(const std::string& codec) const {
    for (const char* supported : kSupportedBluetoothCodecs) {
        if (codec == supported) return true;
    }
    return false;
}

Return<Result> Device::openBluetooth(bool enable) {
    std::lock_guard<std::mutex> lock(mBluetoothMutex);
    ALOGI("openBluetooth: enable=%d", enable);

    std::string param = StringPrintf("bluetooth_enabled=%d", enable ? 1 : 0);
    Result result = setParameterImpl(param.c_str());

    if (result == Result::OK) {
        mBluetoothEnabled = enable;
    }
    return result;
}

Return<Result> Device::setBluetoothCodec(const hidl_string& codec) {
    std::lock_guard<std::mutex> lock(mBluetoothMutex);
    std::string codecStr = codec;

    if (!isValidBluetoothCodec(codecStr)) {
        return Result::INVALID_ARGUMENTS;
    }

    std::string param = StringPrintf("bt_a2dp_codec=%s", codecStr.c_str());
    Result result = setParameterImpl(param.c_str());

    if (result == Result::OK) {
        mBluetoothCodec = codecStr;
    }
    return result;
}

Return<void> Device::getBluetoothStatus(getBluetoothStatus_cb _hidl_cb) {
    std::lock_guard<std::mutex> lock(mBluetoothMutex);
    _hidl_cb(Result::OK, mBluetoothEnabled, mBluetoothCodec);
    return Void();
}

Return<Result> Device::setBluetoothVolume(float volume) {
    std::lock_guard<std::mutex> lock(mBluetoothMutex);

    if (volume < 0.0f || volume > 1.0f) {
        return Result::INVALID_ARGUMENTS;
    }

    int volumeIndex = static_cast<int>(volume * 127);
    std::string param = StringPrintf("bt_a2dp_volume=%d", volumeIndex);
    Result result = setParameterImpl(param.c_str());

    if (result == Result::OK) {
        mBluetoothVolume = volume;
    }
    return result;
}

Return<Result> Device::setBluetoothA2dpDevice(const hidl_string& address) {
    std::lock_guard<std::mutex> lock(mBluetoothMutex);
    std::string addrStr = address;

    if (addrStr.length() != 17) {
        return Result::INVALID_ARGUMENTS;
    }

    std::string param = StringPrintf("bt_a2dp_device=%s", addrStr.c_str());
    Result result = setParameterImpl(param.c_str());

    if (result == Result::OK) {
        mBluetoothDeviceAddress = addrStr;
        mBluetoothConnected = true;
    }
    return result;
}

Return<Result> Device::disconnectBluetoothA2dp() {
    std::lock_guard<std::mutex> lock(mBluetoothMutex);

    Result result = setParameterImpl("bt_a2dp_disconnect=1");

    if (result == Result::OK) {
        mBluetoothConnected = false;
        mBluetoothDeviceAddress = "";
    }
    return result;
}

Return<Result> Device::setBluetoothScoMode(bool enable) {
    std::lock_guard<std::mutex> lock(mBluetoothMutex);

    std::string param = StringPrintf("bt_sco_enable=%d", enable ? 1 : 0);
    Result result = setParameterImpl(param.c_str());

    if (result == Result::OK) {
        mBluetoothScoEnabled = enable;
    }
    return result;
}

Return<void> Device::getBluetoothDeviceInfo(getBluetoothDeviceInfo_cb _hidl_cb) {
    std::lock_guard<std::mutex> lock(mBluetoothMutex);
    _hidl_cb(Result::OK, mBluetoothDeviceName, mBluetoothDeviceAddress, mBluetoothConnected);
    return Void();
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace audio
}  // namespace hardware
}  // namespace android
```

---

### 4. 客户端主程序 - BluetoothAudioClient.cpp

**文件路径**: `vendor/qcom/bluetooth_audio_ctrl/BluetoothAudioClient.cpp`

```cpp
/*
 * QCS8250 Bluetooth Audio Control Client
 *
 * 用法:
 *   bluetooth_audio_ctrl enable                    # 开启蓝牙音频
 *   bluetooth_audio_ctrl disable                   # 关闭蓝牙音频
 *   bluetooth_audio_ctrl codec <sbc|aac|aptx|...>  # 设置编解码器
 *   bluetooth_audio_ctrl volume <0.0-1.0>          # 设置音量
 *   bluetooth_audio_ctrl connect <MAC>             # 连接 A2DP 设备
 *   bluetooth_audio_ctrl disconnect                # 断开 A2DP 设备
 *   bluetooth_audio_ctrl sco <on|off>              # SCO 模式
 *   bluetooth_audio_ctrl status                    # 查询状态
 */

#define LOG_TAG "BluetoothAudioClient"

#include <android/hardware/audio/2.0/IDevicesFactory.h>
#include <android/hardware/audio/2.0/IDevice.h>
#include <hidl/HidlTransportSupport.h>
#include <log/log.h>
#include <iostream>
#include <string>

using android::sp;
using android::hardware::Return;
using android::hardware::audio::V2_0::IDevicesFactory;
using android::hardware::audio::V2_0::IDevice;
using android::hardware::audio::V2_0::Result;

class BluetoothAudioClient {
public:
    bool initialize() {
        // 1. 初始化 HIDL 线程池（必须）
        android::hardware::configureRpcThreadpool(1, false);

        // 2. 获取 IDevicesFactory 服务
        sp<IDevicesFactory> factory = IDevicesFactory::getService();
        if (factory == nullptr) {
            std::cerr << "错误: 无法获取 IDevicesFactory 服务" << std::endl;
            return false;
        }

        // 3. 打开 primary 设备
        Result openResult = Result::NOT_INITIALIZED;
        factory->openDevice("primary", [&](Result r, const sp<IDevice>& d) {
            openResult = r;
            if (r == Result::OK) mDevice = d;
        });

        if (openResult != Result::OK || mDevice == nullptr) {
            std::cerr << "错误: 无法打开 primary 音频设备" << std::endl;
            return false;
        }

        return true;
    }

    bool setBluetoothEnabled(bool enable) {
        Return<Result> ret = mDevice->openBluetooth(enable);
        if (!ret.isOk()) return false;
        if ((Result)ret == Result::OK) {
            std::cout << "蓝牙音频已" << (enable ? "开启" : "关闭") << std::endl;
            return true;
        }
        return false;
    }

    bool setCodec(const std::string& codec) {
        Return<Result> ret = mDevice->setBluetoothCodec(codec);
        if (!ret.isOk()) return false;
        if ((Result)ret == Result::OK) {
            std::cout << "编解码器已设置为: " << codec << std::endl;
            return true;
        }
        return false;
    }

    bool setVolume(float volume) {
        Return<Result> ret = mDevice->setBluetoothVolume(volume);
        if (!ret.isOk()) return false;
        if ((Result)ret == Result::OK) {
            std::cout << "音量已设置为: " << (volume * 100) << "%" << std::endl;
            return true;
        }
        return false;
    }

    bool connectA2dp(const std::string& address) {
        Return<Result> ret = mDevice->setBluetoothA2dpDevice(address);
        if (!ret.isOk()) return false;
        if ((Result)ret == Result::OK) {
            std::cout << "已连接: " << address << std::endl;
            return true;
        }
        return false;
    }

    bool disconnectA2dp() {
        Return<Result> ret = mDevice->disconnectBluetoothA2dp();
        if (!ret.isOk()) return false;
        if ((Result)ret == Result::OK) {
            std::cout << "已断开连接" << std::endl;
            return true;
        }
        return false;
    }

    bool setScoMode(bool enable) {
        Return<Result> ret = mDevice->setBluetoothScoMode(enable);
        if (!ret.isOk()) return false;
        if ((Result)ret == Result::OK) {
            std::cout << "SCO 模式已" << (enable ? "开启" : "关闭") << std::endl;
            return true;
        }
        return false;
    }

    void getStatus() {
        mDevice->getBluetoothStatus([](Result r, bool enabled, const auto& codec) {
            if (r == Result::OK) {
                std::cout << "状态: " << (enabled ? "已开启" : "已关闭") << std::endl;
                std::cout << "编解码器: " << codec << std::endl;
            }
        });
    }

private:
    sp<IDevice> mDevice;
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "用法: bluetooth_audio_ctrl <命令> [参数]" << std::endl;
        std::cout << "命令: enable, disable, codec, volume, connect, disconnect, sco, status" << std::endl;
        return 1;
    }

    BluetoothAudioClient client;
    if (!client.initialize()) return 1;

    std::string cmd = argv[1];

    if (cmd == "enable") return client.setBluetoothEnabled(true) ? 0 : 1;
    if (cmd == "disable") return client.setBluetoothEnabled(false) ? 0 : 1;
    if (cmd == "codec" && argc >= 3) return client.setCodec(argv[2]) ? 0 : 1;
    if (cmd == "volume" && argc >= 3) return client.setVolume(atof(argv[2])) ? 0 : 1;
    if (cmd == "connect" && argc >= 3) return client.connectA2dp(argv[2]) ? 0 : 1;
    if (cmd == "disconnect") return client.disconnectA2dp() ? 0 : 1;
    if (cmd == "sco" && argc >= 3) return client.setScoMode(strcmp(argv[2], "on") == 0) ? 0 : 1;
    if (cmd == "status") { client.getStatus(); return 0; }

    std::cerr << "未知命令: " << cmd << std::endl;
    return 1;
}
```

---

### 5. Android.bp 编译配置

**文件路径**: `vendor/qcom/bluetooth_audio_ctrl/Android.bp`

```blueprint
cc_binary {
    name: "bluetooth_audio_ctrl",
    vendor: true,
    relative_install_path: "hw",

    srcs: [
        "BluetoothAudioClient.cpp",
    ],

    cflags: [
        "-Wall",
        "-Werror",
        "-Wno-unused-parameter",
    ],

    shared_libs: [
        // Android 基础库
        "liblog",
        "libutils",
        "libcutils",
        "libbase",

        // HIDL 基础库 (关键!)
        "libhidlbase",
        "libhidltransport",
        "libhwbinder",

        // Audio HAL 2.0 接口库 (关键!)
        "android.hardware.audio@2.0",
        "android.hardware.audio.common@2.0",
        "android.hardware.audio.common@2.0-util",
    ],

    proprietary: true,
    compile_multilib: "both",
    init_rc: ["bluetooth_audio_ctrl.rc"],
}
```

---

### 6. SELinux 策略

**文件路径**: `device/qcom/qcs8250/sepolicy/bluetooth_audio.te`

```te
# 定义类型
type bluetooth_audio_ctrl, domain;
type bluetooth_audio_ctrl_exec, exec_type, vendor_file_type, file_type;

# 域转换
init_daemon_domain(bluetooth_audio_ctrl)
domain_auto_trans(shell, bluetooth_audio_ctrl_exec, bluetooth_audio_ctrl)

# HwBinder 权限 (关键!)
hwbinder_use(bluetooth_audio_ctrl)
hal_client_domain(bluetooth_audio_ctrl, hal_audio)
allow bluetooth_audio_ctrl hal_audio_hwservice:hwservice_manager find;
binder_call(bluetooth_audio_ctrl, hal_audio_default)

# Vendor 文件权限
allow bluetooth_audio_ctrl vendor_file:dir search;
allow bluetooth_audio_ctrl vendor_file:file { read open getattr execute };

# 蓝牙设备权限
allow bluetooth_audio_ctrl bluetooth_device:chr_file { read write open ioctl };

# 日志权限
allow bluetooth_audio_ctrl logd:unix_dgram_socket sendto;

# 属性权限
get_prop(bluetooth_audio_ctrl, default_prop)
set_prop(bluetooth_audio_ctrl, vendor_bluetooth_prop)

# 音频设备权限
allow bluetooth_audio_ctrl audio_device:chr_file { read write open ioctl };
```

**文件路径**: `device/qcom/qcs8250/sepolicy/file_contexts`

```
/vendor/bin/hw/bluetooth_audio_ctrl    u:object_r:bluetooth_audio_ctrl_exec:s0
```

---

## 🔨 编译步骤

```bash
# 1. 进入 Android 源码目录
cd /path/to/android/source

# 2. 设置编译环境
source build/envsetup.sh
lunch qcs8250-userdebug

# 3. 更新 HIDL 接口 (如果修改了 .hal 文件)
cd hardware/interfaces/audio/2.0
hidl-gen -L hash -r android.hardware:hardware/interfaces \
    -r android.hidl:system/libhidl/transport android.hardware.audio@2.0

# 4. 编译 Audio HAL
mmm hardware/interfaces/audio/2.0/default

# 5. 编译客户端工具
mmm vendor/qcom/bluetooth_audio_ctrl

# 6. 编译 SELinux 策略
mmm device/qcom/qcs8250/sepolicy
```

---

## 📱 使用方法

```bash
# 推送到设备
adb push out/target/product/qcs8250/vendor/bin/hw/bluetooth_audio_ctrl /vendor/bin/hw/

# 设置权限
adb shell chmod 755 /vendor/bin/hw/bluetooth_audio_ctrl

# 开启蓝牙音频
adb shell bluetooth_audio_ctrl enable

# 设置编解码器
adb shell bluetooth_audio_ctrl codec aptx

# 设置音量 (80%)
adb shell bluetooth_audio_ctrl volume 0.8

# 连接 A2DP 设备
adb shell bluetooth_audio_ctrl connect AA:BB:CC:DD:EE:FF

# 查询状态
adb shell bluetooth_audio_ctrl status

# 断开连接
adb shell bluetooth_audio_ctrl disconnect

# 关闭蓝牙音频
adb shell bluetooth_audio_ctrl disable
```

---

## ⚠️ 重要提醒

### 1. SELinux 必须配置
如果不配置 SELinux，会出现类似以下错误:
```
avc: denied { find } for service=android.hardware.audio@2.0::IDevicesFactory
```

### 2. 必须初始化 HIDL 线程池
调用任何 HIDL 接口前必须:
```cpp
android::hardware::configureRpcThreadpool(1, false);
```

### 3. 保持 sp<IDevice> 引用
不要让 `sp<IDevice>` 对象提前释放，否则会导致服务断开。

### 4. 底层 HAL 参数映射
需要确保底层 `audio_hw.c` 能够正确处理以下参数:
- `bluetooth_enabled=1/0`
- `bt_a2dp_codec=xxx`
- `bt_a2dp_volume=xxx`
- `bt_a2dp_device=XX:XX:XX:XX:XX:XX`
- `bt_a2dp_disconnect=1`
- `bt_sco_enable=1/0`

---

## 📚 参考链接

- [Android HIDL 官方文档](https://source.android.com/devices/architecture/hidl)
- [Audio HAL 接口定义](https://cs.android.com/android/platform/superproject/+/master:hardware/interfaces/audio/)
- [SELinux 策略编写指南](https://source.android.com/security/selinux)

---

## 📝 License

Apache License 2.0
