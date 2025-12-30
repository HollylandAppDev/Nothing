# Android 10 Audio HAL 2.0 添加自定义接口指南

## 目标
在 Audio HAL 2.0 中添加 `openBluetooth()` 接口

---

## 一、修改 HIDL 接口定义

### 1.1 修改 IDevice.hal

**文件路径**: `hardware/interfaces/audio/2.0/IDevice.hal`

```hidl
package android.hardware.audio@2.0;

import android.hardware.audio.common@2.0;

interface IDevice {
    // ... 现有接口 ...
    
    /**
     * 初始化检查
     */
    initCheck() generates (Result retval);
    
    /**
     * 设置主音量
     */
    setMasterVolume(float volume) generates (Result retval);
    
    // ... 其他现有接口 ...
    
    /**
     * ========== 新增接口 ==========
     * 打开蓝牙音频
     * @param enable true=开启, false=关闭
     * @return retval 操作结果
     */
    openBluetooth(bool enable) generates (Result retval);
    
    /**
     * 获取蓝牙状态
     * @return retval 操作结果
     * @return enabled 蓝牙是否开启
     */
    getBluetoothStatus() generates (Result retval, bool enabled);
};
```

### 1.2 重新生成 HIDL 代码

```bash
# 进入 Android 源码根目录
cd $ANDROID_BUILD_TOP

# 方法1: 使用 hidl-gen 工具生成
hidl-gen -o hardware/interfaces/audio/2.0/default \
         -Lc++-impl \
         -randroid.hardware:hardware/interfaces \
         -randroid.hidl:system/libhidl/transport \
         android.hardware.audio@2.0

# 方法2: 使用 update-makefiles 更新
hardware/interfaces/update-makefiles.sh

# 方法3: 整体编译时自动生成
make android.hardware.audio@2.0
```

---

## 二、HAL Service 端实现

### 2.1 修改 Device.h

**文件路径**: `hardware/interfaces/audio/2.0/default/Device.h`

```cpp
#ifndef ANDROID_HARDWARE_AUDIO_V2_0_DEVICE_H
#define ANDROID_HARDWARE_AUDIO_V2_0_DEVICE_H

#include <android/hardware/audio/2.0/IDevice.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>

namespace android {
namespace hardware {
namespace audio {
namespace V2_0 {
namespace implementation {

using ::android::hardware::audio::V2_0::IDevice;
using ::android::hardware::audio::V2_0::Result;
using ::android::hardware::Return;
using ::android::hardware::Void;

struct Device : public IDevice {
    // 构造函数
    Device(audio_hw_device_t* device);
    
    // ... 现有方法声明 ...
    
    Return<Result> initCheck() override;
    Return<Result> setMasterVolume(float volume) override;
    Return<void> getMasterVolume(getMasterVolume_cb _hidl_cb) override;
    Return<Result> setMicMute(bool mute) override;
    Return<void> getMicMute(getMicMute_cb _hidl_cb) override;
    
    // ========== 新增方法声明 ==========
    Return<Result> openBluetooth(bool enable) override;
    Return<void> getBluetoothStatus(getBluetoothStatus_cb _hidl_cb) override;
    
private:
    audio_hw_device_t* mDevice;
    bool mBluetoothEnabled;  // 新增：蓝牙状态
};

}  // namespace implementation
}  // namespace V2_0
}  // namespace audio
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_AUDIO_V2_0_DEVICE_H
```

### 2.2 修改 Device.cpp

**文件路径**: `hardware/interfaces/audio/2.0/default/Device.cpp`

```cpp
#include "Device.h"
#include <log/log.h>

namespace android {
namespace hardware {
namespace audio {
namespace V2_0 {
namespace implementation {

Device::Device(audio_hw_device_t* device) 
    : mDevice(device), 
      mBluetoothEnabled(false) {  // 初始化蓝牙状态
}

// ... 现有方法实现 ...

// ========== 新增方法实现 ==========

Return<Result> Device::openBluetooth(bool enable) {
    ALOGD("%s: enable=%d", __func__, enable);
    
    // 方法1: 通过 setParameters 传递给底层 HAL
    // 这是最常用的方式，不需要修改底层 audio_hw.c 接口
    char param[64];
    snprintf(param, sizeof(param), "bluetooth_enabled=%d", enable ? 1 : 0);
    
    int ret = mDevice->set_parameters(mDevice, param);
    if (ret != 0) {
        ALOGE("%s: set_parameters failed, ret=%d", __func__, ret);
        return Result::INVALID_STATE;
    }
    
    mBluetoothEnabled = enable;
    
    // 方法2: 如果底层 HAL 有专门的接口，可以直接调用
    // 需要在 audio_hw_device_t 结构体中添加新函数指针
    /*
    if (mDevice->open_bluetooth != NULL) {
        int ret = mDevice->open_bluetooth(mDevice, enable);
        if (ret != 0) {
            return Result::INVALID_STATE;
        }
    }
    */
    
    ALOGD("%s: success, bluetooth %s", __func__, enable ? "enabled" : "disabled");
    return Result::OK;
}

Return<void> Device::getBluetoothStatus(getBluetoothStatus_cb _hidl_cb) {
    ALOGD("%s: mBluetoothEnabled=%d", __func__, mBluetoothEnabled);
    
    // 方法1: 返回缓存的状态
    _hidl_cb(Result::OK, mBluetoothEnabled);
    
    // 方法2: 从底层 HAL 查询
    /*
    char value[32];
    int ret = mDevice->get_parameters(mDevice, "bluetooth_enabled");
    if (ret != 0) {
        _hidl_cb(Result::INVALID_STATE, false);
    } else {
        bool enabled = (strcmp(value, "1") == 0);
        _hidl_cb(Result::OK, enabled);
    }
    */
    
    return Void();
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace audio
}  // namespace hardware
}  // namespace android
```

---

## 三、AudioFlinger HAL 封装层修改

### 3.1 修改 DeviceHalInterface.h

**文件路径**: `frameworks/av/media/libaudiohal/include/media/audiohal/DeviceHalInterface.h`

```cpp
#ifndef ANDROID_HARDWARE_DEVICE_HAL_INTERFACE_H
#define ANDROID_HARDWARE_DEVICE_HAL_INTERFACE_H

#include <utils/RefBase.h>
#include <utils/Errors.h>

namespace android {

class DeviceHalInterface : public RefBase {
public:
    // ... 现有接口 ...
    
    virtual status_t initCheck() = 0;
    virtual status_t setMasterVolume(float volume) = 0;
    virtual status_t getMasterVolume(float *volume) = 0;
    
    // ========== 新增接口 ==========
    virtual status_t openBluetooth(bool enable) = 0;
    virtual status_t getBluetoothStatus(bool *enabled) = 0;
    
protected:
    virtual ~DeviceHalInterface() {}
};

}  // namespace android

#endif  // ANDROID_HARDWARE_DEVICE_HAL_INTERFACE_H
```

### 3.2 修改 DeviceHalHidl.h

**文件路径**: `frameworks/av/media/libaudiohal/impl/DeviceHalHidl.h`

```cpp
#ifndef ANDROID_HARDWARE_DEVICE_HAL_HIDL_H
#define ANDROID_HARDWARE_DEVICE_HAL_HIDL_H

#include <media/audiohal/DeviceHalInterface.h>
#include <android/hardware/audio/2.0/IDevice.h>

namespace android {

using ::android::hardware::audio::V2_0::IDevice;

class DeviceHalHidl : public DeviceHalInterface {
public:
    DeviceHalHidl(const sp<IDevice>& device);
    
    // ... 现有方法 ...
    
    virtual status_t initCheck() override;
    virtual status_t setMasterVolume(float volume) override;
    
    // ========== 新增方法 ==========
    virtual status_t openBluetooth(bool enable) override;
    virtual status_t getBluetoothStatus(bool *enabled) override;
    
private:
    sp<IDevice> mDevice;
};

}  // namespace android

#endif  // ANDROID_HARDWARE_DEVICE_HAL_HIDL_H
```

### 3.3 修改 DeviceHalHidl.cpp

**文件路径**: `frameworks/av/media/libaudiohal/impl/DeviceHalHidl.cpp`

```cpp
#include "DeviceHalHidl.h"
#include <log/log.h>

namespace android {

using ::android::hardware::audio::V2_0::Result;

// ... 现有实现 ...

// ========== 新增方法实现 ==========

status_t DeviceHalHidl::openBluetooth(bool enable) {
    if (mDevice == nullptr) {
        return NO_INIT;
    }
    
    Result retval;
    Return<Result> ret = mDevice->openBluetooth(enable);
    
    if (!ret.isOk()) {
        ALOGE("%s: HIDL call failed", __func__);
        return FAILED_TRANSACTION;
    }
    
    retval = ret;
    return retval == Result::OK ? OK : INVALID_OPERATION;
}

status_t DeviceHalHidl::getBluetoothStatus(bool *enabled) {
    if (mDevice == nullptr) {
        return NO_INIT;
    }
    
    Result retval;
    Return<void> ret = mDevice->getBluetoothStatus(
        [&](Result r, bool e) {
            retval = r;
            if (enabled != nullptr) {
                *enabled = e;
            }
        });
    
    if (!ret.isOk()) {
        ALOGE("%s: HIDL call failed", __func__);
        return FAILED_TRANSACTION;
    }
    
    return retval == Result::OK ? OK : INVALID_OPERATION;
}

}  // namespace android
```

---

## 四、Vendor HAL 底层实现 (可选)

如果需要在底层 `audio_hw.c` 中处理蓝牙开关：

### 4.1 方法1: 通过 set_parameters 处理

**文件路径**: `device/[vendor]/[device]/audio/audio_hw.c`

```c
static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct str_parms *parms;
    char value[32];
    int ret;
    
    ALOGD("%s: kvpairs=%s", __func__, kvpairs);
    
    parms = str_parms_create_str(kvpairs);
    
    // ========== 处理蓝牙开关 ==========
    ret = str_parms_get_str(parms, "bluetooth_enabled", value, sizeof(value));
    if (ret >= 0) {
        bool enable = (strcmp(value, "1") == 0);
        ALOGD("%s: bluetooth_enabled=%d", __func__, enable);
        
        if (enable) {
            // 开启蓝牙音频
            // 1. 配置蓝牙音频路由
            // 2. 打开蓝牙 PCM 设备
            // 3. 设置蓝牙相关参数
            adev->bluetooth_enabled = true;
            // setup_bluetooth_audio(adev);
        } else {
            // 关闭蓝牙音频
            // 1. 恢复默认音频路由
            // 2. 关闭蓝牙 PCM 设备
            adev->bluetooth_enabled = false;
            // teardown_bluetooth_audio(adev);
        }
    }
    
    // 处理其他参数...
    
    str_parms_destroy(parms);
    return 0;
}
```

### 4.2 方法2: 添加新的函数指针 (需要修改 HAL 头文件)

**文件路径**: `hardware/libhardware/include/hardware/audio.h`

```c
struct audio_hw_device {
    struct hw_device_t common;
    
    // ... 现有函数指针 ...
    
    int (*init_check)(const struct audio_hw_device *dev);
    int (*set_master_volume)(struct audio_hw_device *dev, float volume);
    
    // ========== 新增函数指针 ==========
    /**
     * 打开/关闭蓝牙音频
     * @param enable 1=开启, 0=关闭
     * @return 0 成功, 负值 失败
     */
    int (*open_bluetooth)(struct audio_hw_device *dev, bool enable);
    
    /**
     * 获取蓝牙状态
     * @param enabled 返回蓝牙状态
     * @return 0 成功, 负值 失败
     */
    int (*get_bluetooth_status)(struct audio_hw_device *dev, bool *enabled);
};
```

然后在 `audio_hw.c` 中实现：

```c
static int adev_open_bluetooth(struct audio_hw_device *dev, bool enable)
{
    struct audio_device *adev = (struct audio_device *)dev;
    
    ALOGD("%s: enable=%d", __func__, enable);
    
    pthread_mutex_lock(&adev->lock);
    
    if (enable) {
        // 开启蓝牙
        adev->bluetooth_enabled = true;
        // 具体实现...
    } else {
        // 关闭蓝牙
        adev->bluetooth_enabled = false;
        // 具体实现...
    }
    
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static int adev_get_bluetooth_status(struct audio_hw_device *dev, bool *enabled)
{
    struct audio_device *adev = (struct audio_device *)dev;
    
    if (enabled != NULL) {
        *enabled = adev->bluetooth_enabled;
    }
    
    return 0;
}

// 在 adev_open 中注册函数指针
static int adev_open(const hw_module_t* module, const char* name,
                     hw_device_t** device)
{
    struct audio_device *adev;
    
    adev = calloc(1, sizeof(struct audio_device));
    if (!adev) return -ENOMEM;
    
    adev->device.common.tag = HARDWARE_DEVICE_TAG;
    adev->device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    adev->device.common.module = (struct hw_module_t *)module;
    adev->device.common.close = adev_close;
    
    // ... 现有初始化 ...
    adev->device.init_check = adev_init_check;
    adev->device.set_master_volume = adev_set_master_volume;
    
    // ========== 注册新函数 ==========
    adev->device.open_bluetooth = adev_open_bluetooth;
    adev->device.get_bluetooth_status = adev_get_bluetooth_status;
    
    *device = &adev->device.common;
    return 0;
}
```

---

## 五、AudioFlinger 调用示例

### 5.1 在 AudioFlinger 中调用

**文件路径**: `frameworks/av/services/audioflinger/AudioFlinger.cpp`

```cpp
status_t AudioFlinger::openBluetooth(bool enable) {
    ALOGD("%s: enable=%d", __func__, enable);
    
    Mutex::Autolock _l(mLock);
    
    // 获取主设备
    AudioHwDevice *dev = mAudioHwDevs.valueFor(AUDIO_MODULE_HANDLE_NONE);
    if (dev == nullptr) {
        ALOGE("%s: no primary device", __func__);
        return NO_INIT;
    }
    
    // 调用 HAL 接口
    return dev->hwDevice()->openBluetooth(enable);
}

status_t AudioFlinger::getBluetoothStatus(bool *enabled) {
    ALOGD("%s", __func__);
    
    Mutex::Autolock _l(mLock);
    
    AudioHwDevice *dev = mAudioHwDevs.valueFor(AUDIO_MODULE_HANDLE_NONE);
    if (dev == nullptr) {
        return NO_INIT;
    }
    
    return dev->hwDevice()->getBluetoothStatus(enabled);
}
```

### 5.2 在 IAudioFlinger.aidl 中添加接口 (如需暴露给上层)

**文件路径**: `frameworks/av/media/libaudioclient/aidl/android/media/IAudioFlingerService.aidl`

```aidl
interface IAudioFlingerService {
    // ... 现有接口 ...
    
    // 新增蓝牙控制接口
    int openBluetooth(boolean enable);
    boolean getBluetoothStatus();
}
```

---

## 六、编译和测试

### 6.1 编译命令

```bash
# 编译 HIDL 接口
mmm hardware/interfaces/audio/2.0/

# 编译 HAL Service
mmm hardware/interfaces/audio/2.0/default/

# 编译 AudioFlinger
mmm frameworks/av/services/audioflinger/

# 编译 libaudiohal
mmm frameworks/av/media/libaudiohal/

# 或者整体编译
make -j$(nproc)
```

### 6.2 测试方法

```bash
# 1. 查看日志
adb logcat -s AudioFlinger:* audio_hw:* DeviceHalHidl:*

# 2. 通过 dumpsys 查看状态
adb shell dumpsys media.audio_flinger

# 3. 如果暴露了 Binder 接口，可以通过 service call 测试
adb shell service call audio XX  # XX 是接口编号
```

---

## 七、完整调用流程图

```
┌───────────────────┐
│    App / Service  │
│  (Java/Native)    │
└─────────┬─────────┘
          │ Binder
          ▼
┌───────────────────┐
│   AudioFlinger    │
│ openBluetooth()   │
└─────────┬─────────┘
          │
          ▼
┌───────────────────┐
│  DeviceHalHidl    │
│ openBluetooth()   │
└─────────┬─────────┘
          │ HIDL (hwbinder)
          ▼
┌───────────────────────────────────┐
│  android.hardware.audio@2.0      │
│  Device::openBluetooth()         │
└─────────┬─────────────────────────┘
          │
          ▼
┌───────────────────┐
│   audio_hw.c      │
│ set_parameters()  │
│ 或 open_bluetooth()│
└─────────┬─────────┘
          │
          ▼
┌───────────────────┐
│  蓝牙音频处理      │
│  (A2DP/SCO/etc)   │
└───────────────────┘
```

---

## 八、注意事项

1. **接口兼容性**: 修改 HIDL 接口会破坏二进制兼容，需要同时更新 HAL Service 和 Framework

2. **推荐方式**: 如果只是传递参数，建议使用现有的 `setParameters()` 接口，无需修改 HIDL

3. **蓝牙音频**: 实际的蓝牙音频通常由 `audio.a2dp.default.so` 或蓝牙协议栈管理

4. **SELinux**: 新增接口可能需要更新 SELinux 策略

5. **版本号**: 如果修改了 HIDL 接口，考虑升级版本号 (如 2.0 -> 2.1)

---

## 九、替代方案：使用现有 setParameters

如果不想修改 HIDL 接口，可以直接使用现有的 `setParameters`：

```cpp
// AudioFlinger 侧
status_t AudioFlinger::openBluetooth(bool enable) {
    String8 params = String8::format("bluetooth_enabled=%d", enable ? 1 : 0);
    return mPrimaryHardwareDev->hwDevice()->setParameters(params);
}

// audio_hw.c 侧只需要处理 set_parameters 中的 "bluetooth_enabled" 参数
```

这种方式**不需要修改任何 HIDL 接口**，是最简单的实现方式。
