# 在树莓派zero上开发tf-lite的C++环境<br>a C++ Environment for Building Tf-lite Projects on Raspberry Pi Zero (armv6)

Version: TensorFlow 2.4.1

Encode: UTF-8

GPU/XNNPACK acceleration not supported.

***

## 创建并生成项目

1. 在./project目录创建你的tf-lite项目。可以参考已有的示例项目。

2. 编写代码。

3. 执行编译脚本 ./build_project.sh <你的项目文件夹的名字，不需要前面的路径>

4. 生成好的项目会在./build目录中。

***
***

## Create & Build Project

1. Create your tf-lite project in ./project folder, like other projects already in it.

2. Coding~~~

3. ./build_project.sh <no_path_only_the_name_of_your_project_folder>

4. Your built project is fresh in ./build folder!

***
***

## 运行示例项目<br>Run Demo Project

1. cp -r ./project/label_image_tf1.14/data ./build/label_image_tf1.14/

2. cd ./build/label_image_tf1.14/data

3. ../bin/label_image_tf1.14 -m ./mobnet_v3_coco_official.tflite -l ./labelmap.txt -i dogs.bmp -o 1 -t 1

注意：输入给模型的图片文件必须是bmp，且只能是文件名，不能有路径。输出的图片在当前目录。

Note: The input iamge needs to be *.bmp without path. The output image is generated in the current directory.

***
***

## 如需交叉编译<br>if Cross Compiling is Needed
交叉编译工具链（使用压缩包中的6.5.0版本）：

Toolchain (use toolchain 6.5.0 in tar.gz):

https://github.com/rvagg/rpi-newer-crosstools/archive/eb68350c5c8ec1663b7fe52c742ac4271e3217c5.tar.gz 

请事先指定好Makefile中的CC、CXX、AR变量。

CC/CXX/AR variables in Makefile need to be set.

'''
/your/path/to/rpi-newer-crosstools/x64-gcc-6.5.0/arm-rpi-linux-gnueabihf/bin/arm-rpi-linux-gnueabihf-gcc 
 
/your/path/to/rpi-newer-crosstools/x64-gcc-6.5.0/arm-rpi-linux-gnueabihf/bin/arm-rpi-linux-gnueabihf-g++ 
 
/your/path/to/rpi-newer-crosstools/x64-gcc-6.5.0/arm-rpi-linux-gnueabihf/bin/arm-rpi-linux-gnueabihf-ar
'''

交叉编译tensorflowlite库(本项目已编译好)：

More info about cross-compiling tensorflowlite library on rpi zero (already compiled in this environment):

https://blog.csdn.net/weixin_41973774/article/details/114807080