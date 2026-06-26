# Knowledge Graph - Mini Low-Power Coin Cell Design

## L1: Definitions (13 items)

| # | Definition | C Type | Lean Type |
|---|-----------|--------|-----------|
| 1 | Coin cell chemistry types | BatteryChemistry enum | BatteryChem inductive |
| 2 | Coin cell model designations | CoinCellModel enum | CoinCellModel inductive |
| 3 | Battery nominal parameters | CoinCellParams struct | BatteryParams structure |
| 4 | Battery runtime state | BatteryState struct | BatteryState structure |
| 5 | MCU power operating modes | McuPowerMode enum | - |
| 6 | Clock domains for gating | ClockDomain enum | - |
| 7 | Wake-up sources | WakeupSource enum | - |
| 8 | Power consumer categories | PowerConsumer enum | - |
| 9 | Regulator topologies | RegulatorType enum | - |
| 10 | Energy harvesting source types | HarvestingSource enum | - |
| 11 | Battery aging mechanisms | AgingMechanism enum | - |
| 12 | Energy buffer types | BufferType enum | - |
| 13 | MPPT algorithm types | MPPTAlgorithm enum | - |

## L2: Core Concepts (12 items)

| # | Concept | Implementation |
|---|---------|---------------|
| 1 | Peukert discharge model | PeukertModel struct + functions |
| 2 | Shepherd discharge model | ShepherdModel struct + functions |
| 3 | Discharge curve LUT | DischargeLUT + interpolation |
| 4 | MCU power profiling | McuPowerProfile + 4 MCU families |
| 5 | Duty cycling analysis | DutyCycle + DutyCyclePhase |
| 6 | Power budget analysis | PowerBudget + consumer entries |
| 7 | Battery capacity derating | CapacityDerating 5-factor model |
| 8 | Load profile analysis | LoadProfile + LoadSegment |
| 9 | Voltage regulation efficiency | LDO/Switching params + models |
| 10 | Energy buffer management | EnergyBuffer + sizing |
| 11 | Energy neutrality | EnergyNeutralityState |
| 12 | Adaptive duty cycling | AdaptiveDutyController (PI) |

## L3: Mathematical Structures (10 items)

| # | Structure | Implementation |
|---|-----------|---------------|
| 1 | Linear interpolation | Binary search + lerp |
| 2 | Battery statistics | BatteryStats (mean, stddev, min/max) |
| 3 | Power state machine | FSM with transition tables |
| 4 | Energy integral | Trapezoidal integration |
| 5 | Efficiency map | Piecewise linear model |
| 6 | Pareto frontier | Duty cycle sweep |
| 7 | Probability distributions | Weibull, Box-Muller Gaussian |
| 8 | Rainflow counting | 4-point algorithm (ASTM E1049) |
| 9 | Lanczos Gamma function | Mathematical special function |
| 10 | Newton-Raphson solver | MLE for Weibull parameters |

## L4: Fundamental Laws (8 items)

| # | Law | C Function | Lean Theorem |
|---|-----|-----------|-------------|
| 1 | Peukert Law | peukert_capacity() | peukert_ideal_battery |
| 2 | Ohm Law (IR drop) | terminal_voltage_under_load() | terminal_voltage_lte_ocv |
| 3 | Arrhenius Equation | arrhenius_self_discharge_rate() | arrhenius_af_ge_one |
| 4 | Conservation of Energy | battery_compute_stats() | energy_from_capacity |
| 5 | Friis Transmission | rf_harvested_power() | - |
| 6 | Maximum Power Transfer | teg_power_output() | - |
| 7 | Miner Rule (fatigue) | rainflow_damage() | - |
| 8 | Seebeck Effect | teg_power_output() | - |

## L5: Algorithms (18 items)

Shepherd simulation, Coulomb counting, EKF SoC, LUT interpolation, Clock gating, RTC scheduling, DVFS selection, Power budget optimization, Pareto frontier, BOD supervision, LDO efficiency, Switching efficiency, Monte Carlo life, Weibull MLE, Rainflow counting, RUL prediction, MPPT P&O, Adaptive PI control.

## L6: Canonical Problems (4 items)

1. CR2032 temperature logger (example_cr2032_logger.c)
2. BLE beacon coin cell design (example_ble_beacon.c)
3. Wireless sensor node optimization (example_sensor_node.c)
4. CR2032 logger formal verification (coin_cell_formal.lean)

## L7: Applications (3 items)

1. IoT temperature logger (1.7+ years on CR2032)
2. BLE beacon (>5 years on CR2450)
3. LoRaWAN sensor node with indoor solar harvesting

## L8: Advanced Topics (5 items)

1. Extended Kalman Filter for SoC estimation
2. Monte Carlo statistical battery life
3. Weibull reliability analysis (MLE, B10)
4. Energy harvesting + coin cell hybrid
5. Adaptive duty cycling (PI energy control)

## L9: Research Frontiers (3 items)

1. Solid-state thin-film batteries (documented)
2. Battery-less energy harvesting (framework implemented)
3. AI-based battery life prediction (linear regression baseline)
