/*
 * QCS8250 Bluetooth Audio Control Client Library
 *
 * 这是一个库版本，可以被其他进程链接使用
 */

#ifndef BLUETOOTH_AUDIO_CLIENT_H
#define BLUETOOTH_AUDIO_CLIENT_H

#include <string>
#include <memory>

namespace qcom {
namespace bluetooth {

/**
 * 蓝牙音频状态
 */
struct BluetoothAudioStatus {
    bool enabled;
    std::string codec;
};

/**
 * 蓝牙设备信息
 */
struct BluetoothDeviceInfo {
    std::string deviceName;
    std::string macAddress;
    bool connected;
};

/**
 * 蓝牙音频控制客户端类
 *
 * 使用方法:
 *   BluetoothAudioClient client;
 *   if (client.initialize()) {
 *       client.enable();
 *       client.setCodec("aptx");
 *       client.setVolume(0.8f);
 *   }
 */
class BluetoothAudioClient {
public:
    BluetoothAudioClient();
    ~BluetoothAudioClient();

    /**
     * 初始化客户端，获取 HAL 服务
     * @return true 成功, false 失败
     */
    bool initialize();

    /**
     * 检查是否已初始化
     */
    bool isInitialized() const;

    /**
     * 开启蓝牙音频
     */
    bool enable();

    /**
     * 关闭蓝牙音频
     */
    bool disable();

    /**
     * 设置蓝牙编解码器
     * @param codec 编解码器类型: "sbc", "aac", "aptx", "aptx_hd", "ldac"
     */
    bool setCodec(const std::string& codec);

    /**
     * 设置蓝牙音量
     * @param volume 音量值 0.0f ~ 1.0f
     */
    bool setVolume(float volume);

    /**
     * 连接 A2DP 设备
     * @param address MAC 地址 (格式: "XX:XX:XX:XX:XX:XX")
     */
    bool connectA2dpDevice(const std::string& address);

    /**
     * 断开 A2DP 设备
     */
    bool disconnectA2dp();

    /**
     * 设置 SCO 模式
     * @param enable true 开启, false 关闭
     */
    bool setScoMode(bool enable);

    /**
     * 获取蓝牙音频状态
     */
    BluetoothAudioStatus getStatus();

    /**
     * 获取蓝牙设备信息
     */
    BluetoothDeviceInfo getDeviceInfo();

private:
    class Impl;
    std::unique_ptr<Impl> mImpl;
};

}  // namespace bluetooth
}  // namespace qcom

#endif  // BLUETOOTH_AUDIO_CLIENT_H
