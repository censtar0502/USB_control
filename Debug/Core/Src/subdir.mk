################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/app.c \
../Core/Src/cdc_logger.c \
../Core/Src/gkl_link.c \
../Core/Src/keyboard.c \
../Core/Src/main.c \
../Core/Src/pump_mgr.c \
../Core/Src/pump_proto_gkl.c \
../Core/Src/pump_response_parser.c \
../Core/Src/pump_transactions.c \
../Core/Src/settings.c \
../Core/Src/ssd1309.c \
../Core/Src/stm32h7xx_hal_msp.c \
../Core/Src/stm32h7xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32h7xx.c \
../Core/Src/ui.c 

OBJS += \
./Core/Src/app.o \
./Core/Src/cdc_logger.o \
./Core/Src/gkl_link.o \
./Core/Src/keyboard.o \
./Core/Src/main.o \
./Core/Src/pump_mgr.o \
./Core/Src/pump_proto_gkl.o \
./Core/Src/pump_response_parser.o \
./Core/Src/pump_transactions.o \
./Core/Src/settings.o \
./Core/Src/ssd1309.o \
./Core/Src/stm32h7xx_hal_msp.o \
./Core/Src/stm32h7xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32h7xx.o \
./Core/Src/ui.o 

C_DEPS += \
./Core/Src/app.d \
./Core/Src/cdc_logger.d \
./Core/Src/gkl_link.d \
./Core/Src/keyboard.d \
./Core/Src/main.d \
./Core/Src/pump_mgr.d \
./Core/Src/pump_proto_gkl.d \
./Core/Src/pump_response_parser.d \
./Core/Src/pump_transactions.d \
./Core/Src/settings.d \
./Core/Src/ssd1309.d \
./Core/Src/stm32h7xx_hal_msp.d \
./Core/Src/stm32h7xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32h7xx.d \
./Core/Src/ui.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_PWR_LDO_SUPPLY -DUSE_HAL_DRIVER -DSTM32H750xx -c -I../USB_DEVICE/App -I../USB_DEVICE/Target -I../Core/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc -I../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/app.cyclo ./Core/Src/app.d ./Core/Src/app.o ./Core/Src/app.su ./Core/Src/cdc_logger.cyclo ./Core/Src/cdc_logger.d ./Core/Src/cdc_logger.o ./Core/Src/cdc_logger.su ./Core/Src/gkl_link.cyclo ./Core/Src/gkl_link.d ./Core/Src/gkl_link.o ./Core/Src/gkl_link.su ./Core/Src/keyboard.cyclo ./Core/Src/keyboard.d ./Core/Src/keyboard.o ./Core/Src/keyboard.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/pump_mgr.cyclo ./Core/Src/pump_mgr.d ./Core/Src/pump_mgr.o ./Core/Src/pump_mgr.su ./Core/Src/pump_proto_gkl.cyclo ./Core/Src/pump_proto_gkl.d ./Core/Src/pump_proto_gkl.o ./Core/Src/pump_proto_gkl.su ./Core/Src/pump_response_parser.cyclo ./Core/Src/pump_response_parser.d ./Core/Src/pump_response_parser.o ./Core/Src/pump_response_parser.su ./Core/Src/pump_transactions.cyclo ./Core/Src/pump_transactions.d ./Core/Src/pump_transactions.o ./Core/Src/pump_transactions.su ./Core/Src/settings.cyclo ./Core/Src/settings.d ./Core/Src/settings.o ./Core/Src/settings.su ./Core/Src/ssd1309.cyclo ./Core/Src/ssd1309.d ./Core/Src/ssd1309.o ./Core/Src/ssd1309.su ./Core/Src/stm32h7xx_hal_msp.cyclo ./Core/Src/stm32h7xx_hal_msp.d ./Core/Src/stm32h7xx_hal_msp.o ./Core/Src/stm32h7xx_hal_msp.su ./Core/Src/stm32h7xx_it.cyclo ./Core/Src/stm32h7xx_it.d ./Core/Src/stm32h7xx_it.o ./Core/Src/stm32h7xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32h7xx.cyclo ./Core/Src/system_stm32h7xx.d ./Core/Src/system_stm32h7xx.o ./Core/Src/system_stm32h7xx.su ./Core/Src/ui.cyclo ./Core/Src/ui.d ./Core/Src/ui.o ./Core/Src/ui.su

.PHONY: clean-Core-2f-Src

