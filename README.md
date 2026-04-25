# QtFFmpegPlayer

一个基于 Qt 和 FFmpeg 的本地视频播放器示例项目，用于学习桌面端播放器的基础实现，包括：

- 本地视频文件打开
- 视频解码与画面渲染
- 音频解码与播放
- 播放 / 暂停控制
- 基础音视频同步
- 项目内私有 FFmpeg 依赖管理

当前项目主要面向 macOS 开发和验证。

## 当前功能

- 支持打开本地常见视频文件
- 支持视频画面显示
- 支持音频播放
- 支持播放与暂停
- 支持基础音视频同步
- 工程编译时直接使用仓库内打包好的 FFmpeg，不依赖系统单独安装 FFmpeg

## 技术栈

- Qt Widgets
- Qt Multimedia
- FFmpeg
- CMake
- C++17

## 依赖版本与验证环境

当前已验证的开发环境如下：

- Qt: `6.11.0`
- CMake: `3.30.5`
- FFmpeg: `8.1`
- 操作系统: `macOS 26.2`
- 架构: `arm64`
- Xcode: `26.2`

当前工程中实际链接使用的 FFmpeg 主要动态库版本：

- `libavformat.62`
- `libavcodec.62`
- `libavutil.60`
- `libswscale.9`
- `libswresample.6`

## 构建说明

### 1. 准备环境

请先确保本机已经安装：

- Qt 开发环境
- CMake
- Xcode Command Line Tools 或完整 Xcode

说明：

- 当前仓库已经内置私有 FFmpeg
- 正常构建时不需要再通过 Homebrew 单独安装 FFmpeg
- 当前 CMake 工程目标名仍为 `OpenGLAndFFmpeg`

### 2. 构建私有 FFmpeg

如果仓库中的 `3rdparty/ffmpeg/macos-arm64` 已经存在完整头文件和库文件，这一步通常可以跳过。

如需重新生成，可执行：

```bash
./3rdparty/ffmpeg/scripts/build_ffmpeg_macos.sh
```

脚本详细说明见：

- [3rdparty/ffmpeg/README.md](3rdparty/ffmpeg/README.md)

### 3. 配置并编译工程

可以使用 Qt Creator 直接打开工程，也可以使用 CMake 构建。

示例命令：

```bash
cmake -S . -B build/Release -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release
```

如果使用 Qt 自带 CMake，也可以替换为对应的 CMake 可执行文件路径。

### 4. 运行

构建完成后，可执行文件或 `.app` 会出现在对应的构建目录中。

## FFmpeg 依赖说明

本项目的 CMake 配置已经改为优先并直接使用仓库中的私有 FFmpeg 路径：

- Apple Silicon macOS: `3rdparty/ffmpeg/macos-arm64`
- Intel macOS: `3rdparty/ffmpeg/macos-x64`

当前这份工程的主要目标是：

- 让项目在另一台已安装 Qt 的 macOS 电脑上可以直接编译
- 避免构建阶段依赖系统环境中的 Homebrew FFmpeg

## 项目结构

```text
.
├── 3rdparty/
│   └── ffmpeg/                  私有 FFmpeg 及构建脚本
├── audiobufferdevice.*         音频缓冲区
├── audiooutputcontroller.*     Qt 音频输出控制
├── ffmpegplayer.*              FFmpeg 解码与播放线程核心
├── main.cpp                    程序入口
├── videowidget.*               视频画面显示控件
├── widgetvideo.*               主界面与交互控制
├── CMakeLists.txt              CMake 构建配置
└── OpenGLAndFFmpeg_zh_CN.ts    翻译文件
```

## 功能流程概览

播放器当前主链路可以概括为：

1. UI 层选择本地视频文件
2. `FfmpegPlayer` 打开媒体文件并读取流信息
3. FFmpeg 分别解码视频流和音频流
4. 视频帧转换为 `QImage` 后交给 `VideoWidget` 显示
5. 音频帧重采样为 PCM 后交给 `QAudioSink` 播放
6. 播放线程支持播放、暂停和基础音视频同步

## 当前定位

这个项目当前更偏向：

- Qt + FFmpeg 播放器入门学习
- 本地播放器基础架构练习
- 后续继续扩展进度条、拖动播放、打包分发的基础工程

## 后续可扩展方向

- 播放进度条和当前时间显示
- 拖动播放
- 停止播放与播放结束处理
- 更完整的音视频同步策略
- OpenGL 渲染优化
- macOS `.app` 打包与分发

## 说明

- 当前项目以学习和工程实践为主
- 当前验证重点放在 macOS 平台
- 如果后续需要支持 Windows，可以再单独补充 Windows 对应的私有 FFmpeg 目录和构建脚本
