/*
 * QCS8250 Bluetooth Audio Control Client
 *
 * 这是一个独立的客户端进程，通过 HIDL 接口调用 Audio HAL 2.0
 * 控制蓝牙音频功能
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
 *   bluetooth_audio_ctrl info                      # 查询设备信息
 */

#define LOG_TAG "BluetoothAudioClient"

#include <android/hardware/audio/2.0/IDevicesFactory.h>
#include <android/hardware/audio/2.0/IDevice.h>
#include <android/hardware/audio/2.0/types.h>
#include <hidl/HidlTransportSupport.h>
#include <hidl/Status.h>
#include <log/log.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

using android::sp;
using android::hardware::Return;
using android::hardware::Void;
using android::hardware::hidl_string;
using android::hardware::hidl_vec;

using android::hardware::audio::V2_0::IDevicesFactory;
using android::hardware::audio::V2_0::IDevice;
using android::hardware::audio::V2_0::Result;

// 颜色输出
#define COLOR_RED     "\033[1;31m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_RESET   "\033[0m"

/**
 * 蓝牙音频控制客户端类
 */
class BluetoothAudioClient {
public:
    BluetoothAudioClient() : mDevice(nullptr), mInitialized(false) {}

    ~BluetoothAudioClient() {
        mDevice = nullptr;
    }

    /**
     * 初始化客户端，获取 HAL 服务
     */
    bool initialize() {
        ALOGI("Initializing BluetoothAudioClient...");

        // 1. 初始化 HIDL 线程池（必须）
        android::hardware::configureRpcThreadpool(1, false /*callerWillJoin*/);

        // 2. 获取 IDevicesFactory 服务
        sp<IDevicesFactory> factory = IDevicesFactory::getService();
        if (factory == nullptr) {
            ALOGE("Failed to get IDevicesFactory service");
            std::cerr << COLOR_RED << "错误: 无法获取 IDevicesFactory 服务" << COLOR_RESET << std::endl;
            return false;
        }
        ALOGI("Got IDevicesFactory service");

        // 3. 打开 primary 设备，获取 IDevice
        Result openResult = Result::NOT_INITIALIZED;
        factory->openDevice("primary", [&](Result r, const sp<IDevice>& d) {
            openResult = r;
            if (r == Result::OK) {
                mDevice = d;
            }
        });

        if (openResult != Result::OK || mDevice == nullptr) {
            ALOGE("Failed to open primary device: %d", static_cast<int>(openResult));
            std::cerr << COLOR_RED << "错误: 无法打开 primary 音频设备" << COLOR_RESET << std::endl;
            return false;
        }

        ALOGI("Opened primary device successfully");
        mInitialized = true;
        return true;
    }

    /**
     * 开启/关闭蓝牙音频
     */
    bool setBluetoothEnabled(bool enable) {
        if (!checkInitialized()) return false;

        ALOGI("setBluetoothEnabled: %d", enable);
        Return<Result> ret = mDevice->openBluetooth(enable);

        if (!ret.isOk()) {
            ALOGE("openBluetooth transport error");
            return false;
        }

        Result result = ret;
        if (result == Result::OK) {
            std::cout << COLOR_GREEN << "蓝牙音频已" << (enable ? "开启" : "关闭")
                      << COLOR_RESET << std::endl;
            return true;
        } else {
            std::cerr << COLOR_RED << "操作失败: " << resultToString(result)
                      << COLOR_RESET << std::endl;
            return false;
        }
    }

    /**
     * 设置蓝牙编解码器
     */
    bool setCodec(const std::string& codec) {
        if (!checkInitialized()) return false;

        ALOGI("setCodec: %s", codec.c_str());
        Return<Result> ret = mDevice->setBluetoothCodec(codec);

        if (!ret.isOk()) {
            ALOGE("setBluetoothCodec transport error");
            return false;
        }

        Result result = ret;
        if (result == Result::OK) {
            std::cout << COLOR_GREEN << "编解码器已设置为: " << codec
                      << COLOR_RESET << std::endl;
            return true;
        } else {
            std::cerr << COLOR_RED << "设置编解码器失败: " << resultToString(result)
                      << COLOR_RESET << std::endl;
            return false;
        }
    }

    /**
     * 设置蓝牙音量
     */
    bool setVolume(float volume) {
        if (!checkInitialized()) return false;

        if (volume < 0.0f || volume > 1.0f) {
            std::cerr << COLOR_RED << "音量范围错误，应为 0.0 ~ 1.0" << COLOR_RESET << std::endl;
            return false;
        }

        ALOGI("setVolume: %f", volume);
        Return<Result> ret = mDevice->setBluetoothVolume(volume);

        if (!ret.isOk()) {
            ALOGE("setBluetoothVolume transport error");
            return false;
        }

        Result result = ret;
        if (result == Result::OK) {
            std::cout << COLOR_GREEN << "蓝牙音量已设置为: " << (volume * 100) << "%"
                      << COLOR_RESET << std::endl;
            return true;
        } else {
            std::cerr << COLOR_RED << "设置音量失败: " << resultToString(result)
                      << COLOR_RESET << std::endl;
            return false;
        }
    }

    /**
     * 连接 A2DP 设备
     */
    bool connectA2dpDevice(const std::string& address) {
        if (!checkInitialized()) return false;

        ALOGI("connectA2dpDevice: %s", address.c_str());
        Return<Result> ret = mDevice->setBluetoothA2dpDevice(address);

        if (!ret.isOk()) {
            ALOGE("setBluetoothA2dpDevice transport error");
            return false;
        }

        Result result = ret;
        if (result == Result::OK) {
            std::cout << COLOR_GREEN << "A2DP 设备已连接: " << address
                      << COLOR_RESET << std::endl;
            return true;
        } else {
            std::cerr << COLOR_RED << "连接失败: " << resultToString(result)
                      << COLOR_RESET << std::endl;
            return false;
        }
    }

    /**
     * 断开 A2DP 设备
     */
    bool disconnectA2dp() {
        if (!checkInitialized()) return false;

        ALOGI("disconnectA2dp");
        Return<Result> ret = mDevice->disconnectBluetoothA2dp();

        if (!ret.isOk()) {
            ALOGE("disconnectBluetoothA2dp transport error");
            return false;
        }

        Result result = ret;
        if (result == Result::OK) {
            std::cout << COLOR_GREEN << "A2DP 已断开" << COLOR_RESET << std::endl;
            return true;
        } else {
            std::cerr << COLOR_RED << "断开失败: " << resultToString(result)
                      << COLOR_RESET << std::endl;
            return false;
        }
    }

    /**
     * 设置 SCO 模式
     */
    bool setScoMode(bool enable) {
        if (!checkInitialized()) return false;

        ALOGI("setScoMode: %d", enable);
        Return<Result> ret = mDevice->setBluetoothScoMode(enable);

        if (!ret.isOk()) {
            ALOGE("setBluetoothScoMode transport error");
            return false;
        }

        Result result = ret;
        if (result == Result::OK) {
            std::cout << COLOR_GREEN << "SCO 模式已" << (enable ? "开启" : "关闭")
                      << COLOR_RESET << std::endl;
            return true;
        } else {
            std::cerr << COLOR_RED << "设置 SCO 失败: " << resultToString(result)
                      << COLOR_RESET << std::endl;
            return false;
        }
    }

    /**
     * 获取蓝牙状态
     */
    bool getStatus() {
        if (!checkInitialized()) return false;

        ALOGI("getStatus");
        bool enabled = false;
        std::string codec;
        Result result = Result::NOT_INITIALIZED;

        mDevice->getBluetoothStatus([&](Result r, bool e, const hidl_string& c) {
            result = r;
            enabled = e;
            codec = c;
        });

        if (result == Result::OK) {
            std::cout << COLOR_BLUE << "========== 蓝牙音频状态 ==========" << COLOR_RESET << std::endl;
            std::cout << "  状态: " << (enabled ? COLOR_GREEN "已开启" : COLOR_YELLOW "已关闭")
                      << COLOR_RESET << std::endl;
            std::cout << "  编解码器: " << COLOR_GREEN << codec << COLOR_RESET << std::endl;
            std::cout << COLOR_BLUE << "==================================" << COLOR_RESET << std::endl;
            return true;
        } else {
            std::cerr << COLOR_RED << "获取状态失败" << COLOR_RESET << std::endl;
            return false;
        }
    }

    /**
     * 获取设备信息
     */
    bool getDeviceInfo() {
        if (!checkInitialized()) return false;

        ALOGI("getDeviceInfo");
        std::string deviceName;
        std::string macAddress;
        bool connected = false;
        Result result = Result::NOT_INITIALIZED;

        mDevice->getBluetoothDeviceInfo([&](Result r, const hidl_string& name,
                                            const hidl_string& addr, bool conn) {
            result = r;
            deviceName = name;
            macAddress = addr;
            connected = conn;
        });

        if (result == Result::OK) {
            std::cout << COLOR_BLUE << "========== 蓝牙设备信息 ==========" << COLOR_RESET << std::endl;
            std::cout << "  设备名称: " << (deviceName.empty() ? "(无)" : deviceName) << std::endl;
            std::cout << "  MAC 地址: " << (macAddress.empty() ? "(无)" : macAddress) << std::endl;
            std::cout << "  连接状态: " << (connected ? COLOR_GREEN "已连接" : COLOR_YELLOW "未连接")
                      << COLOR_RESET << std::endl;
            std::cout << COLOR_BLUE << "==================================" << COLOR_RESET << std::endl;
            return true;
        } else {
            std::cerr << COLOR_RED << "获取设备信息失败" << COLOR_RESET << std::endl;
            return false;
        }
    }

private:
    sp<IDevice> mDevice;
    bool mInitialized;

    bool checkInitialized() {
        if (!mInitialized || mDevice == nullptr) {
            std::cerr << COLOR_RED << "客户端未初始化" << COLOR_RESET << std::endl;
            return false;
        }
        return true;
    }

    const char* resultToString(Result result) {
        switch (result) {
            case Result::OK: return "OK";
            case Result::NOT_INITIALIZED: return "NOT_INITIALIZED";
            case Result::INVALID_ARGUMENTS: return "INVALID_ARGUMENTS";
            case Result::INVALID_STATE: return "INVALID_STATE";
            case Result::NOT_SUPPORTED: return "NOT_SUPPORTED";
            default: return "UNKNOWN";
        }
    }
};

/**
 * 打印使用帮助
 */
void printUsage(const char* progName) {
    std::cout << COLOR_BLUE << "QCS8250 蓝牙音频控制工具" << COLOR_RESET << std::endl;
    std::cout << std::endl;
    std::cout << "用法: " << progName << " <命令> [参数]" << std::endl;
    std::cout << std::endl;
    std::cout << "命令:" << std::endl;
    std::cout << "  enable                    开启蓝牙音频" << std::endl;
    std::cout << "  disable                   关闭蓝牙音频" << std::endl;
    std::cout << "  codec <codec>             设置编解码器 (sbc/aac/aptx/aptx_hd/ldac)" << std::endl;
    std::cout << "  volume <0.0-1.0>          设置音量" << std::endl;
    std::cout << "  connect <MAC>             连接 A2DP 设备 (格式: XX:XX:XX:XX:XX:XX)" << std::endl;
    std::cout << "  disconnect                断开 A2DP 设备" << std::endl;
    std::cout << "  sco <on|off>              设置 SCO 模式" << std::endl;
    std::cout << "  status                    查询蓝牙状态" << std::endl;
    std::cout << "  info                      查询设备信息" << std::endl;
    std::cout << std::endl;
    std::cout << "示例:" << std::endl;
    std::cout << "  " << progName << " enable" << std::endl;
    std::cout << "  " << progName << " codec aptx" << std::endl;
    std::cout << "  " << progName << " volume 0.8" << std::endl;
    std::cout << "  " << progName << " connect AA:BB:CC:DD:EE:FF" << std::endl;
}

/**
 * 主函数
 */
int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string command = argv[1];

    // 帮助命令
    if (command == "-h" || command == "--help" || command == "help") {
        printUsage(argv[0]);
        return 0;
    }

    // 初始化客户端
    BluetoothAudioClient client;
    if (!client.initialize()) {
        return 1;
    }

    // 处理命令
    if (command == "enable") {
        return client.setBluetoothEnabled(true) ? 0 : 1;
    }
    else if (command == "disable") {
        return client.setBluetoothEnabled(false) ? 0 : 1;
    }
    else if (command == "codec") {
        if (argc < 3) {
            std::cerr << COLOR_RED << "请指定编解码器: sbc/aac/aptx/aptx_hd/ldac" << COLOR_RESET << std::endl;
            return 1;
        }
        return client.setCodec(argv[2]) ? 0 : 1;
    }
    else if (command == "volume") {
        if (argc < 3) {
            std::cerr << COLOR_RED << "请指定音量值 (0.0 ~ 1.0)" << COLOR_RESET << std::endl;
            return 1;
        }
        float volume = atof(argv[2]);
        return client.setVolume(volume) ? 0 : 1;
    }
    else if (command == "connect") {
        if (argc < 3) {
            std::cerr << COLOR_RED << "请指定 MAC 地址 (格式: XX:XX:XX:XX:XX:XX)" << COLOR_RESET << std::endl;
            return 1;
        }
        return client.connectA2dpDevice(argv[2]) ? 0 : 1;
    }
    else if (command == "disconnect") {
        return client.disconnectA2dp() ? 0 : 1;
    }
    else if (command == "sco") {
        if (argc < 3) {
            std::cerr << COLOR_RED << "请指定 on 或 off" << COLOR_RESET << std::endl;
            return 1;
        }
        bool enable = (strcmp(argv[2], "on") == 0 || strcmp(argv[2], "1") == 0);
        return client.setScoMode(enable) ? 0 : 1;
    }
    else if (command == "status") {
        return client.getStatus() ? 0 : 1;
    }
    else if (command == "info") {
        return client.getDeviceInfo() ? 0 : 1;
    }
    else {
        std::cerr << COLOR_RED << "未知命令: " << command << COLOR_RESET << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    return 0;
}
