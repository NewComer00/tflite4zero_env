# **tflite4zero_env**<br>在树莓派zero(armv6)上开发Tensorflow Lite项目的C++环境

[![zh](https://img.shields.io/badge/README-zh-red.svg)](./README.md)
[![en](https://img.shields.io/badge/README-en-gre.svg)](./README.en.md)

---
---

## 基本信息
版本: TensorFlow 2.4.1

编码: UTF-8

目前还不支持GPU/XNNPACK加速。

---
---

## 更新
20210401：
1. 重构了项目构建的方式。现在每一个项目都拥有自己的Makefile文件了，Makefile文件放置在对应项目的文件夹内。  
    构建项目的语法仍然是```./build_project.sh <项目名>```

2. 修改了示例项目的Makefile文件。现在每个项目可以在Makefile的```SRC_DIRS```变量中添加多个源文件目录。

---
---

## 使用说明
### 创建并构建项目
1. 给予项目构建脚本```./build_project.sh```执行权限：  
   ```console
   chmod +x ./build_project.sh
   ```

2. 在```./project```目录创建你的tf-lite项目。可以参考已有的示例项目。  
   **项目文件夹的名称即为你的<u>*项目名*</u>。**

3. 编写项目代码和Makefile文件。

4. 执行构建脚本：  
   ```console
   ./build_project.sh <项目名>

   # 请不要使用source ./build_project.sh或. ./build_project.sh命令，脚本中的exit命令会导致当前ssh窗口退出
   ```

5. 生成好的项目会在```./build```目录中。

---

### 运行示例项目
这是一个运行MobileNetV3目标识别网络模型的示例。 

1. 构建示例项目:
   ```console
   ./build_project.sh label_image_tf1.14
   ```
2. 复制网络模型、标签、数据集和测试图片到项目构建目录：
   ```console
   cp -r ./project/label_image_tf1.14/data ./build/label_image_tf1.14/
   ```
3. 切换到测试图片所在目录：
   ```console
   cd ./build/label_image_tf1.14/data
   ```
4. 运行MobileNetV3网络模型，对```dogs.bmp```进行目标识别：
   ```console
   ../bin/label_image_tf1.14 -m ./mobnet_v3_coco_official.tflite -l ./labelmap.txt -i dogs.bmp -o 1 -t 1
   ```

注意：输入给模型的图片文件必须是bmp格式，且只能是文件名，不能有路径。输出的图片在当前目录，如```out_dogs.bmp```。

---

### 如需交叉编译
交叉编译工具链（使用压缩包中的6.5.0版本）：  
https://github.com/rvagg/rpi-newer-crosstools/archive/eb68350c5c8ec1663b7fe52c742ac4271e3217c5.tar.gz 

请事先在Makefile中指定好CC、CXX、AR变量。

```
/your/path/to/rpi-newer-crosstools/x64-gcc-6.5.0/arm-rpi-linux-gnueabihf/bin/arm-rpi-linux-gnueabihf-gcc 
 
/your/path/to/rpi-newer-crosstools/x64-gcc-6.5.0/arm-rpi-linux-gnueabihf/bin/arm-rpi-linux-gnueabihf-g++ 
 
/your/path/to/rpi-newer-crosstools/x64-gcc-6.5.0/arm-rpi-linux-gnueabihf/bin/arm-rpi-linux-gnueabihf-ar
```

交叉编译tensorflowlite库教程(本项目已编译好)：  
https://blog.csdn.net/weixin_41973774/article/details/114807080
