# mini-peripheral-sensor-shield-design

## Module Status: COMPLETE

- **L1 Definitions**: Complete - 18 sensor type structs, 6 enums, 7 support structs
- **L2 Core Concepts**: Complete - Signal conditioning, shield interfaces, bus protocols
- **L3 Math Structures**: Complete - Transfer functions, filter coefficients, vector/quaternion
- **L4 Fundamental Laws**: Complete - 12 laws (Steinhart-Hart, CVD, Seebeck, Wheatstone bridge, Gauge Factor, Ohm, Joule, Nyquist, Magnus, Beer-Lambert, Allan Variance, Shannon-Hartley)
- **L5 Algorithms/Methods**: Complete - 15 algorithms (calibration, filtering, fusion, matrix ops)
- **L6 Canonical Problems**: Complete - 3 end-to-end examples
- **L7 Applications**: Complete - IoT air quality, drone AHRS, industrial temperature
- **L8 Advanced Topics**: Partial+ - Kalman, Mahony AHRS, sensor voting, quaternion orientation
- **L9 Research Frontiers**: Partial - Energy harvesting, smart dust, quantum sensors

## Code Metrics
- include/ + src/ total lines: 4019 (C + H)
- Lean 4 formalization: 158 lines
- Headers: 6, Sources: 7 (6 .c + 1 .lean)
- Tests: 25 assertions, all passing
- Examples: 3 end-to-end (>30 lines, with main + printf)

## Core Theorems (L4)
1. Ohm's Law (1827): V = I*R
2. Joule's Law (1841): P = I^2*R
3. Kirchhoff's Laws (1845): KCL + KVL
4. Steinhart-Hart Equation: 1/T = A + B*ln(R) + C*(ln(R))^3
5. Callendar-Van Dusen Equation: R(T) = R0*(1+A*T+B*T^2+...)
6. Seebeck Effect (1821): V = S*(T_hot - T_cold)
7. Gauge Factor: GF = dR/R / epsilon
8. Nyquist-Shannon Theorem (1949): fs >= 2*f_max
9. Shannon-Hartley Theorem (1948): C = B*log2(1+SNR)
10. Beer-Lambert Law: I = I0*exp(-alpha*c*L)
11. Magnus Formula: Dew point from T and RH
12. Allan Variance: MEMS noise characterization

## Key Formulas (L3-L4)
- Wheatstone bridge: Vout = Vex*(R1/(R1+R2) - R3/(R3+R4))
- Voltage divider: Vout = Vin*R2/(R1+R2)
- Instrumentation amp: G = (1+2R1/Rg)*(R3/R2)
- Sallen-Key LP: fc = 1/(2*pi*sqrt(R1*R2*C1*C2))
- EMA filter: y[n] = alpha*x[n] + (1-alpha)*y[n-1]
- Complementary filter: angle = alpha*gyro + (1-alpha)*accel
- Kalman: K = P_pred/(P_pred+R), x = x + K*(z-x)
- Magnus dew point: T_dp = (b*gamma)/(a-gamma)
- Speed of sound: c = 331.3 + 0.606*T m/s
- ADC LSB: V_LSB = Vref/2^N
- I2C pull-up: Rp_min = (Vdd-0.4)/0.003

## Core Algorithms (L5)
1. Steinhart-Hart 3-point calibration (Gaussian elimination)
2. Linear least-squares regression with R^2
3. Polynomial calibration up to 3rd order
4. Newton-Raphson iteration
5. Moving average O(1) ring buffer
6. EMA filter (configurable tau/fc)
7. Median filter (spike rejection)
8. FIR filter (windowed sinc, Hamming)
9. IIR biquad (Direct Form I, Butterworth)
10. Complementary filter (gyro+accel fusion)
11. 1D Kalman filter (predict+update)
12. Mahony AHRS (PI on SO(3))
13. Sensor voting (MAD outlier rejection)
14. Gauss-Jordan elimination
15. Bilinear transform (analog-to-digital)

## Canonical Problems (L6)
1. Thermistor Temperature Shield: NTC + divider + ADC + EMA
2. IMU Sensor Shield: MPU-6050 + complementary + Kalman
3. Gas Sensor Shield: MQ-135 + CCS811 IoT air quality

## Applications (L7)
- IoT Air Quality Monitoring (Detroit air quality)
- Drone IMU Attitude Estimation (UAV stabilization)
- Industrial Temperature Control (4-20mA transmitter)

## Advanced Topics (L8)
- Kalman filtering with adaptive noise
- Mahony nonlinear observer on SO(3)
- Sensor redundancy with confidence scoring
- Quaternion orientation (gimbal lock free)

## Nine-School Course Mapping
| School | Courses | Topics |
|--------|---------|--------|
| MIT | 6.002, 6.003, 6.007 | Dividers, filters, EMI |
| Stanford | EE101A, EE102A, EE264 | Amplifiers, DSP, filtering |
| Berkeley | EE105, EE140, EE123 | Sensor physics, op-amps, DSP |
| Illinois | ECE 310, ECE 445 | DSP, PCB design |
| Michigan | EECS 311, EECS 455 | Analog, IoT |
| Georgia Tech | ECE 3042, ECE 4270 | Microelectronics, DSP |
| TU Munich | Sensor Systems, Embedded | Calibration, MCU |
| ETH Zurich | 227-0455, 227-0427 | EMI/EMC, signals |
| Tsinghua | Sensor Tech, Embedded Design | Shield engineering |

## Build
```
make        # Build and test (25 tests)
make test   # Run tests only
make examples  # Build and run 3 examples
make clean  # Clean artifacts
```

## Reference Texts
- Fraden: "Handbook of Modern Sensors" (5th ed., 2016)
- Wilson: "Sensor Technology Handbook" (2005)
- Sedra & Smith: "Microelectronic Circuits" (2020)
- Horowitz & Hill: "The Art of Electronics" (3rd ed., 2015)
- Oppenheim & Schafer: "Discrete-Time Signal Processing" (2010)
- Press et al.: "Numerical Recipes in C" (3rd ed., 2007)
- Kalman: "A New Approach to Linear Filtering" (1960)
- Mahony et al.: "Nonlinear Complementary Filters on SO(3)" (2008)