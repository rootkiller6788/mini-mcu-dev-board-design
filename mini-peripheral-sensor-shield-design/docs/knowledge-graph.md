# Knowledge Graph - mini-peripheral-sensor-shield-design

## L1: Definitions (Complete)
18 sensor type structs: thermistor, RTD, thermocouple, strain gauge, load cell, accelerometer, gyroscope, magnetometer, photodiode, pressure, humidity, ultrasonic, gas, current, Hall, PIR, color, temperature IC
6 enums: principle (18), output type (12), physical quantity (20), unit (31), transfer function type (10), sensor type tag (20)
7 support structs: specs, operating conditions, reading, transfer function, + 4 interface structs

## L2: Core Concepts (Complete)
Signal conditioning: 9 amplifier configs, Wheatstone bridge, 6 filter types, 4-20mA loop, level shifting, ESD protection
Shield interface: 12 form factors, pin mapping, I2C/SPI/UART/1-Wire configs, power distribution
Digital filtering: MA, EMA, median, FIR, IIR biquad
Sensor fusion: complementary, Kalman, Mahony, voting

## L3: Mathematical Structures (Complete)
Transfer functions (10 types), filter coefficients (z-transform), frequency response, vector/quaternion algebra, matrix operations (2x2, 3x3 inverse, Gauss-Jordan)

## L4: Fundamental Laws (Complete)
12 laws: Ohm, Joule, Kirchhoff, Steinhart-Hart, Callendar-Van Dusen, Seebeck, Gauge Factor, Nyquist-Shannon, Shannon-Hartley, Beer-Lambert, Magnus, Allan Variance
C implementations + Lean 4 formalizations

## L5: Algorithms/Methods (Complete)
15 algorithms with full implementations (see README for list)

## L6: Canonical Problems (Complete)
3 end-to-end examples: thermistor shield, IMU shield, gas sensor shield

## L7: Applications (Complete)
IoT air quality (Detroit), drone AHRS, industrial temperature sensing

## L8: Advanced Topics (Partial)
Kalman, Mahony AHRS, sensor voting, quaternion orientation

## L9: Research Frontiers (Partial)
Energy harvesting sensors, smart dust, quantum sensors, bio-compatible sensors