# Course Dependency Tree - mini-peripheral-sensor-shield-design

## Prerequisites
- Analog Electronics (R, C, L, op-amps)
- Circuit Analysis (KVL, KCL, Thevenin/Norton)
- Signals and Systems (frequency response, sampling)
- MCU Fundamentals (GPIO, ADC, I2C/SPI/UART)

## Module Tree
```
sensor_types (L1, L3, L4)
-> signal_conditioning (L2-L5)
  -> shield_interface (L2, L4, L6)
    -> sensor_calibration (L3, L5)
      -> sensor_filter (L3, L5)
        -> sensor_fusion (L5, L8)
```

## Downstream Modules
- mini-mcu-embedded-sys
- mini-iot-edge-computing
- mini-control-automation
- mini-navigation-positioning