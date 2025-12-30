# Android 10 Audio HAL 2.0 添加自定义接口指南

## 目标
在 Audio HAL 2.0 中添加 `openBluetooth()` 接口，并从**自定义进程**直接调用

## 架构图

```
┌─────────────────────────────────┐
│      你的自定义进程              │
│   (HIDL Client / 直接调用)      │
│                                 │
│  sp<IDevice> device =           │
│    IDevice::getService();       │
│  device->openBluetooth(true);   │
└───────────────┬─────────────────┘
                │ HIDL (hwbinder)
                ▼
┌─────────────────────────────────┐
│  android.hardware.audio@2.0    │
│         -service               │
│  ┌───────────────────────────┐ │
│  │ Device::openBluetooth()   │ │
│  └───────────────────────────┘ │
└───────────────┬─────────────────┘
                │
                ▼
┌─────────────────────────────────┐
│         audio_hw.c              │
│    (Vendor HAL 实现)            │
└─────────────────────────────────┘
```

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

## 五、自定义进程调用 HIDL 接口

### 5.1 客户端代码示例

**文件路径**: `vendor/[your_company]/bluetooth_ctrl/BluetoothAudioClient.cpp`

```cpp
#define LOG_TAG "BluetoothAudioClient"

#include <android/hardware/audio/2.0/IDevicesFactory.h>
#include <android/hardware/audio/2.0/IDevice.h>
#include <android/hardware/audio/2.0/types.h>
#include <hidl/HidlTransportSupport.h>
#include <log/log.h>

using android::sp;
using android::hardware::Return;
using android::hardware::Void;
using android::hardware::audio::V2_0::IDevicesFactory;
using android::hardware::audio::V2_0::IDevice;
using android::hardware::audio::V2_0::Result;

class BluetoothAudioClient {
public:
    BluetoothAudioClient() : mDevice(nullptr) {
        init();
    }

    ~BluetoothAudioClient() {
        mDevice = nullptr;
    }

    /**
     * 初始化：获取 Audio HAL 服务
     */
    bool init() {
        // 1. 获取 IDevicesFactory 服务
        sp<IDevicesFactory> factory = IDevicesFactory::getService();
        if (factory == nullptr) {
            ALOGE("Failed to get IDevicesFactory service");
            return false;
        }
        ALOGD("Got IDevicesFactory service");

        // 2. 打开 primary 设备
        Result retval;
        Return<void> ret = factory->openDevice(
            "primary",  // 设备名称: "primary", "a2dp", "usb" 等
            [&](Result r, const sp<IDevice>& device) {
                retval = r;
                if (r == Result::OK) {
                    mDevice = device;
                }
            });

        if (!ret.isOk()) {
            ALOGE("openDevice HIDL call failed");
            return false;
        }

        if (retval != Result::OK) {
            ALOGE("openDevice failed, result=%d", retval);
            return false;
        }

        ALOGD("Successfully opened primary device");
        return true;
    }

    /**
     * 打开/关闭蓝牙
     */
    bool openBluetooth(bool enable) {
        if (mDevice == nullptr) {
            ALOGE("Device not initialized");
            return false;
        }

        ALOGD("openBluetooth: enable=%d", enable);

        // 调用 HIDL 接口
        Return<Result> ret = mDevice->openBluetooth(enable);

        if (!ret.isOk()) {
            ALOGE("openBluetooth HIDL call failed");
            return false;
        }

        Result result = ret;
        if (result != Result::OK) {
            ALOGE("openBluetooth failed, result=%d", result);
            return false;
        }

        ALOGD("openBluetooth success");
        return true;
    }

    /**
     * 获取蓝牙状态
     */
    bool getBluetoothStatus(bool* enabled) {
        if (mDevice == nullptr) {
            ALOGE("Device not initialized");
            return false;
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
            ALOGE("getBluetoothStatus HIDL call failed");
            return false;
        }

        return retval == Result::OK;
    }

private:
    sp<IDevice> mDevice;
};

// ========== main 函数示例 ==========
int main(int argc, char** argv) {
    ALOGD("BluetoothAudioClient starting...");

    // 初始化 HIDL
    android::hardware::configureRpcThreadpool(1, true /* callerWillJoin */);

    // 创建客户端
    BluetoothAudioClient client;

    // 测试调用
    if (argc > 1) {
        bool enable = (strcmp(argv[1], "on") == 0 || strcmp(argv[1], "1") == 0);
        ALOGD("Setting bluetooth: %s", enable ? "ON" : "OFF");
        
        if (client.openBluetooth(enable)) {
            ALOGD("openBluetooth succeeded");
        } else {
            ALOGE("openBluetooth failed");
        }
    }

    // 获取状态
    bool status = false;
    if (client.getBluetoothStatus(&status)) {
        ALOGD("Bluetooth status: %s", status ? "enabled" : "disabled");
    }

    // 如果是服务进程，加入线程池
    // android::hardware::joinRpcThreadpool();

    return 0;
}
```

### 5.2 Android.bp 编译配置

**文件路径**: `vendor/[your_company]/bluetooth_ctrl/Android.bp`

```blueprint
cc_binary {
    name: "bluetooth_audio_ctrl",
    vendor: true,  // 放在 vendor 分区
    
    srcs: [
        "BluetoothAudioClient.cpp",
    ],
    
    shared_libs: [
        // 基础库
        "liblog",
        "libutils",
        "libcutils",
        "libbase",
        
        // HIDL 相关库
        "libhidlbase",
        "libhidltransport",
        "libhwbinder",
        
        // Audio HAL 2.0 接口库
        "android.hardware.audio@2.0",
        "android.hardware.audio.common@2.0",
        "android.hardware.audio.common@2.0-util",
    ],
    
    // 头文件路径
    include_dirs: [
        "hardware/interfaces/audio/2.0/default",
    ],
    
    cflags: [
        "-Wall",
        "-Werror",
        "-Wno-unused-parameter",
    ],
}
```

### 5.3 如果是作为服务进程运行

**文件路径**: `vendor/[your_company]/bluetooth_ctrl/BluetoothAudioService.cpp`

```cpp
#define LOG_TAG "BluetoothAudioService"

#include <android/hardware/audio/2.0/IDevicesFactory.h>
#include <android/hardware/audio/2.0/IDevice.h>
#include <hidl/HidlTransportSupport.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <log/log.h>

using namespace android;
using namespace android::hardware::audio::V2_0;

sp<IDevice> gAudioDevice = nullptr;

// 初始化 Audio HAL 连接
bool initAudioHal() {
    sp<IDevicesFactory> factory = IDevicesFactory::getService();
    if (factory == nullptr) {
        ALOGE("Cannot get IDevicesFactory");
        return false;
    }

    Result retval;
    factory->openDevice("primary",
        [&](Result r, const sp<IDevice>& device) {
            retval = r;
            if (r == Result::OK) {
                gAudioDevice = device;
            }
        });

    return (retval == Result::OK && gAudioDevice != nullptr);
}

// 暴露给其他进程调用的接口 (通过 Binder)
class BluetoothAudioService : public BBinder {
public:
    enum {
        OPEN_BLUETOOTH = IBinder::FIRST_CALL_TRANSACTION,
        GET_BLUETOOTH_STATUS,
    };

    status_t onTransact(uint32_t code, const Parcel& data, 
                        Parcel* reply, uint32_t flags) override {
        switch (code) {
            case OPEN_BLUETOOTH: {
                bool enable = data.readBool();
                ALOGD("OPEN_BLUETOOTH: enable=%d", enable);
                
                if (gAudioDevice != nullptr) {
                    Return<Result> ret = gAudioDevice->openBluetooth(enable);
                    reply->writeInt32(ret.isOk() && ret == Result::OK ? 0 : -1);
                } else {
                    reply->writeInt32(-1);
                }
                return NO_ERROR;
            }
            case GET_BLUETOOTH_STATUS: {
                bool enabled = false;
                if (gAudioDevice != nullptr) {
                    gAudioDevice->getBluetoothStatus(
                        [&](Result r, bool e) {
                            if (r == Result::OK) enabled = e;
                        });
                }
                reply->writeBool(enabled);
                return NO_ERROR;
            }
            default:
                return BBinder::onTransact(code, data, reply, flags);
        }
    }
};

int main() {
    ALOGD("BluetoothAudioService starting...");

    // 初始化 HIDL 线程池
    hardware::configureRpcThreadpool(4, false);

    // 初始化 Binder
    sp<ProcessState> proc(ProcessState::self());
    proc->startThreadPool();

    // 初始化 Audio HAL
    if (!initAudioHal()) {
        ALOGE("Failed to init Audio HAL");
        return -1;
    }

    // 注册服务
    sp<BluetoothAudioService> service = new BluetoothAudioService();
    defaultServiceManager()->addService(String16("bluetooth_audio"), service);

    ALOGD("BluetoothAudioService is running...");

    // 主线程加入
    IPCThreadState::self()->joinThreadPool();
    return 0;
}
```

### 5.4 init.rc 配置（服务自启动）

**文件路径**: `vendor/[your_company]/bluetooth_ctrl/bluetooth_audio.rc`

```rc
service bluetooth_audio_ctrl /vendor/bin/bluetooth_audio_ctrl
    class main
    user system
    group system audio bluetooth
    disabled
    oneshot

# 或者作为常驻服务
service bluetooth_audio_svc /vendor/bin/bluetooth_audio_service
    class hal
    user system
    group system audio bluetooth
```

### 5.5 SELinux 权限配置

**文件路径**: `device/[vendor]/[device]/sepolicy/bluetooth_audio.te`

```te
# 定义新的类型
type bluetooth_audio_ctrl, domain;
type bluetooth_audio_ctrl_exec, exec_type, vendor_file_type, file_type;

# 入口规则
init_daemon_domain(bluetooth_audio_ctrl)

# 允许访问 hwbinder
hwbinder_use(bluetooth_audio_ctrl)

# 允许调用 audio HAL
hal_client_domain(bluetooth_audio_ctrl, hal_audio)

# 允许访问 audio HAL 服务
allow bluetooth_audio_ctrl hal_audio_hwservice:hwservice_manager find;

# 允许使用 HwBinder
allow bluetooth_audio_ctrl hwservicemanager:binder { call transfer };

# 日志权限
allow bluetooth_audio_ctrl log_device:chr_file { write open };
```

**文件路径**: `device/[vendor]/[device]/sepolicy/file_contexts`

```
/vendor/bin/bluetooth_audio_ctrl    u:object_r:bluetooth_audio_ctrl_exec:s0
```

**文件路径**: `device/[vendor]/[device]/sepolicy/hwservice_contexts`

```
# 如果需要暴露新的 hwservice
android.hardware.audio::IDevice    u:object_r:hal_audio_hwservice:s0
```

---

## 六、编译和测试

### 6.1 编译命令

```bash
# 1. 编译 HIDL 接口 (修改 IDevice.hal 后)
mmm hardware/interfaces/audio/2.0/

# 2. 编译 HAL Service (修改 Device.cpp 后)
mmm hardware/interfaces/audio/2.0/default/

# 3. 编译你的客户端程序
mmm vendor/[your_company]/bluetooth_ctrl/

# 或者整体编译
make -j$(nproc)
```

### 6.2 推送测试

```bash
# 推送修改后的 HAL Service
adb root
adb remount
adb push out/target/product/[device]/vendor/bin/hw/android.hardware.audio@2.0-service /vendor/bin/hw/

# 推送你的客户端程序
adb push out/target/product/[device]/vendor/bin/bluetooth_audio_ctrl /vendor/bin/

# 重启 HAL Service
adb shell stop
adb shell start

# 或者只重启 audio HAL
adb shell "pkill -9 android.hardware.audio@2.0-service"
# HAL 会被 hwservicemanager 自动重启
```

### 6.3 运行测试

```bash
# 运行你的程序
adb shell /vendor/bin/bluetooth_audio_ctrl on
adb shell /vendor/bin/bluetooth_audio_ctrl off

# 查看日志
adb logcat -s BluetoothAudioClient:* audio_hw:* Device:*

# 查看 HAL 服务状态
adb shell lshal | grep audio
```

### 6.4 调试技巧

```bash
# 1. 检查 HAL 服务是否运行
adb shell ps -A | grep audio

# 2. 检查 HIDL 接口是否可用
adb shell lshal debug android.hardware.audio@2.0::IDevicesFactory/default

# 3. 检查 SELinux 是否阻止
adb shell dmesg | grep -i denied
adb shell setenforce 0  # 临时关闭 SELinux 测试

# 4. 查看 hwbinder 调用
adb shell "echo 1 > /sys/kernel/debug/tracing/events/binder/enable"
adb shell cat /sys/kernel/debug/tracing/trace
```

---

## 七、完整调用流程图（自定义进程版本）

```
┌─────────────────────────────────────┐
│       你的自定义进程                 │
│    bluetooth_audio_ctrl             │
│                                     │
│  1. IDevicesFactory::getService()   │
│  2. factory->openDevice("primary")  │
│  3. device->openBluetooth(true)     │
└─────────────────┬───────────────────┘
                  │ 
                  │ HIDL (hwbinder IPC)
                  │
                  ▼
┌─────────────────────────────────────┐
│  android.hardware.audio@2.0-service │
│                                     │
│  Device::openBluetooth(bool enable) │
│      │                              │
│      ▼                              │
│  mDevice->set_parameters(           │
│    "bluetooth_enabled=1")           │
└─────────────────┬───────────────────┘
                  │
                  ▼
┌─────────────────────────────────────┐
│          audio_hw.c                 │
│   (Vendor 底层 HAL 实现)             │
│                                     │
│  adev_set_parameters() {            │
│    // 解析 bluetooth_enabled        │
│    // 执行蓝牙音频开关逻辑           │
│  }                                  │
└─────────────────┬───────────────────┘
                  │
                  ▼
┌─────────────────────────────────────┐
│        蓝牙音频处理                  │
│    (A2DP/SCO/HFP 等)                │
└─────────────────────────────────────┘
```

---

## 八、注意事项

### 8.1 自定义进程调用的关键点

1. **HIDL 初始化**: 必须调用 `configureRpcThreadpool()` 初始化 HIDL 线程池

2. **服务获取顺序**: 先获取 `IDevicesFactory`，再通过它打开 `IDevice`

3. **设备名称**: `openDevice()` 参数通常是 `"primary"`，不同设备可能不同

4. **生命周期**: 保持 `sp<IDevice>` 引用，避免被释放

### 8.2 权限相关

1. **SELinux**: 必须配置正确的 SELinux 策略，否则会被拒绝访问

2. **用户/组**: 进程需要 `system` 或 `audio` 组权限

3. **hwbinder**: 需要 `hwbinder_use()` 权限

### 8.3 其他注意事项

1. **接口兼容性**: 修改 HIDL 接口会破坏二进制兼容，需要同时更新 HAL Service

2. **蓝牙音频**: 实际的蓝牙音频通常由 `audio.a2dp.default.so` 或蓝牙协议栈管理

3. **版本号**: 如果修改了 HIDL 接口，考虑升级版本号 (如 2.0 -> 2.1)

4. **与 AudioFlinger 冲突**: 你的进程和 AudioFlinger 都在调用同一个 HAL，注意状态同步

---

## 九、替代方案：使用现有 setParameters

如果不想修改 HIDL 接口，可以从自定义进程直接调用现有的 `setParameters`：

```cpp
#include <android/hardware/audio/2.0/IDevicesFactory.h>
#include <android/hardware/audio/2.0/IDevice.h>

using namespace android::hardware::audio::V2_0;

int main() {
    android::hardware::configureRpcThreadpool(1, true);
    
    // 获取服务
    sp<IDevicesFactory> factory = IDevicesFactory::getService();
    sp<IDevice> device;
    
    factory->openDevice("primary", [&](Result r, const sp<IDevice>& d) {
        if (r == Result::OK) device = d;
    });
    
    // 使用现有的 setParameters 接口
    hidl_vec<ParameterValue> params;
    params.resize(1);
    params[0].key = "bluetooth_enabled";
    params[0].value = "1";  // 或 "0"
    
    Return<Result> ret = device->setParameters(params);
    
    return 0;
}
```

这种方式**不需要修改任何 HIDL 接口**，只需要底层 `audio_hw.c` 处理该参数。

---

## 十、文件清单总结

### 需要修改的文件

| 序号 | 文件 | 说明 |
|------|------|------|
| 1 | `hardware/interfaces/audio/2.0/IDevice.hal` | 添加 HIDL 接口定义 |
| 2 | `hardware/interfaces/audio/2.0/default/Device.h` | HAL Service 头文件 |
| 3 | `hardware/interfaces/audio/2.0/default/Device.cpp` | HAL Service 实现 |
| 4 | `device/[vendor]/[device]/audio/audio_hw.c` | 底层 HAL 处理 (可选) |

### 需要新增的文件

| 序号 | 文件 | 说明 |
|------|------|------|
| 1 | `vendor/xxx/bluetooth_ctrl/BluetoothAudioClient.cpp` | 你的客户端代码 |
| 2 | `vendor/xxx/bluetooth_ctrl/Android.bp` | 编译配置 |
| 3 | `device/xxx/sepolicy/bluetooth_audio.te` | SELinux 策略 |
| 4 | `device/xxx/sepolicy/file_contexts` | 文件上下文 |

---

## 十一、快速开始

```bash
# 1. 修改 HIDL 接口
vim hardware/interfaces/audio/2.0/IDevice.hal

# 2. 修改 HAL Service
vim hardware/interfaces/audio/2.0/default/Device.cpp

# 3. 创建你的客户端目录
mkdir -p vendor/mycompany/bluetooth_ctrl

# 4. 创建客户端代码和编译配置
vim vendor/mycompany/bluetooth_ctrl/BluetoothAudioClient.cpp
vim vendor/mycompany/bluetooth_ctrl/Android.bp

# 5. 配置 SELinux
vim device/myvendor/mydevice/sepolicy/bluetooth_audio.te

# 6. 编译
mmm hardware/interfaces/audio/2.0/
mmm hardware/interfaces/audio/2.0/default/
mmm vendor/mycompany/bluetooth_ctrl/

# 7. 推送测试
adb root && adb remount
adb push ... /vendor/bin/hw/
adb push ... /vendor/bin/
adb shell stop && adb shell start
adb shell /vendor/bin/bluetooth_audio_ctrl on
```
