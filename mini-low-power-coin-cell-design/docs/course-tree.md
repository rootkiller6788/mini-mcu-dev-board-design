# Course Tree - Mini Low-Power Coin Cell Design

## Prerequisites

- Basic Circuit Theory (Ohm Law, KCL/KVL, RC circuits)
  - Analog Electronics (LDO design, switching converters, BOD circuits)
  - Digital Electronics (MCU architectures, clock gating, low-power design)
  - Signal Processing (Kalman filtering, digital filtering)
  - Probability and Statistics (Gaussian, Weibull, MLE, linear regression)

## Dependencies Within This Module

- coin_cell_battery.h (base definitions and battery models)
  - low_power_mcu.h (MCU power states, duty cycling)
    - power_budget.h (power budget, life estimation)
      - battery_life.h (advanced life, aging, reliability)
  - voltage_regulation.h (LDO, switching, BOD)
  - energy_harvesting.h (solar, TEG, RF harvesting)

## Postrequisites

- Wireless sensor network design
- Battery management systems (BMS)
- Energy-autonomous IoT systems
- Medical implant power design
