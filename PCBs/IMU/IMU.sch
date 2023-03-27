EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title ""
Date "2020-06-12"
Rev ""
Comp "Abstract Foundry Limited"
Comment1 ""
Comment2 ""
Comment3 "Author: Sean Bremner"
Comment4 "Copyright (c) 2020 Abstract Foundry Limited"
$EndDescr
$Comp
L AbstractFoundry:C C2
U 1 1 5E002925
P 7700 1500
F 0 "C2" H 7815 1546 50  0000 L CNN
F 1 "4.7uF" H 7815 1455 50  0000 L CNN
F 2 "AbstractFoundry:C_0402_1005Metric" H 7738 1350 50  0001 C CNN
F 3 "~" H 7700 1500 50  0001 C CNN
	1    7700 1500
	1    0    0    -1  
$EndComp
Text GLabel 6200 1150 0    50   Input ~ 0
5V
Text GLabel 6200 1650 0    50   Input ~ 0
GND
Text GLabel 7800 1300 2    50   Input ~ 0
VDD
Wire Wire Line
	7700 1300 7800 1300
Connection ~ 7700 1300
Wire Wire Line
	7700 1350 7700 1300
Wire Wire Line
	7450 1300 7700 1300
Wire Wire Line
	6700 1450 6700 1650
Wire Wire Line
	6700 1450 6750 1450
$Comp
L AbstractFoundry:C C1
U 1 1 60A104F9
P 6350 1350
F 0 "C1" H 6465 1396 50  0000 L CNN
F 1 "4.7uF" H 6465 1305 50  0000 L CNN
F 2 "AbstractFoundry:C_0402_1005Metric" H 6388 1200 50  0001 C CNN
F 3 "~" H 6350 1350 50  0001 C CNN
	1    6350 1350
	1    0    0    -1  
$EndComp
Wire Wire Line
	6350 1200 6350 1150
Connection ~ 6700 1650
Wire Wire Line
	6200 1650 6350 1650
Wire Wire Line
	6350 1500 6350 1650
Connection ~ 6350 1650
Wire Wire Line
	6350 1650 6700 1650
Wire Wire Line
	6200 1150 6350 1150
Connection ~ 6350 1150
$Comp
L AbstractFoundry:MaleConn_1x6 J2
U 1 1 60A105F0
P 3350 1400
F 0 "J2" H 3350 1900 50  0000 C CNN
F 1 "MaleConn_1x6" H 3408 1776 50  0000 C CNN
F 2 "AbstractFoundry:M22-5330605R" H 3350 1450 50  0001 C CNN
F 3 "" H 3350 1450 50  0001 C CNN
F 4 "Changjiang Connectors" H 3350 1450 50  0001 C CNN "Manufacturer"
F 5 "A2005WR-S-6P" H 3350 1450 50  0001 C CNN "Manufacturer_Part_Number"
F 6 "https://www.mouser.co.uk/ProductDetail/Harwin/M22-5330605?qs=sGAEpiMZZMs%252BGHln7q6pmzlZUuX%2F53qjqM6RSwiaBWM%3D" H 3350 1450 50  0001 C CNN "Link"
F 7 "0.25" H 3350 1400 50  0001 C CNN "USD_price_for_10"
F 8 "0.25" H 3350 1400 50  0001 C CNN "USD_price_for_100"
F 9 "M22-5330605" H 3350 1400 50  0001 C CNN "Alternative part"
	1    3350 1400
	1    0    0    -1  
$EndComp
Text Label 3650 1150 0    50   ~ 0
VPOWER
Text Label 3650 1250 0    50   ~ 0
5V
Text Label 3650 1350 0    50   ~ 0
GND
Wire Wire Line
	3550 1150 4800 1150
Wire Wire Line
	3550 1250 4800 1250
Wire Wire Line
	3550 1350 4800 1350
Wire Wire Line
	3550 1450 4800 1450
Text Label 3650 1450 0    50   ~ 0
CAN
$Comp
L AbstractFoundry:STM32F042F6P6 U2
U 1 1 60A105F2
P 3350 3150
F 0 "U2" H 3350 2261 50  0000 C CNN
F 1 "STM32F042F6P6" H 3350 2170 50  0000 C CNN
F 2 "AbstractFoundry:STM32F042F6P6" H 2850 2450 50  0001 R CNN
F 3 "http://www.st.com/st-web-ui/static/active/en/resource/technical/document/datasheet/DM00105814.pdf" H 3350 3150 50  0001 C CNN
	1    3350 3150
	1    0    0    -1  
$EndComp
Text GLabel 3700 1800 2    50   Input ~ 0
SWDIO
Text GLabel 3700 1900 2    50   Input ~ 0
SWCLK
Text GLabel 4000 3650 2    50   Input ~ 0
SWDIO
Text GLabel 4000 3750 2    50   Input ~ 0
SWCLK
Wire Wire Line
	3950 2650 4000 2650
Wire Wire Line
	4000 2750 3950 2750
Wire Wire Line
	3950 2850 4000 2850
Wire Wire Line
	4000 2950 3950 2950
Wire Wire Line
	4000 3150 3950 3150
Wire Wire Line
	3950 3250 4000 3250
Wire Wire Line
	4000 3350 3950 3350
Wire Wire Line
	4000 3650 3950 3650
Wire Wire Line
	3950 3750 4000 3750
Text GLabel 3050 4000 0    50   Input ~ 0
GND
Wire Wire Line
	3050 4000 3150 4000
Wire Wire Line
	3150 4000 3150 3950
Text GLabel 2650 3350 0    50   Input ~ 0
I2C_SDA
Text GLabel 2650 3450 0    50   Input ~ 0
I2C_SCL
Wire Wire Line
	2650 3350 2750 3350
Wire Wire Line
	2650 3450 2750 3450
Wire Wire Line
	2750 3650 2650 3650
$Comp
L AbstractFoundry:R R1
U 1 1 60A104F6
P 2400 2450
F 0 "R1" H 2470 2496 50  0000 L CNN
F 1 "10K" H 2470 2405 50  0000 L CNN
F 2 "AbstractFoundry:R_0402_1005Metric" V 2330 2450 50  0001 C CNN
F 3 "~" H 2400 2450 50  0001 C CNN
	1    2400 2450
	1    0    0    1   
$EndComp
Wire Wire Line
	2400 2600 2400 2650
Wire Wire Line
	2400 2650 2750 2650
Wire Wire Line
	3250 2250 3250 2450
Wire Wire Line
	3150 2450 3150 2250
Connection ~ 3150 2250
Wire Wire Line
	3150 2250 3250 2250
Wire Wire Line
	2400 2300 2400 2250
Connection ~ 2400 2250
Wire Wire Line
	2400 2250 3150 2250
$Comp
L AbstractFoundry:FemaleConn_1x6 J1
U 1 1 60A105F1
P 5000 1350
F 0 "J1" H 5000 1800 50  0000 L CNN
F 1 "FemaleConn_1x6" H 4750 1700 50  0000 L CNN
F 2 "AbstractFoundry:M22-6540642R" H 5000 1350 50  0001 C CNN
F 3 "" H 5000 1350 50  0001 C CNN
F 4 "Changjiang Connectors" H 5000 1350 50  0001 C CNN "Manufacturer"
F 5 "A2005HWR-S-6P-Y" H 5000 1350 50  0001 C CNN "Manufacturer_Part_Number"
F 6 "https://www.mouser.co.uk/ProductDetail/Harwin/M22-6540642R?qs=%2Fha2pyFadugSTfxSUTVZ2Ehr1GZOewZmeQPAP3RhgAADXaKn5dWIiw%3D%3D" H 5000 1350 50  0001 C CNN "Link"
F 7 "0.3" H 5000 1350 50  0001 C CNN "USD_price_for_10"
F 8 "0.3" H 5000 1350 50  0001 C CNN "USD_price_for_100"
F 9 "M22-6540642" H 5000 1350 50  0001 C CNN "Alternative part"
	1    5000 1350
	1    0    0    -1  
$EndComp
$Comp
L AbstractFoundry:M2.5MountingHole H1
U 1 1 5F1EED8D
P 1550 1650
F 0 "H1" H 1650 1696 50  0000 L CNN
F 1 "M2.5MountingHole" H 1650 1605 50  0000 L CNN
F 2 "AbstractFoundry:MountingHole_2.7mm_M2.5_ISO7380" H 1550 1650 50  0001 C CNN
F 3 "~" H 1550 1650 50  0001 C CNN
	1    1550 1650
	1    0    0    -1  
$EndComp
$Comp
L AbstractFoundry:M2.5MountingHole H2
U 1 1 5F1F3D85
P 1550 1850
F 0 "H2" H 1650 1896 50  0000 L CNN
F 1 "M2.5MountingHole" H 1650 1805 50  0000 L CNN
F 2 "AbstractFoundry:MountingHole_2.7mm_M2.5_ISO7380" H 1550 1850 50  0001 C CNN
F 3 "~" H 1550 1850 50  0001 C CNN
	1    1550 1850
	1    0    0    -1  
$EndComp
$Comp
L AbstractFoundry:R R3
U 1 1 5DF346AE
P 5500 3550
F 0 "R3" H 5570 3596 50  0000 L CNN
F 1 "10R" H 5570 3505 50  0000 L CNN
F 2 "AbstractFoundry:R_0402_1005Metric" V 5430 3550 50  0001 C CNN
F 3 "~" H 5500 3550 50  0001 C CNN
	1    5500 3550
	0    -1   1    0   
$EndComp
Wire Wire Line
	3550 1550 3600 1550
Wire Wire Line
	3550 1650 4800 1650
Text Label 3650 1550 0    50   ~ 0
HS1
Text Label 3650 1650 0    50   ~ 0
HS2
Wire Wire Line
	3600 1550 3600 1800
Wire Wire Line
	3600 1800 3700 1800
Connection ~ 3600 1550
Wire Wire Line
	3600 1550 4800 1550
Wire Wire Line
	3550 1650 3550 1900
Wire Wire Line
	3550 1900 3700 1900
Connection ~ 3550 1650
Text GLabel 1750 2250 0    50   Input ~ 0
VDD
Connection ~ 1900 2250
Wire Wire Line
	1750 2250 1900 2250
Wire Wire Line
	1900 2250 2400 2250
Wire Wire Line
	1900 2300 1900 2250
Wire Wire Line
	1900 2750 1900 2600
Wire Wire Line
	1850 2750 1900 2750
Text GLabel 1850 2750 0    50   Input ~ 0
GND
$Comp
L AbstractFoundry:C C3
U 1 1 60A104F7
P 1900 2450
F 0 "C3" H 2015 2496 50  0000 L CNN
F 1 "4.7uF" H 2015 2405 50  0000 L CNN
F 2 "AbstractFoundry:C_0402_1005Metric" H 1938 2300 50  0001 C CNN
F 3 "~" H 1900 2450 50  0001 C CNN
	1    1900 2450
	1    0    0    -1  
$EndComp
Wire Wire Line
	3950 3050 4000 3050
$Comp
L AbstractFoundry:CompanyIcon IGNORE1
U 1 1 5D9C91E9
P 1400 800
F 0 "IGNORE1" H 1528 671 50  0000 L CNN
F 1 "CompanyIcon" H 1528 580 50  0000 L CNN
F 2 "AbstractFoundry:CompanyIcon" H 1400 800 50  0001 C CNN
F 3 "" H 1400 800 50  0001 C CNN
	1    1400 800 
	1    0    0    -1  
$EndComp
$Comp
L AbstractFoundry:R R6
U 1 1 60A104F3
P 1800 5550
F 0 "R6" H 1870 5596 50  0000 L CNN
F 1 "3.3K" H 1870 5505 50  0000 L CNN
F 2 "AbstractFoundry:R_0402_1005Metric" V 1730 5550 50  0001 C CNN
F 3 "~" H 1800 5550 50  0001 C CNN
	1    1800 5550
	1    0    0    -1  
$EndComp
$Comp
L AbstractFoundry:R R5
U 1 1 5D1CDE94
P 1450 5550
F 0 "R5" H 1520 5596 50  0000 L CNN
F 1 "3.3K" H 1520 5505 50  0000 L CNN
F 2 "AbstractFoundry:R_0402_1005Metric" V 1380 5550 50  0001 C CNN
F 3 "~" H 1450 5550 50  0001 C CNN
	1    1450 5550
	1    0    0    -1  
$EndComp
Text GLabel 1350 6100 0    50   Input ~ 0
I2C_SDA
Text GLabel 1350 5800 0    50   Input ~ 0
I2C_SCL
$Comp
L AbstractFoundry:BNO055 U4
U 1 1 5D7FCD24
P 3550 5350
F 0 "U4" H 3550 6131 50  0000 C CNN
F 1 "BNO055" H 3550 6040 50  0000 C CNN
F 2 "AbstractFoundry:IMU-BNO055" H 3750 5550 60  0001 L CNN
F 3 "" H 3750 5650 60  0001 L CNN
F 4 "Bosch" H 3750 6450 60  0001 L CNN "Manufacturer"
F 5 "BNO055" H 3550 5350 50  0001 C CNN "Manufacturer_Part_Number"
F 6 "https://www.mouser.co.uk/ProductDetail/Bosch-Sensortec/BNO055?qs=sGAEpiMZZMtIHXa%252BTo%2Fr2SMZuH5v3473jYOqmUidcJg%3D" H 3550 5350 50  0001 C CNN "Link"
F 7 "7.44" H 3550 5350 50  0001 C CNN "USD_price_for_10"
F 8 "5.56" H 3550 5350 50  0001 C CNN "USD_price_for_100"
	1    3550 5350
	1    0    0    -1  
$EndComp
Wire Wire Line
	1350 6100 1550 6100
Wire Wire Line
	1450 5700 1450 5800
Connection ~ 1450 5800
Wire Wire Line
	1450 5800 1350 5800
Wire Wire Line
	2800 5350 2850 5350
Wire Wire Line
	2850 5150 2800 5150
Wire Wire Line
	2800 5150 2800 5250
Wire Wire Line
	2850 5250 2800 5250
Connection ~ 2800 5250
Wire Wire Line
	2800 5250 2800 5350
Wire Wire Line
	3650 6350 3650 6150
Wire Wire Line
	3350 6150 3350 6350
Connection ~ 3350 6350
Wire Wire Line
	3350 6350 3450 6350
Wire Wire Line
	3450 6150 3450 6350
Connection ~ 3450 6350
Wire Wire Line
	3450 6350 3550 6350
Wire Wire Line
	3550 6150 3550 6350
Text GLabel 4350 5050 2    50   Input ~ 0
Interrupt0
Wire Wire Line
	4250 5050 4350 5050
Wire Wire Line
	3650 4700 3650 4750
Wire Wire Line
	3750 4700 3750 4750
Wire Wire Line
	4250 5400 4450 5400
Wire Wire Line
	4450 5400 4450 5700
Wire Wire Line
	4450 6000 4450 6350
Wire Wire Line
	4450 6350 3650 6350
Text GLabel 2700 6350 0    50   Input ~ 0
GND
Text GLabel 4350 4950 2    50   Input ~ 0
Boot_loader_indicator
Wire Wire Line
	4250 4950 4350 4950
$Comp
L AbstractFoundry:C C5
U 1 1 5E402C8C
P 6100 5500
F 0 "C5" H 6215 5546 50  0000 L CNN
F 1 "0.1uF" H 6215 5455 50  0000 L CNN
F 2 "AbstractFoundry:C_0402_1005Metric" H 6138 5350 50  0001 C CNN
F 3 "~" H 6100 5500 50  0001 C CNN
	1    6100 5500
	1    0    0    -1  
$EndComp
Wire Wire Line
	6100 4700 6100 5350
Wire Wire Line
	6100 6350 6100 5650
Connection ~ 6100 4700
Connection ~ 6100 6350
Text GLabel 6650 4700 2    50   Input ~ 0
VDD
Text GLabel 6650 6350 2    50   Input ~ 0
GND
$Comp
L AbstractFoundry:R R7
U 1 1 5E44E364
P 2250 4950
F 0 "R7" H 2320 4996 50  0000 L CNN
F 1 "10K" H 2320 4905 50  0000 L CNN
F 2 "AbstractFoundry:R_0402_1005Metric" V 2180 4950 50  0001 C CNN
F 3 "~" H 2250 4950 50  0001 C CNN
	1    2250 4950
	1    0    0    -1  
$EndComp
$Comp
L AbstractFoundry:C C8
U 1 1 5E473FDC
P 5100 5900
F 0 "C8" H 5215 5946 50  0000 L CNN
F 1 "22pF" H 5215 5855 50  0000 L CNN
F 2 "AbstractFoundry:C_0402_1005Metric" H 5138 5750 50  0001 C CNN
F 3 "~" H 5100 5900 50  0001 C CNN
	1    5100 5900
	1    0    0    -1  
$EndComp
$Comp
L AbstractFoundry:C C9
U 1 1 5E4845A8
P 5600 5900
F 0 "C9" H 5715 5946 50  0000 L CNN
F 1 "22pF" H 5715 5855 50  0000 L CNN
F 2 "AbstractFoundry:C_0402_1005Metric" H 5638 5750 50  0001 C CNN
F 3 "~" H 5600 5900 50  0001 C CNN
	1    5600 5900
	1    0    0    -1  
$EndComp
Wire Wire Line
	4450 6350 5100 6350
Connection ~ 4450 6350
Wire Wire Line
	3750 4700 6100 4700
Wire Wire Line
	5100 5750 5100 5600
Connection ~ 5100 5600
Wire Wire Line
	5100 6050 5100 6350
Connection ~ 5100 6350
Wire Wire Line
	5100 6350 5600 6350
Wire Wire Line
	5600 5150 5600 5600
Wire Wire Line
	5600 6050 5600 6350
Connection ~ 5600 6350
Wire Wire Line
	5600 6350 6100 6350
Wire Wire Line
	4250 5150 5600 5150
Wire Wire Line
	5500 5600 5600 5600
Connection ~ 5600 5600
Wire Wire Line
	5600 5600 5600 5750
Wire Wire Line
	5100 5250 5100 5600
Wire Wire Line
	4250 5250 5100 5250
$Comp
L AbstractFoundry:Crystal_32.768K Y1
U 1 1 5F293A1E
P 5350 5600
F 0 "Y1" H 5350 5868 50  0000 C CNN
F 1 "Crystal_32.768K" H 5350 5777 50  0000 C CNN
F 2 "AbstractFoundry:Crystal_SMD_3215-2Pin_3.2x1.5mm" H 5350 5600 50  0001 C CNN
F 3 "~" H 5350 5600 50  0001 C CNN
F 4 "https://www.mouser.co.uk/ProductDetail/Murata-Electronics/XRCGB32M000F0L00R0?qs=sGAEpiMZZMve4%2FbfQkoj%252BBf0vke3Oiv%252BB14HWfMktg8%3D" H 5350 5600 50  0001 C CNN "Link"
	1    5350 5600
	1    0    0    -1  
$EndComp
Wire Wire Line
	5100 5600 5200 5600
Text GLabel 4000 3150 2    50   Input ~ 0
Boot_load
Text GLabel 4000 2650 2    50   Input ~ 0
Reset
Text GLabel 4000 2750 2    50   Input ~ 0
Interrupt0
$Comp
L AbstractFoundry:BoardText IGNORE2
U 1 1 5E277C1A
P 1400 1100
F 0 "IGNORE2" H 1528 971 50  0000 L CNN
F 1 "IMU v0.11" H 1528 880 50  0000 L CNN
F 2 "AbstractFoundry:BoardText" H 1450 800 50  0001 C CNN
F 3 "" H 1400 1100 50  0001 C CNN
	1    1400 1100
	1    0    0    -1  
$EndComp
Wire Wire Line
	6700 1650 7700 1650
Wire Wire Line
	6350 1150 6700 1150
Text GLabel 2450 4150 0    50   Input ~ 0
GND
Wire Wire Line
	2600 3750 2750 3750
$Comp
L AbstractFoundry:ME6211C33M5G-N U1
U 1 1 6123D29D
P 7100 950
F 0 "U1" H 7100 1015 50  0000 C CNN
F 1 "ME6211C33M5G-N" H 7100 924 50  0000 C CNN
F 2 "AbstractFoundry:SOT-23-5" H 7100 950 50  0001 C CNN
F 3 "" H 7100 950 50  0001 C CNN
	1    7100 950 
	1    0    0    -1  
$EndComp
Wire Wire Line
	6700 1150 6700 1300
Wire Wire Line
	6700 1300 6750 1300
Connection ~ 6700 1150
Wire Wire Line
	6700 1150 6750 1150
Text GLabel 4000 3350 2    50   Input ~ 0
Boot_loader_indicator
Text GLabel 6250 3550 2    50   Input ~ 0
CAN
$Comp
L AbstractFoundry:R R2
U 1 1 60A105F4
P 6100 3250
F 0 "R2" H 6170 3296 50  0000 L CNN
F 1 "10K" H 6170 3205 50  0000 L CNN
F 2 "AbstractFoundry:R_0402_1005Metric" V 6030 3250 50  0001 C CNN
F 3 "~" H 6100 3250 50  0001 C CNN
	1    6100 3250
	1    0    0    1   
$EndComp
Text GLabel 6000 2900 0    50   Input ~ 0
VDD
Wire Wire Line
	6000 2900 6100 2900
Wire Wire Line
	6100 2900 6100 3100
Wire Wire Line
	6100 3400 6100 3550
Connection ~ 6100 3550
Wire Wire Line
	6100 3550 6250 3550
Wire Wire Line
	5650 3550 5850 3550
Text GLabel 5000 3400 2    50   Input ~ 0
VDD
Text GLabel 5000 3750 2    50   Input ~ 0
GND
Wire Wire Line
	4900 3450 4900 3400
Wire Wire Line
	4900 3400 5000 3400
Wire Wire Line
	4900 3650 4900 3750
Wire Wire Line
	4900 3750 5000 3750
Wire Wire Line
	5150 3550 5350 3550
Wire Wire Line
	3950 3450 4600 3450
Wire Wire Line
	4600 3450 4600 3250
Wire Wire Line
	4600 3250 5850 3250
Wire Wire Line
	5850 3250 5850 3550
Connection ~ 5850 3550
Wire Wire Line
	5850 3550 6100 3550
Wire Wire Line
	3950 3550 4600 3550
$Comp
L AbstractFoundry:R R4
U 1 1 6162A81D
P 2600 3950
F 0 "R4" H 2670 3996 50  0000 L CNN
F 1 "10K" H 2670 3905 50  0000 L CNN
F 2 "AbstractFoundry:R_0402_1005Metric" V 2530 3950 50  0001 C CNN
F 3 "~" H 2600 3950 50  0001 C CNN
	1    2600 3950
	1    0    0    1   
$EndComp
Wire Wire Line
	2450 4150 2600 4150
Wire Wire Line
	2600 4150 2600 4100
Wire Wire Line
	2600 3800 2600 3750
$Comp
L AbstractFoundry:74LVC2G07 U3
U 1 1 6171C858
P 4900 3550
F 0 "U3" H 4875 3817 50  0000 C CNN
F 1 "74LVC2G07" H 4875 3726 50  0000 C CNN
F 2 "AbstractFoundry:SOT-23-6" H 4900 3550 50  0001 C CNN
F 3 "http://www.ti.com/lit/sg/scyt129e/scyt129e.pdf" H 4900 3550 50  0001 C CNN
	1    4900 3550
	1    0    0    -1  
$EndComp
Text GLabel 4000 2850 2    50   Input ~ 0
UART_TX
Text GLabel 4000 2950 2    50   Input ~ 0
UART_RX
Text GLabel 4000 3250 2    50   Input ~ 0
PS1
Text GLabel 1350 6200 0    50   Input ~ 0
UART_RX
Wire Wire Line
	1350 6200 1550 6200
Wire Wire Line
	1550 6200 1550 6100
Connection ~ 1550 6100
Text GLabel 1350 5900 0    50   Input ~ 0
UART_TX
Wire Wire Line
	1350 5900 1450 5900
Wire Wire Line
	1450 5900 1450 5800
Wire Wire Line
	1550 6100 1800 6100
Text GLabel 2700 5650 0    50   Input ~ 0
Reset
Text GLabel 2200 5150 0    50   Input ~ 0
Boot_load
Text GLabel 2550 5250 0    50   Input ~ 0
PS1
Wire Wire Line
	2250 5150 2250 5100
Wire Wire Line
	2250 5150 2550 5150
Wire Wire Line
	2550 5150 2550 4950
Wire Wire Line
	2550 4950 2850 4950
Wire Wire Line
	2250 4800 2250 4700
Wire Wire Line
	2250 4700 3650 4700
Wire Wire Line
	2650 5050 2650 5250
Wire Wire Line
	2650 5050 2850 5050
Wire Wire Line
	1800 5700 1800 6100
Wire Wire Line
	2200 5150 2250 5150
Connection ~ 2250 5150
Wire Wire Line
	2550 5250 2650 5250
Wire Wire Line
	1450 5800 2200 5800
Wire Wire Line
	2200 5800 2200 5450
Wire Wire Line
	2200 5450 2850 5450
Wire Wire Line
	1800 6100 2300 6100
Wire Wire Line
	2300 6100 2300 5550
Wire Wire Line
	2300 5550 2850 5550
Connection ~ 1800 6100
Wire Wire Line
	2700 5650 2850 5650
Text GLabel 2700 5350 0    50   Input ~ 0
GND
Wire Wire Line
	2700 6350 3350 6350
Wire Wire Line
	2700 5350 2800 5350
Connection ~ 2800 5350
Wire Wire Line
	1050 4900 1050 4700
Wire Wire Line
	1050 5300 1050 5200
Wire Wire Line
	950  5300 1050 5300
Wire Wire Line
	1000 4700 1050 4700
Text GLabel 950  5300 0    50   Input ~ 0
GND
Text GLabel 1000 4700 0    50   Input ~ 0
VDD
$Comp
L AbstractFoundry:C C4
U 1 1 60A104F2
P 1050 5050
F 0 "C4" H 1165 5096 50  0000 L CNN
F 1 "4.7uF" H 1165 5005 50  0000 L CNN
F 2 "AbstractFoundry:C_0402_1005Metric" H 1088 4900 50  0001 C CNN
F 3 "~" H 1050 5050 50  0001 C CNN
	1    1050 5050
	1    0    0    -1  
$EndComp
Connection ~ 2250 4700
Connection ~ 1050 4700
Wire Wire Line
	1800 4700 2250 4700
Connection ~ 1800 4700
Wire Wire Line
	1800 5400 1800 4700
Wire Wire Line
	1450 4700 1800 4700
Wire Wire Line
	1050 4700 1450 4700
Connection ~ 1450 4700
Wire Wire Line
	1450 5400 1450 4700
Wire Wire Line
	6100 4700 6650 4700
Wire Wire Line
	6100 6350 6650 6350
$Comp
L AbstractFoundry:C C6
U 1 1 618E9C70
P 4450 5850
F 0 "C6" H 4565 5896 50  0000 L CNN
F 1 "1uF" H 4565 5805 50  0000 L CNN
F 2 "AbstractFoundry:C_0402_1005Metric" H 4488 5700 50  0001 C CNN
F 3 "~" H 4450 5850 50  0001 C CNN
	1    4450 5850
	1    0    0    -1  
$EndComp
$EndSCHEMATC
