/*
 * QCS8250 Bluetooth Audio Control Client Library Implementation
 */

#define LOG_TAG "BluetoothAudioClientLib"

#include "include/BluetoothAudioClient.h"

#include <android/hardware/audio/2.0/IDevicesFactory.h>
#include <android/hardware/audio/2.0/IDevice.h>
#include <android/hardware/audio/2.0/types.h>
#include <hidl/HidlTransportSupport.h>
#include <hidl/Status.h>
#include <log/log.h>

#include <mutex>

using android::sp;
using android::hardware::Return;
using android::hardware::Void;
using android::hardware::hidl_string;
using android::hardware::hidl_vec;

using android::hardware::audio::V2_0::IDevicesFactory;
using android::hardware::audio::V2_0::IDevice;
using android::hardware::audio::V2_0::Result;

namespace qcom {
namespace bluetooth {

/**
 * 实现类 (PIMPL 模式)
 */
class BluetoothAudioClient::Impl {
public:
    Impl() : mDevice(nullptr), mInitialized(false) {}

    ~Impl() {
        mDevice = nullptr;
    }

    bool initialize() {
        std::lock_guard<std::mutex> lock(mMutex);

        if (mInitialized) {
            ALOGW("Already initialized");
            return true;
        }

        ALOGI("Initializing BluetoothAudioClient...");

        // 初始化 HIDL 线程池
        static bool rpcInitialized = false;
        if (!rpcInitialized) {
            android::hardware::configureRpcThreadpool(1, false);
            rpcInitialized = true;
        }

        // 获取 IDevicesFactory 服务
        sp<IDevicesFactory> factory = IDevicesFactory::getService();
        if (factory == nullptr) {
            ALOGE("Failed to get IDevicesFactory service");
            return false;
        }

        // 打开 primary 设备
        Result openResult = Result::NOT_INITIALIZED;
        factory->openDevice("primary", [&](Result r, const sp<IDevice>& d) {
            openResult = r;
            if (r == Result::OK) {
                mDevice = d;
            }
        });

        if (openResult != Result::OK || mDevice == nullptr) {
            ALOGE("Failed to open primary device");
            return false;
        }

        mInitialized = true;
        ALOGI("BluetoothAudioClient initialized successfully");
        return true;
    }

    bool isInitialized() const {
        return mInitialized;
    }

    bool enable() {
        return setEnabled(true);
    }

    bool disable() {
        return setEnabled(false);
    }

    bool setEnabled(bool enable) {
        if (!checkInit()) return false;

        Return<Result> ret = mDevice->openBluetooth(enable);
        if (!ret.isOk()) {
            ALOGE("openBluetooth transport error");
            return false;
        }
        return (Result)ret == Result::OK;
    }

    bool setCodec(const std::string& codec) {
        if (!checkInit()) return false;

        Return<Result> ret = mDevice->setBluetoothCodec(codec);
        if (!ret.isOk()) {
            ALOGE("setBluetoothCodec transport error");
            return false;
        }
        return (Result)ret == Result::OK;
    }

    bool setVolume(float volume) {
        if (!checkInit()) return false;

        if (volume < 0.0f || volume > 1.0f) {
            ALOGE("Invalid volume: %f", volume);
            return false;
        }

        Return<Result> ret = mDevice->setBluetoothVolume(volume);
        if (!ret.isOk()) {
            ALOGE("setBluetoothVolume transport error");
            return false;
        }
        return (Result)ret == Result::OK;
    }

    bool connectA2dpDevice(const std::string& address) {
        if (!checkInit()) return false;

        Return<Result> ret = mDevice->setBluetoothA2dpDevice(address);
        if (!ret.isOk()) {
            ALOGE("setBluetoothA2dpDevice transport error");
            return false;
        }
        return (Result)ret == Result::OK;
    }

    bool disconnectA2dp() {
        if (!checkInit()) return false;

        Return<Result> ret = mDevice->disconnectBluetoothA2dp();
        if (!ret.isOk()) {
            ALOGE("disconnectBluetoothA2dp transport error");
            return false;
        }
        return (Result)ret == Result::OK;
    }

    bool setScoMode(bool enable) {
        if (!checkInit()) return false;

        Return<Result> ret = mDevice->setBluetoothScoMode(enable);
        if (!ret.isOk()) {
            ALOGE("setBluetoothScoMode transport error");
            return false;
        }
        return (Result)ret == Result::OK;
    }

    BluetoothAudioStatus getStatus() {
        BluetoothAudioStatus status = {false, ""};

        if (!checkInit()) return status;

        mDevice->getBluetoothStatus([&](Result r, bool e, const hidl_string& c) {
            if (r == Result::OK) {
                status.enabled = e;
                status.codec = c;
            }
        });

        return status;
    }

    BluetoothDeviceInfo getDeviceInfo() {
        BluetoothDeviceInfo info = {"", "", false};

        if (!checkInit()) return info;

        mDevice->getBluetoothDeviceInfo([&](Result r, const hidl_string& name,
                                            const hidl_string& addr, bool conn) {
            if (r == Result::OK) {
                info.deviceName = name;
                info.macAddress = addr;
                info.connected = conn;
            }
        });

        return info;
    }

private:
    sp<IDevice> mDevice;
    bool mInitialized;
    mutable std::mutex mMutex;

    bool checkInit() {
        if (!mInitialized || mDevice == nullptr) {
            ALOGE("Not initialized");
            return false;
        }
        return true;
    }
};

// BluetoothAudioClient 实现

BluetoothAudioClient::BluetoothAudioClient()
    : mImpl(std::make_unique<Impl>()) {
}

BluetoothAudioClient::~BluetoothAudioClient() = default;

bool BluetoothAudioClient::initialize() {
    return mImpl->initialize();
}

bool BluetoothAudioClient::isInitialized() const {
    return mImpl->isInitialized();
}

bool BluetoothAudioClient::enable() {
    return mImpl->enable();
}

bool BluetoothAudioClient::disable() {
    return mImpl->disable();
}

bool BluetoothAudioClient::setCodec(const std::string& codec) {
    return mImpl->setCodec(codec);
}

bool BluetoothAudioClient::setVolume(float volume) {
    return mImpl->setVolume(volume);
}

bool BluetoothAudioClient::connectA2dpDevice(const std::string& address) {
    return mImpl->connectA2dpDevice(address);
}

bool BluetoothAudioClient::disconnectA2dp() {
    return mImpl->disconnectA2dp();
}

bool BluetoothAudioClient::setScoMode(bool enable) {
    return mImpl->setScoMode(enable);
}

BluetoothAudioStatus BluetoothAudioClient::getStatus() {
    return mImpl->getStatus();
}

BluetoothDeviceInfo BluetoothAudioClient::getDeviceInfo() {
    return mImpl->getDeviceInfo();
}

}  // namespace bluetooth
}  // namespace qcom
