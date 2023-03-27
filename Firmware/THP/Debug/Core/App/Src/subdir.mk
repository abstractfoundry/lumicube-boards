################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (10.3-2021.10)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/App/Src/app.c 

OBJS += \
./Core/App/Src/app.o 

C_DEPS += \
./Core/App/Src/app.d 


# Each subdirectory must supply rules for building sources it contributes
Core/App/Src/%.o Core/App/Src/%.su: ../Core/App/Src/%.c Core/App/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DUSE_HAL_DRIVER -DSTM32F042x6 -DDEBUG -c -I../Core/CommonLibraries/Can/Inc -I../Core/App/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Core/BNO055 -I../Drivers/CMSIS/Include -I../Core/Inc -I../Core/CommonLibraries/libcanard/inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Core/CommonLibraries/AFlibrary/Inc -I../../Libraries/BNO055 -Og -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Core-2f-App-2f-Src

clean-Core-2f-App-2f-Src:
	-$(RM) ./Core/App/Src/app.d ./Core/App/Src/app.o ./Core/App/Src/app.su

.PHONY: clean-Core-2f-App-2f-Src

