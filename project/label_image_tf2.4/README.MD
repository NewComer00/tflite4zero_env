代码改编自tensorflow2.4.1中的label_image demo。

修改了头文件的包含路径，去掉了代码中调用的gpu delegate功能。

对于mobile net模型，似乎该程序的输出结果不太对？

***
***

Code adapted from the 'label_image' demo in tensorflow 2.4.1

Changed the include path of some header files

Removed the calls of several 'gpu delegate' functions

The classification result given by this program with Mobile Net model seems not right...?