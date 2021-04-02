# **tflite4zero_env**<br>a C++ Environment for Building Tensorflow Lite Projects on Raspberry Pi Zero (armv6)

[![zh](https://img.shields.io/badge/README-zh-red.svg)](./README.md)
[![en](https://img.shields.io/badge/README-en-gre.svg)](./README.en.md)

---
---

## General information
Version: TensorFlow 2.4.1

Encode: UTF-8

GPU/XNNPACK acceleration not supported, yet.

---
---

## Update
20210401：
1. Refactored the way the project was built. Now every project has its own Makefile, which is placed in the folder of the corresponding project.  
    The syntax for building a project is still ```./build_project.sh <project name>```

2. Modified the Makefile of the sample project. Now each project can add multiple source file directories in the ```SRC_DIRS``` variable of Makefile.

---
---

## Instructions for use
### Create and build the project
1. Give the project build script ```./build_project.sh``` execute permission:
    ```console
    chmod +x ./build_project.sh
    ```

2. Create your tf-lite project in the ```./project``` directory. You can refer to existing sample projects.  
    **The name of the project folder is your <u>*project name*</u>.**

3. Write the project code and Makefile.

4. Execute the build script:
    ```console
    ./build_project.sh <project name>

    # Please do not use the source ./build_project.sh or. ./build_project.sh command, the exit command in the script will cause the current ssh window to exit
    ```

5. The generated project will be in the ```./build``` directory.

---

### Run the sample project
This is an example of running the MobileNetV3 target recognition network model.

1. Build the sample project:
    ```console
    ./build_project.sh label_image_tf1.14
    ```
2. Copy the network model, label, data set and test picture to the project build directory:
    ```console
    cp -r ./project/label_image_tf1.14/data ./build/label_image_tf1.14/
    ```
3. Switch to the directory where the test picture is located:
    ```console
    cd ./build/label_image_tf1.14/data
    ```
4. Run the MobileNetV3 network model and perform target recognition on ```dogs.bmp```:
    ```console
    ../bin/label_image_tf1.14 -m ./mobnet_v3_coco_official.tflite -l ./labelmap.txt -i dogs.bmp -o 1 -t 1
    ```

Note: The image file input to the model must be in bmp format, and can only be a file name without path. The output picture is in the current directory, such as ```out_dogs.bmp```.

---

## If cross compile is needed
Cross compile toolchain (use toolchain 6.5.0 in tar.gz):

https://github.com/rvagg/rpi-newer-crosstools/archive/eb68350c5c8ec1663b7fe52c742ac4271e3217c5.tar.gz 

CC/CXX/AR variables need to be set in Makefile.

```
/your/path/to/rpi-newer-crosstools/x64-gcc-6.5.0/arm-rpi-linux-gnueabihf/bin/arm-rpi-linux-gnueabihf-gcc 
 
/your/path/to/rpi-newer-crosstools/x64-gcc-6.5.0/arm-rpi-linux-gnueabihf/bin/arm-rpi-linux-gnueabihf-g++ 
 
/your/path/to/rpi-newer-crosstools/x64-gcc-6.5.0/arm-rpi-linux-gnueabihf/bin/arm-rpi-linux-gnueabihf-ar
```

More info about cross-compiling tensorflowlite library on rpi zero (already compiled in our environment):

https://blog.csdn.net/weixin_41973774/article/details/114807080
(in Chinese)