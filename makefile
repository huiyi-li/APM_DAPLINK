# Makefile模板 for ARM GCC交叉编译

# 工具链定义
CROSS_COMPILE = arm-none-eabi-
CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)as
CP = $(CROSS_COMPILE)objcopy
SZ = $(CROSS_COMPILE)size

# 目标定义
TARGET = firmware

# 编译选项
CPU = -mcpu=cortex-m4
FPU = -mfpu=fpv4-sp-d16
FLOAT_ABI = -mfloat-abi=hard  # 修正变量名（使用下划线）
MCU = $(CPU) -mthumb $(FPU) $(FLOAT_ABI)

# 包含目录
CINC = -IUser \
           -Ibsp/inc \
           -ILibraries/APM32F4xx_StdPeriphDriver/inc \
		   -ILibraries/Device/Geehy/APM32F4xx/Include \
		   -ILibraries/CMSIS/Include \

AINC = 

# 编译选项
CFLAGS = $(MCU) $(CINC) -Wall -fdata-sections -ffunction-sections -O0 -g3 -DAPM32F40X

ASFLAGS = $(MCU) $(AINC)  # 使用统一的MCU定义，确保汇编器参数正确

# 链接脚本
LDSCRIPT = Libraries/Device/Geehy/APM32F4xx/Source/gcc/APM32F4xxxG_FLASH.ld

# 库文件
LIBS = -lc -lm -lnosys
LIBDIR = 

# 链接选项
LDFLAGS = $(MCU) -T$(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,--gc-sections

# 源文件
C_SOURCES = \
User/main.c \
User/system_apm32f4xx.c \
User/apm32f4xx_int.c \
bsp/src/bsp_delay.c \
bsp/src/bsp_sdcard.c \
bsp/src/bsp_w25qxx.c \
$(wildcard Libraries/APM32F4xx_StdPeriphDriver/src/*.c)

# 汇编源文件
ASM_SOURCES = \
Libraries/Device/Geehy/APM32F4xx/Source/gcc/startup_apm32f40x.S

# 目标文件
OBJECTS = $(addprefix build/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

OBJECTS += $(addprefix build/,$(notdir $(ASM_SOURCES:.S=.o)))
vpath %.S $(sort $(dir $(ASM_SOURCES)))  

# 默认目标
all: $(TARGET).elf

# 生成规则
$(TARGET).elf: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

%.hex: %.elf
	$(CP) -O ihex $< $@

%.bin: %.elf
	$(CP) -O binary -S $< $@

build/%.o: %.c Makefile | build
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=build/$(notdir $(<:.c=.lst)) $< -o $@

build/%.o: %.S Makefile | build
	$(CC) -c $(ASFLAGS) -Wa,-ad,-alms=build/$(notdir $(<:.S=.lst)) $< -o $@

build:
	mkdir -p $@

# 清理
clean:
	rm -rf build
	rm -f $(TARGET).elf $(TARGET).hex $(TARGET).bin

# 重新构建
rebuild: clean all