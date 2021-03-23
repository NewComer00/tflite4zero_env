# 最终可执行文件名，可以外部定义，必须是projs目录中存在的目录名。
# 默认为label_image_tf1.14。
TARGET_EXEC ?= label_image_tf1.14

# 包含所有项目的目录
PROJS_DIR := ./project

# tensorflow源码的最外层目录，以及其依赖的第三方库目录
TF_DIR := ./tensorflow_src
TF_DEPENDENCY_DIR := $(TF_DIR)/tensorflow/lite/tools/make/downloads

# 生成好的项目所在目录，项目obj和可执行文件所在目录
BUILD_DIR := ./build/$(TARGET_EXEC)
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin

# 项目源文件、头文件和库文件所在目录
SRC_DIRS := $(PROJS_DIR)/$(TARGET_EXEC)/src
INC_DIRS := $(PROJS_DIR)/$(TARGET_EXEC)/inc \
	$(TF_DIR) \
	$(TF_DEPENDENCY_DIR)/flatbuffers/include \
	$(TF_DEPENDENCY_DIR)/absl \
	$(TF_DEPENDENCY_DIR)/googletest/googlemock/include \
	$(TF_DEPENDENCY_DIR)/googletest/googletest/include 
LIB_DIRS := ./lib

# 符合要求的源文件名称
SRCS := $(shell find $(SRC_DIRS) \
	-name *.cpp -or \
	-name *.cc -or \
	-name *.c \
	 | xargs basename -a \
	)

# 需要的链接库
LIBS := -ltensorflow-lite \
	-latomic \
	-lpthread \
	-lrt \
	-ldl

# obj文件的文件名及存放路径
OBJS := $(SRCS:%=$(OBJ_DIR)/%.o)


# 为编译器指定要包含的头文件目录
CPPFLAGS := $(addprefix -I,$(INC_DIRS))

# 为C++编译器指定预编译参数
CXXFLAGS += \
	-march=armv6 \
	-mfpu=vfp \
	-funsafe-math-optimizations \
	-ftree-vectorize \
	-fPIC \
	-marm

# 为C编译器指定预编译参数
CFLAGS += \
	-march=armv6 \
	-mfpu=vfp \
	-funsafe-math-optimizations \
	-ftree-vectorize \
	-fPIC \
	-marm

# 指定链接库和参数
LDFLAGS := \
	$(addprefix -L,$(LIB_DIRS)) \
	$(LIBS) \
	-Wl,--no-export-dynamic \
	-Wl,--exclude-libs,ALL \
	-Wl,--gc-sections \
	-Wl,--as-needed


# =========================================================


# 链接所有obj文件
$(BIN_DIR)/$(TARGET_EXEC): $(OBJS)
	mkdir -p $(dir $@)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

# 编译所有c源文件
$(OBJ_DIR)/%.c.o: $(SRC_DIRS)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# 编译所有cpp源文件
$(OBJ_DIR)/%.cpp.o: $(SRC_DIRS)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# 编译所有cc源文件
$(OBJ_DIR)/%.cc.o: $(SRC_DIRS)/%.cc
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@


# =========================================================


# 清空项目生成目录
.PHONY: clean
clean:
	rm -r $(BUILD_DIR)