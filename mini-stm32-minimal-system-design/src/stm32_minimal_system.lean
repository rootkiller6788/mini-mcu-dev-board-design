/-
  Formalization: STM32 Minimal System Design

  Knowledge Levels: L1 (Definitions), L4 (Fundamental Laws)
  Reference: STM32F103 RM0008, ST AN4488
  Course mapping: Berkeley EE16A/B, MIT 6.003
-/

namespace STM32MinimalSystem

inductive VoltageDomain : Type where
  | vdd | vdda | vbat | vref | vdd_usb
  deriving BEq, Repr

inductive ClockSource : Type where
  | hsi | hse | lsi | lse | pll
  deriving BEq, Repr

inductive BootMode : Type where
  | main_flash | system_mem | sram
  deriving BEq, Repr

inductive ResetSource : Type where
  | por | bor | external | watchdog | software | low_power
  deriving BEq, Repr

structure PowerSpec where
  domain : VoltageDomain
  nominal_voltage : Float
  min_voltage : Float
  max_voltage : Float
  max_current : Float
  ripple_tolerance : Float
  requires_filtering : Bool
  deriving Repr

structure CrystalSpec where
  nominal_freq : Float
  freq_tolerance_ppm : Float
  load_capacitance : Float
  shunt_capacitance : Float
  esr_max : Float
  drive_level_max : Float
  deriving Repr

structure PLLConfig where
  enabled : Bool
  input_source : ClockSource
  input_divider : Nat
  multiplier : Nat
  sys_divider : Nat
  usb_divider : Nat
  output_freq : Float
  deriving Repr

structure ResetConfig where
  nrst_pullup_resistance : Float
  nrst_capacitance : Float
  reset_time_required : Float
  has_external_supervisor : Bool
  deriving Repr

structure BoardConfig where
  vdd_pin_count : Nat
  vss_pin_count : Nat
  vdda_pin_count : Nat
  core_max_freq : Float
  flash_size_kb : Nat
  sram_size_kb : Nat
  vdd_spec : PowerSpec
  hse_crystal : CrystalSpec
  pll : PLLConfig
  reset : ResetConfig
  deriving Repr

def rc_voltage (vdd : Float) (v_init : Float) (r : Float) (c : Float) (t : Float) : Float :=
  let tau := r * c
  vdd * (1.0 - Float.exp (-t / tau)) + v_init * Float.exp (-t / tau)

def rc_time_to_threshold (vdd : Float) (vth : Float) (r : Float) (c : Float) : Float :=
  let tau := r * c
  -tau * Float.log (1.0 - vth / vdd)

def junction_temperature (ambient_temp : Float) (power_w : Float) (theta_ja : Float) : Float :=
  ambient_temp + power_w * theta_ja

def ipc2221_current (k : Float) (delta_t : Float) (area_mil2 : Float) : Float :=
  k * (delta_t ^ 0.44) * (area_mil2 ^ 0.725)

def self_resonant_freq (capacitance : Float) (esl : Float) : Float :=
  1.0 / (2.0 * Float.pi * Float.sqrt (esl * capacitance))

def gm_critical (esr : Float) (freq : Float) (c0 : Float) (cl : Float) : Float :=
  let omega := 2.0 * Float.pi * freq
  let c_total := c0 + cl
  4.0 * esr * omega * omega * c_total * c_total

def valid_pin_config (cfg : BoardConfig) : Bool :=
  cfg.vdd_pin_count >= cfg.vdda_pin_count &&
  cfg.vdd_pin_count + cfg.vss_pin_count + cfg.vdda_pin_count >= 3

def vdd_in_range (nominal : Float) : Bool :=
  nominal >= 2.0 && nominal <= 3.6

def boot_mode_from_pins (boot0 : Bool) (boot1 : Bool) : BootMode :=
  if !boot0 then BootMode.main_flash
  else if !boot1 then BootMode.system_mem
  else BootMode.sram

theorem boot_mode_deterministic (b0 b1 : Bool) :
  (boot_mode_from_pins b0 b1 = BootMode.main_flash)
  \/ (boot_mode_from_pins b0 b1 = BootMode.system_mem)
  \/ (boot_mode_from_pins b0 b1 = BootMode.sram) := by
  unfold boot_mode_from_pins
  split <;> simp

theorem zero_power_at_zero_voltage (current : Float) : 0.0 * current = 0.0 := by
  ring

end STM32MinimalSystem
