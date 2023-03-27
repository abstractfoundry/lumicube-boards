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
P 6900 1300
F 0 "C2" H 7015 1346 50  0000 L CNN
F 1 "4.7uF" H 7015 1255 50  0000 L CNN
F 2 "AbstractFoundry:C_0402_1005Metric" H 6938 1150 50  0001 C CNN
F 3 "~" H 6900 1300 50  0001 C CNN
	1    6900 1300
	1    0    0    -1  
$EndComp
Text GLabel 5400 950  0    50   Input ~ 0
5V
Text GLabel 5400 1450 0    50   Input ~ 0
GND
Text GLabel 7000 1100 2    50   Input ~ 0
VDD
Wire Wire Line
	6900 1100 7000 1100
Connection ~ 6900 1100
Wire Wire Line
	6900 1150 6900 1100
Wire Wire Line
	6650 1100 6900 1100
Wire Wire Line
	5900 1250 5900 1450
Wire Wire Line
	5900 1250 5950 1250
$Comp
L AbstractFoundry:C C1
U 1 1 5E151098
P 5550 1150
F 0 "C1" H 5665 1196 50  0000 L CNN
F 1 "4.7uF" H 5665 1105 50  0000 L CNN
F 2 "AbstractFoundry:C_0402_1005Metric" H 5588 1000 50  0001 C CNN
F 3 "~" H 5550 1150 50  0001 C CNN
	1    5550 1150
	1    0    0    -1  
$EndComp
Wire Wire Line
	5550 1000 5550 950 
Connection ~ 5900 1450
Wire Wire Line
	5400 1450 5550 1450
Wire Wire Line
	5550 1300 5550 1450
Connection ~ 5550 1450
Wire Wire Line
	5550 1450 5900 1450
Wire Wire Line
	5400 950  5550 950 
Connection ~ 5550 950 
$Comp
L AbstractFoundry:MaleConn_1x6 J2
U 1 1 5DD58294
P 2550 1200
F 0 "J2" H 2550 1700 50  0000 C CNN
F 1 "MaleConn_1x6" H 2608 1576 50  0000 C CNN
F 2 "AbstractFoundry:M22-5330605R" H 2550 1250 50  0001 C CNN
F 3 "" H 2550 1250 50  0001 C CNN
F 4 "Changjiang Connectors" H 2550 1250 50  0001 C CNN "Manufacturer"
F 5 "A2005WR-S-6P" H 2550 1250 50  0001 C CNN "Manufacturer_Part_Number"
F 6 "https://www.mouser.co.uk/ProductDetail/Harwin/M22-5330605?qs=sGAEpiMZZMs%252BGHln7q6pmzlZUuX%2F53qjqM6RSwiaBWM%3D" H 2550 1250 50  0001 C CNN "Link"
F 7 "0.25" H 2550 1200 50  0001 C CNN "USD_price_for_10"
F 8 "0.25" H 2550 1200 50  0001 C CNN "USD_price_for_100"
F 9 "M22-5330605" H 2550 1200 50  0001 C CNN "Alternative part"
	1    2550 1200
	1    0    0    -1  
$EndComp
Text Label 2850 950  0    50   ~ 0
VPOWER
Text Label 2850 1050 0    50   ~ 0
5V
Text Label 2850 1150 0    50   ~ 0
GND
Wire Wire Line
	2750 950  4000 950 
Wire Wire Line
	2750 1050 4000 1050
Wire Wire Line
	2750 1150 4000 1150
Wire Wire Line
	2750 1250 4000 1250
$Comp
L AbstractFoundry:CompanyIcon IGNORE1
U 1 1 5D9C91E9
P 750 500
F 0 "IGNORE1" H 878 371 50  0000 L CNN
F 1 "CompanyIcon" H 878 280 50  0000 L CNN
F 2 "AbstractFoundry:CompanyIcon" H 750 500 50  0001 C CNN
F 3 "" H 750 500 50  0001 C CNN
	1    750  500 
	1    0    0    -1  
$EndComp
Text Label 2850 1250 0    50   ~ 0
CAN
$Comp
L AbstractFoundry:STM32F042F6P6 U2
U 1 1 5DF33567
P 2550 2950
F 0 "U2" H 2550 2061 50  0000 C CNN
F 1 "STM32F042F6P6" H 2550 1970 50  0000 C CNN
F 2 "AbstractFoundry:STM32F042F6P6" H 2050 2250 50  0001 R CNN
F 3 "http://www.st.com/st-web-ui/static/active/en/resource/technical/document/datasheet/DM00105814.pdf" H 2550 2950 50  0001 C CNN
	1    2550 2950
	1    0    0    -1  
$EndComp
Text GLabel 2900 1600 2    50   Input ~ 0
SWDIO
Text GLabel 2900 1700 2    50   Input ~ 0
SWCLK
Text GLabel 3200 3450 2    50   Input ~ 0
SWDIO
Text GLabel 3200 3550 2    50   Input ~ 0
SWCLK
Wire Wire Line
	3150 2450 3200 2450
Wire Wire Line
	3200 2550 3150 2550
Wire Wire Line
	3150 2650 3200 2650
Wire Wire Line
	3200 2750 3150 2750
Wire Wire Line
	3150 2850 3200 2850
Wire Wire Line
	3200 2950 3150 2950
Wire Wire Line
	3150 3050 3200 3050
Wire Wire Line
	3200 3150 3150 3150
Wire Wire Line
	3200 3450 3150 3450
Wire Wire Line
	3150 3550 3200 3550
Text GLabel 2250 3800 0    50   Input ~ 0
GND
Wire Wire Line
	2250 3800 2350 3800
Wire Wire Line
	2350 3800 2350 3750
Text GLabel 700  2050 0    50   Input ~ 0
VDD
Text GLabel 1850 3150 0    50   Input ~ 0
I2C_SDA
Text GLabel 1850 3250 0    50   Input ~ 0
I2C_SCL
Wire Wire Line
	1850 3150 1950 3150
Wire Wire Line
	1850 3250 1950 3250
Wire Wire Line
	1950 3450 1850 3450
$Comp
L AbstractFoundry:R R1
U 1 1 5DF4F29C
P 1600 2250
F 0 "R1" H 1670 2296 50  0000 L CNN
F 1 "10K" H 1670 2205 50  0000 L CNN
F 2 "AbstractFoundry:R_0402_1005Metric" V 1530 2250 50  0001 C CNN
F 3 "~" H 1600 2250 50  0001 C CNN
	1    1600 2250
	1    0    0    1   
$EndComp
Wire Wire Line
	1600 2400 1600 2450
Wire Wire Line
	1600 2450 1950 2450
Wire Wire Line
	2450 2050 2450 2250
Wire Wire Line
	2350 2250 2350 2050
Connection ~ 2350 2050
Wire Wire Line
	2350 2050 2450 2050
Wire Wire Line
	1600 2100 1600 2050
Wire Wire Line
	1600 2050 2350 2050
$Comp
L AbstractFoundry:C C3
U 1 1 5DF5DD1D
P 850 2250
F 0 "C3" H 965 2296 50  0000 L CNN
F 1 "4.7uF" H 965 2205 50  0000 L CNN
F 2 "AbstractFoundry:C_0402_1005Metric" H 888 2100 50  0001 C CNN
F 3 "~" H 850 2250 50  0001 C CNN
	1    850  2250
	1    0    0    -1  
$EndComp
Text GLabel 800  2550 0    50   Input ~ 0
GND
Wire Wire Line
	700  2050 850  2050
Wire Wire Line
	800  2550 850  2550
Wire Wire Line
	850  2550 850  2400
Wire Wire Line
	850  2100 850  2050
Connection ~ 850  2050
Wire Wire Line
	850  2050 1250 2050
$Comp
L AbstractFoundry:FemaleConn_1x6 J1
U 1 1 5DD58295
P 4200 1150
F 0 "J1" H 4200 1600 50  0000 L CNN
F 1 "FemaleConn_1x6" H 3950 1500 50  0000 L CNN
F 2 "AbstractFoundry:M22-6540642R" H 4200 1150 50  0001 C CNN
F 3 "" H 4200 1150 50  0001 C CNN
F 4 "Changjiang Connectors" H 4200 1150 50  0001 C CNN "Manufacturer"
F 5 "A2005HWR-S-6P-Y" H 4200 1150 50  0001 C CNN "Manufacturer_Part_Number"
F 6 "https://www.mouser.co.uk/ProductDetail/Harwin/M22-6540642R?qs=%2Fha2pyFadugSTfxSUTVZ2Ehr1GZOewZmeQPAP3RhgAADXaKn5dWIiw%3D%3D" H 4200 1150 50  0001 C CNN "Link"
F 7 "0.3" H 4200 1150 50  0001 C CNN "USD_price_for_10"
F 8 "0.3" H 4200 1150 50  0001 C CNN "USD_price_for_100"
F 9 "M22-6540642" H 4200 1150 50  0001 C CNN "Alternative part"
	1    4200 1150
	1    0    0    -1  
$EndComp
Text GLabel 9550 3550 2    50   Input ~ 0
VDD
Text GLabel 9550 2950 2    50   Input ~ 0
GND
Text GLabel 10350 3250 2    50   Input ~ 0
I2C_SDA
Text GLabel 10350 3150 2    50   Input ~ 0
I2C_SCL
$Comp
L AbstractFoundry:R R2
U 1 1 5E02F132
P 9900 2700
F 0 "R2" H 9970 2746 50  0000 L CNN
F 1 "10K" H 9970 2655 50  0000 L CNN
F 2 "AbstractFoundry:R_0402_1005Metric" V 9830 2700 50  0001 C CNN
F 3 "~" H 9900 2700 50  0001 C CNN
	1    9900 2700
	1    0    0    1   
$EndComp
$Comp
L AbstractFoundry:R R3
U 1 1 5E02F4D0
P 10200 2700
F 0 "R3" H 10270 2746 50  0000 L CNN
F 1 "10K" H 10270 2655 50  0000 L CNN
F 2 "AbstractFoundry:R_0402_1005Metric" V 10130 2700 50  0001 C CNN
F 3 "~" H 10200 2700 50  0001 C CNN
	1    10200 2700
	1    0    0    1   
$EndComp
Text GLabel 7600 2650 0    50   Input ~ 0
VDD
Wire Wire Line
	9400 2950 9550 2950
Wire Wire Line
	9400 3550 9550 3550
Wire Wire Line
	8900 2650 8900 2400
Wire Wire Line
	8900 2400 9900 2400
Wire Wire Line
	9400 3150 9900 3150
Wire Wire Line
	9400 3250 9400 3350
Wire Wire Line
	10350 3250 10200 3250
Wire Wire Line
	9900 2850 9900 3150
Connection ~ 9900 3150
Wire Wire Line
	9900 3150 10350 3150
Wire Wire Line
	10200 2850 10200 3250
Connection ~ 10200 3250
Wire Wire Line
	10200 3250 9400 3250
Wire Wire Line
	10200 2550 10200 2400
Wire Wire Line
	9900 2550 9900 2400
Connection ~ 9900 2400
Wire Wire Line
	9900 2400 10200 2400
Text GLabel 7600 3850 0    50   Input ~ 0
GND
$Comp
L AbstractFoundry:BME280 U3
U 1 1 5E02481F
P 8800 3250
F 0 "U3" H 8371 3296 50  0000 R CNN
F 1 "BME280" H 8371 3205 50  0000 R CNN
F 2 "AbstractFoundry:TempHumidyPressure_BME280" H 10250 2800 50  0001 C CNN
F 3 "" H 8800 3050 50  0001 C CNN
F 4 "Bosch Sensortec " H 8800 3250 50  0001 C CNN "Manufacturer"
F 5 "BME280" H 8800 3250 50  0001 C CNN "Manufacturer_Part_Number"
F 6 "https://www.mouser.co.uk/ProductDetail/Bosch-Sensortec/BME280?qs=sGAEpiMZZMsF1ODjcwEocB0teEbUEBlMe2ty%252BXJvNLw%3D" H 8800 3250 50  0001 C CNN "Link"
F 7 "3.73" H 8800 3250 50  0001 C CNN "USD_price_for_10"
F 8 "3.56" H 8800 3250 50  0001 C CNN "USD_price_for_100"
	1    8800 3250
	1    0    0    -1  
$EndComp
Wire Wire Line
	8700 2650 8900 2650
Connection ~ 8700 2650
Connection ~ 8900 2650
Wire Wire Line
	8700 3850 8900 3850
Connection ~ 8700 3850
$Comp
L AbstractFoundry:BoardText IGNORE2
U 1 1 5E2F487D
P 750 750
F 0 "IGNORE2" H 878 621 50  0000 L CNN
F 1 "Temp, humidy," H 878 530 50  0000 L CNN
F 2 "AbstractFoundry:BoardText" H 800 450 50  0001 C CNN
F 3 "" H 750 750 50  0001 C CNN
	1    750  750 
	1    0    0    -1  
$EndComp
$Comp
L AbstractFoundry:BoardText IGNORE3
U 1 1 5E2F57E4
P 750 950
F 0 "IGNORE3" H 878 821 50  0000 L CNN
F 1 "pressure v0.11" H 878 730 50  0000 L CNN
F 2 "AbstractFoundry:BoardText" H 800 650 50  0001 C CNN
F 3 "" H 750 950 50  0001 C CNN
	1    750  950 
	1    0    0    -1  
$EndComp
Wire Wire Line
	7600 2650 7800 2650
Wire Wire Line
	7600 3850 7800 3850
$Comp
L AbstractFoundry:C C5
U 1 1 5E3DA435
P 7800 3000
F 0 "C5" H 7915 3046 50  0000 L CNN
F 1 "4.7uF" H 7915 2955 50  0000 L CNN
F 2 "AbstractFoundry:C_0402_1005Metric" H 7838 2850 50  0001 C CNN
F 3 "~" H 7800 3000 50  0001 C CNN
	1    7800 3000
	1    0    0    -1  
$EndComp
Connection ~ 7800 2650
Connection ~ 7800 3850
$Comp
L AbstractFoundry:M2.5MountingHole H1
U 1 1 5F1EED8D
P 750 1450
F 0 "H1" H 850 1496 50  0000 L CNN
F 1 "M2.5MountingHole" H 850 1405 50  0000 L CNN
F 2 "AbstractFoundry:MountingHole_2.7mm_M2.5_ISO7380" H 750 1450 50  0001 C CNN
F 3 "~" H 750 1450 50  0001 C CNN
	1    750  1450
	1    0    0    -1  
$EndComp
$Comp
L AbstractFoundry:M2.5MountingHole H2
U 1 1 5F1F3D85
P 750 1650
F 0 "H2" H 850 1696 50  0000 L CNN
F 1 "M2.5MountingHole" H 850 1605 50  0000 L CNN
F 2 "AbstractFoundry:MountingHole_2.7mm_M2.5_ISO7380" H 750 1650 50  0001 C CNN
F 3 "~" H 750 1650 50  0001 C CNN
	1    750  1650
	1    0    0    -1  
$EndComp
Wire Wire Line
	5000 3350 5150 3350
Connection ~ 5000 3350
Wire Wire Line
	5000 3200 5000 3350
Wire Wire Line
	5000 2700 5000 2900
Wire Wire Line
	4900 2700 5000 2700
Text GLabel 3900 3200 2    50   Input ~ 0
VDD
$Comp
L AbstractFoundry:R R4
U 1 1 5DF48DC7
P 5000 3050
F 0 "R4" H 5070 3096 50  0000 L CNN
F 1 "10K" H 5070 3005 50  0000 L CNN
F 2 "AbstractFoundry:R_0402_1005Metric" V 4930 3050 50  0001 C CNN
F 3 "~" H 5000 3050 50  0001 C CNN
	1    5000 3050
	1    0    0    1   
$EndComp
Text GLabel 5150 3350 2    50   Input ~ 0
CAN
$Comp
L AbstractFoundry:R R5
U 1 1 5DF346AE
P 4400 3350
F 0 "R5" V 4300 3300 50  0000 L CNN
F 1 "10R" V 4500 3100 50  0000 L CNN
F 2 "AbstractFoundry:R_0402_1005Metric" V 4330 3350 50  0001 C CNN
F 3 "~" H 4400 3350 50  0001 C CNN
	1    4400 3350
	0    -1   1    0   
$EndComp
Wire Wire Line
	4550 3350 4700 3350
Connection ~ 4700 3350
Wire Wire Line
	4700 3350 5000 3350
Wire Wire Line
	7800 2650 8700 2650
Wire Wire Line
	7800 3850 8700 3850
Wire Wire Line
	7800 2850 7800 2650
Wire Wire Line
	7800 3150 7800 3850
Wire Wire Line
	2750 1350 2800 1350
Wire Wire Line
	2750 1450 4000 1450
Text Label 2850 1350 0    50   ~ 0
HS1
Text Label 2850 1450 0    50   ~ 0
HS2
Wire Wire Line
	2800 1350 2800 1600
Wire Wire Line
	2800 1600 2900 1600
Connection ~ 2800 1350
Wire Wire Line
	2800 1350 4000 1350
Wire Wire Line
	2750 1450 2750 1700
Wire Wire Line
	2750 1700 2900 1700
Connection ~ 2750 1450
Wire Wire Line
	5900 1450 6900 1450
Wire Wire Line
	5550 950  5900 950 
Text GLabel 1500 3950 0    50   Input ~ 0
GND
$Comp
L AbstractFoundry:C C4
U 1 1 6109E818
P 1250 2250
F 0 "C4" H 1365 2296 50  0000 L CNN
F 1 "4.7uF" H 1365 2205 50  0000 L CNN
F 2 "AbstractFoundry:C_0402_1005Metric" H 1288 2100 50  0001 C CNN
F 3 "~" H 1250 2250 50  0001 C CNN
	1    1250 2250
	1    0    0    -1  
$EndComp
Wire Wire Line
	850  2550 1250 2550
Wire Wire Line
	1250 2550 1250 2400
Connection ~ 850  2550
Wire Wire Line
	1250 2100 1250 2050
Connection ~ 1250 2050
Wire Wire Line
	1250 2050 1600 2050
Connection ~ 1600 2050
$Comp
L AbstractFoundry:ME6211C33M5G-N U1
U 1 1 6123A5D3
P 6300 750
F 0 "U1" H 6300 815 50  0000 C CNN
F 1 "ME6211C33M5G-N" H 6300 724 50  0000 C CNN
F 2 "AbstractFoundry:SOT-23-5" H 6300 750 50  0001 C CNN
F 3 "" H 6300 750 50  0001 C CNN
	1    6300 750 
	1    0    0    -1  
$EndComp
Wire Wire Line
	5900 950  5900 1100
Wire Wire Line
	5900 1100 5950 1100
Connection ~ 5900 950 
Wire Wire Line
	5900 950  5950 950 
$Comp
L AbstractFoundry:R R6
U 1 1 615DF0CD
P 1700 3750
F 0 "R6" H 1770 3796 50  0000 L CNN
F 1 "10K" H 1770 3705 50  0000 L CNN
F 2 "AbstractFoundry:R_0402_1005Metric" V 1630 3750 50  0001 C CNN
F 3 "~" H 1700 3750 50  0001 C CNN
	1    1700 3750
	1    0    0    1   
$EndComp
Wire Wire Line
	1700 3550 1700 3600
Wire Wire Line
	1700 3550 1950 3550
Wire Wire Line
	1700 3900 1700 3950
Wire Wire Line
	1700 3950 1500 3950
Text GLabel 3900 3550 2    50   Input ~ 0
GND
Wire Wire Line
	3800 3250 3800 3200
Wire Wire Line
	3800 3200 3900 3200
Wire Wire Line
	3800 3450 3800 3550
Wire Wire Line
	3800 3550 3900 3550
Wire Wire Line
	4050 3350 4250 3350
Wire Wire Line
	3500 3350 3150 3350
Wire Wire Line
	3150 3250 3550 3250
Wire Wire Line
	3550 3250 3550 3000
Wire Wire Line
	3550 3000 4700 3000
Wire Wire Line
	4700 3000 4700 3350
Text GLabel 4900 2700 0    50   Input ~ 0
VDD
$Comp
L AbstractFoundry:74LVC2G07 U4
U 1 1 61719DD0
P 3800 3350
F 0 "U4" H 3775 3617 50  0000 C CNN
F 1 "74LVC2G07" H 3775 3526 50  0000 C CNN
F 2 "AbstractFoundry:SOT-23-6" H 3800 3350 50  0001 C CNN
F 3 "http://www.ti.com/lit/sg/scyt129e/scyt129e.pdf" H 3800 3350 50  0001 C CNN
	1    3800 3350
	1    0    0    -1  
$EndComp
$EndSCHEMATC
