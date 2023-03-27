################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (10.3-2021.10)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/CommonLibraries/AFlibrary/Src/CException.c \
../Core/CommonLibraries/AFlibrary/Src/CExceptionConfig.c \
../Core/CommonLibraries/AFlibrary/Src/afException.c \
../Core/CommonLibraries/AFlibrary/Src/afFieldProtocol.c \
../Core/CommonLibraries/AFlibrary/Src/apds9960I2C.c \
../Core/CommonLibraries/AFlibrary/Src/bme280.c \
../Core/CommonLibraries/AFlibrary/Src/bno055.c \
../Core/CommonLibraries/AFlibrary/Src/buttons.c \
../Core/CommonLibraries/AFlibrary/Src/can.c \
../Core/CommonLibraries/AFlibrary/Src/debugComponent.c \
../Core/CommonLibraries/AFlibrary/Src/fix_fft.c \
../Core/CommonLibraries/AFlibrary/Src/fonts.c \
../Core/CommonLibraries/AFlibrary/Src/imu.c \
../Core/CommonLibraries/AFlibrary/Src/ledStrip.c \
../Core/CommonLibraries/AFlibrary/Src/light.c \
../Core/CommonLibraries/AFlibrary/Src/microphone.c \
../Core/CommonLibraries/AFlibrary/Src/screen.c \
../Core/CommonLibraries/AFlibrary/Src/serialLink.c \
../Core/CommonLibraries/AFlibrary/Src/serialProtocol.c \
../Core/CommonLibraries/AFlibrary/Src/speaker.c \
../Core/CommonLibraries/AFlibrary/Src/tft.c \
../Core/CommonLibraries/AFlibrary/Src/thp.c \
../Core/CommonLibraries/AFlibrary/Src/useful.c 

OBJS += \
./Core/CommonLibraries/AFlibrary/Src/CException.o \
./Core/CommonLibraries/AFlibrary/Src/CExceptionConfig.o \
./Core/CommonLibraries/AFlibrary/Src/afException.o \
./Core/CommonLibraries/AFlibrary/Src/afFieldProtocol.o \
./Core/CommonLibraries/AFlibrary/Src/apds9960I2C.o \
./Core/CommonLibraries/AFlibrary/Src/bme280.o \
./Core/CommonLibraries/AFlibrary/Src/bno055.o \
./Core/CommonLibraries/AFlibrary/Src/buttons.o \
./Core/CommonLibraries/AFlibrary/Src/can.o \
./Core/CommonLibraries/AFlibrary/Src/debugComponent.o \
./Core/CommonLibraries/AFlibrary/Src/fix_fft.o \
./Core/CommonLibraries/AFlibrary/Src/fonts.o \
./Core/CommonLibraries/AFlibrary/Src/imu.o \
./Core/CommonLibraries/AFlibrary/Src/ledStrip.o \
./Core/CommonLibraries/AFlibrary/Src/light.o \
./Core/CommonLibraries/AFlibrary/Src/microphone.o \
./Core/CommonLibraries/AFlibrary/Src/screen.o \
./Core/CommonLibraries/AFlibrary/Src/serialLink.o \
./Core/CommonLibraries/AFlibrary/Src/serialProtocol.o \
./Core/CommonLibraries/AFlibrary/Src/speaker.o \
./Core/CommonLibraries/AFlibrary/Src/tft.o \
./Core/CommonLibraries/AFlibrary/Src/thp.o \
./Core/CommonLibraries/AFlibrary/Src/useful.o 

C_DEPS += \
./Core/CommonLibraries/AFlibrary/Src/CException.d \
./Core/CommonLibraries/AFlibrary/Src/CExceptionConfig.d \
./Core/CommonLibraries/AFlibrary/Src/afException.d \
./Core/CommonLibraries/AFlibrary/Src/afFieldProtocol.d \
./Core/CommonLibraries/AFlibrary/Src/apds9960I2C.d \
./Core/CommonLibraries/AFlibrary/Src/bme280.d \
./Core/CommonLibraries/AFlibrary/Src/bno055.d \
./Core/CommonLibraries/AFlibrary/Src/buttons.d \
./Core/CommonLibraries/AFlibrary/Src/can.d \
./Core/CommonLibraries/AFlibrary/Src/debugComponent.d \
./Core/CommonLibraries/AFlibrary/Src/fix_fft.d \
./Core/CommonLibraries/AFlibrary/Src/fonts.d \
./Core/CommonLibraries/AFlibrary/Src/imu.d \
./Core/CommonLibraries/AFlibrary/Src/ledStrip.d \
./Core/CommonLibraries/AFlibrary/Src/light.d \
./Core/CommonLibraries/AFlibrary/Src/microphone.d \
./Core/CommonLibraries/AFlibrary/Src/screen.d \
./Core/CommonLibraries/AFlibrary/Src/serialLink.d \
./Core/CommonLibraries/AFlibrary/Src/serialProtocol.d \
./Core/CommonLibraries/AFlibrary/Src/speaker.d \
./Core/CommonLibraries/AFlibrary/Src/tft.d \
./Core/CommonLibraries/AFlibrary/Src/thp.d \
./Core/CommonLibraries/AFlibrary/Src/useful.d 


# Each subdirectory must supply rules for building sources it contributes
Core/CommonLibraries/AFlibrary/Src/%.o Core/CommonLibraries/AFlibrary/Src/%.su: ../Core/CommonLibraries/AFlibrary/Src/%.c Core/CommonLibraries/AFlibrary/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DUSE_HAL_DRIVER -DSTM32F042x6 -DDEBUG -c -I../Core/CommonLibraries/Can/Inc -I../Core/App/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Core/BNO055 -I../Drivers/CMSIS/Include -I../Core/Inc -I../Core/CommonLibraries/libcanard/inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Core/CommonLibraries/AFlibrary/Inc -I../../Libraries/BNO055 -Og -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Core-2f-CommonLibraries-2f-AFlibrary-2f-Src

clean-Core-2f-CommonLibraries-2f-AFlibrary-2f-Src:
	-$(RM) ./Core/CommonLibraries/AFlibrary/Src/CException.d ./Core/CommonLibraries/AFlibrary/Src/CException.o ./Core/CommonLibraries/AFlibrary/Src/CException.su ./Core/CommonLibraries/AFlibrary/Src/CExceptionConfig.d ./Core/CommonLibraries/AFlibrary/Src/CExceptionConfig.o ./Core/CommonLibraries/AFlibrary/Src/CExceptionConfig.su ./Core/CommonLibraries/AFlibrary/Src/afException.d ./Core/CommonLibraries/AFlibrary/Src/afException.o ./Core/CommonLibraries/AFlibrary/Src/afException.su ./Core/CommonLibraries/AFlibrary/Src/afFieldProtocol.d ./Core/CommonLibraries/AFlibrary/Src/afFieldProtocol.o ./Core/CommonLibraries/AFlibrary/Src/afFieldProtocol.su ./Core/CommonLibraries/AFlibrary/Src/apds9960I2C.d ./Core/CommonLibraries/AFlibrary/Src/apds9960I2C.o ./Core/CommonLibraries/AFlibrary/Src/apds9960I2C.su ./Core/CommonLibraries/AFlibrary/Src/bme280.d ./Core/CommonLibraries/AFlibrary/Src/bme280.o ./Core/CommonLibraries/AFlibrary/Src/bme280.su ./Core/CommonLibraries/AFlibrary/Src/bno055.d ./Core/CommonLibraries/AFlibrary/Src/bno055.o ./Core/CommonLibraries/AFlibrary/Src/bno055.su ./Core/CommonLibraries/AFlibrary/Src/buttons.d ./Core/CommonLibraries/AFlibrary/Src/buttons.o ./Core/CommonLibraries/AFlibrary/Src/buttons.su ./Core/CommonLibraries/AFlibrary/Src/can.d ./Core/CommonLibraries/AFlibrary/Src/can.o ./Core/CommonLibraries/AFlibrary/Src/can.su ./Core/CommonLibraries/AFlibrary/Src/debugComponent.d ./Core/CommonLibraries/AFlibrary/Src/debugComponent.o ./Core/CommonLibraries/AFlibrary/Src/debugComponent.su ./Core/CommonLibraries/AFlibrary/Src/fix_fft.d ./Core/CommonLibraries/AFlibrary/Src/fix_fft.o ./Core/CommonLibraries/AFlibrary/Src/fix_fft.su ./Core/CommonLibraries/AFlibrary/Src/fonts.d ./Core/CommonLibraries/AFlibrary/Src/fonts.o ./Core/CommonLibraries/AFlibrary/Src/fonts.su ./Core/CommonLibraries/AFlibrary/Src/imu.d ./Core/CommonLibraries/AFlibrary/Src/imu.o ./Core/CommonLibraries/AFlibrary/Src/imu.su ./Core/CommonLibraries/AFlibrary/Src/ledStrip.d ./Core/CommonLibraries/AFlibrary/Src/ledStrip.o ./Core/CommonLibraries/AFlibrary/Src/ledStrip.su ./Core/CommonLibraries/AFlibrary/Src/light.d ./Core/CommonLibraries/AFlibrary/Src/light.o ./Core/CommonLibraries/AFlibrary/Src/light.su ./Core/CommonLibraries/AFlibrary/Src/microphone.d ./Core/CommonLibraries/AFlibrary/Src/microphone.o ./Core/CommonLibraries/AFlibrary/Src/microphone.su ./Core/CommonLibraries/AFlibrary/Src/screen.d ./Core/CommonLibraries/AFlibrary/Src/screen.o ./Core/CommonLibraries/AFlibrary/Src/screen.su ./Core/CommonLibraries/AFlibrary/Src/serialLink.d ./Core/CommonLibraries/AFlibrary/Src/serialLink.o ./Core/CommonLibraries/AFlibrary/Src/serialLink.su ./Core/CommonLibraries/AFlibrary/Src/serialProtocol.d ./Core/CommonLibraries/AFlibrary/Src/serialProtocol.o ./Core/CommonLibraries/AFlibrary/Src/serialProtocol.su ./Core/CommonLibraries/AFlibrary/Src/speaker.d ./Core/CommonLibraries/AFlibrary/Src/speaker.o ./Core/CommonLibraries/AFlibrary/Src/speaker.su ./Core/CommonLibraries/AFlibrary/Src/tft.d ./Core/CommonLibraries/AFlibrary/Src/tft.o ./Core/CommonLibraries/AFlibrary/Src/tft.su ./Core/CommonLibraries/AFlibrary/Src/thp.d ./Core/CommonLibraries/AFlibrary/Src/thp.o ./Core/CommonLibraries/AFlibrary/Src/thp.su ./Core/CommonLibraries/AFlibrary/Src/useful.d ./Core/CommonLibraries/AFlibrary/Src/useful.o ./Core/CommonLibraries/AFlibrary/Src/useful.su

.PHONY: clean-Core-2f-CommonLibraries-2f-AFlibrary-2f-Src

