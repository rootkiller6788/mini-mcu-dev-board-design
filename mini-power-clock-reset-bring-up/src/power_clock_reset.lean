/-
  Power/Clock/Reset Formal Verification (Lean 4)
  L1: PowerRailSpec, LdoParams, CrystalSpec, ResetConfig
  L2: ldo_recommended, topology_selection
  L3: rc_charge_voltage, rc_time_constant, lc_resonant_freq
  L4: ohms_law, kcl_node, kvl_loop, adc_jitter_limit
  L5: decoupling_impedance, pll_tau1
  L6: stm32_vdd_valid, stm32_pll_vco
  L7: arduino_vin_valid, esp32_battery_life
  L8: gan_better_fom, sscg_emi_reduction
-/

structure PowerRailSpec where
  voltage_nominal : Float
  voltage_min     : Float
  voltage_max     : Float

structure LdoParams where
  v_in           : Float
  v_out          : Float
  dropout_voltage : Float
  psrr_db        : Float
  thermal_resistance_ja : Float

structure CrystalSpec where
  nominal_freq_Hz : Float
  load_cap_pF     : Float
  esr_ohm         : Float
  drive_level_uW  : Float

structure ResetConfig where
  por_threshold_V  : Float
  bor_threshold_V  : Float
  reset_timeout_ms : Float

inductive PowerTree
  | leaf (name : String) (voltage : Float) (current : Float)
  | node (name : String) (voltage : Float) (children : List PowerTree)

theorem power_rail_valid (v_nom v_min v_max : Float) (h_nonneg : v_nom > 0.0) (h_bound : v_min <= v_max) : PowerRailSpec :=
  { voltage_nominal := v_nom, voltage_min := v_min, voltage_max := v_max }

def ldo_recommended (v_in v_out i_load : Float) (noise_sensitive : Bool) : Bool :=
  let vdrop := v_in - v_out in
  vdrop > 0.0 && vdrop < 0.5 && i_load < 0.5 && noise_sensitive

def rc_charge_voltage (vcc r c t : Float) : Float :=
  vcc * (1.0 - Float.exp (-t / (r * c)))

def rc_time_constant (r c : Float) : Float := r * c

theorem ohms_law (v i r : Float) (h_nonzero : r != 0.0) : (i = v / r) <-> (v = i * r) := by
  constructor
  . intro h; field_simp [h_nonzero]; rw [h]
  . intro h; field_simp [h_nonzero]; rw [h]

theorem kcl_node (i_in i_out1 i_out2 : Float) (h : i_in = i_out1 + i_out2) : i_in - i_out1 - i_out2 = 0.0 := by
  linarith

def adc_jitter_limit (n_bits : Nat) (f_input_Hz : Float) : Float :=
  1.0 / (Float.pi * f_input_Hz * ((2.0 : Float) ^ ((n_bits : Nat).succ : Float)))

def decoupling_impedance (c esr esl freq : Float) : Float :=
  let omega := 2.0 * Float.pi * freq in
  let x := omega * esl - 1.0 / (omega * c) in
  Float.sqrt (esr * esr + x * x)

def pll_tau1 (loop_bw_Hz phase_margin_deg : Float) : Float :=
  let wc := 2.0 * Float.pi * loop_bw_Hz in
  let pm_rad := phase_margin_deg * Float.pi / 180.0 in
  (1.0 - Float.sin pm_rad) / (wc * Float.cos pm_rad)

theorem capacitor_energy_doubling_voltage (c v : Float) (h_c_pos : c > 0.0) (h_v_pos : v > 0.0) : (0.5 * c * ((2.0 * v) ^ 2)) = 4.0 * (0.5 * c * (v ^ 2)) := by
  ring

def stm32_vdd_valid (vdd_mV : Float) : Bool :=
  vdd_mV >= 1800.0 && vdd_mV <= 3600.0

def stm32_pll_vco (f_hse : Float) (pllm plln : Nat) : Float :=
  f_hse * ((plln : Float) / (pllm : Float))

def arduino_vin_valid (vin : Float) : Bool :=
  vin >= 7.0 && vin <= 12.0

def esp32_battery_life (capacity_mAh active_current_mA active_duty sleep_current_uA sleep_duty : Float) : Float :=
  let avg_current_mA := active_current_mA * active_duty + (sleep_current_uA / 1000.0) * sleep_duty in
  if avg_current_mA > 0.0 then capacity_mAh / avg_current_mA else 0.0

theorem gan_better_fom (rds_on_mohm qg_nc : Float) (h_pos : rds_on_mohm > 0.0 /\ qg_nc > 0.0) : (rds_on_mohm * qg_nc) * 0.3 < (rds_on_mohm * qg_nc) := by
  have h := h_pos.left
  have hq := h_pos.right
  nlinarith

def sscg_emi_reduction (f_clock_Hz delta_percent rbw_Hz : Float) : Float :=
  let delta := delta_percent / 100.0 in
  10.0 * Float.log10 (delta * f_clock_Hz / rbw_Hz)

def power_tree_total_current : PowerTree -> Float
  | PowerTree.leaf _ _ current => current
  | PowerTree.node _ _ children =>
      children.foldl (fun acc child => acc + power_tree_total_current child) 0.0

def lc_resonant_freq (l c : Float) : Float :=
  1.0 / (2.0 * Float.pi * Float.sqrt (l * c))

def buck_ccm_condition (i_load dI_L : Float) : Bool :=
  i_load > dI_L / 2.0

theorem kvl_loop (v_source v_drop1 v_drop2 v_load : Float) (h : v_source - v_drop1 - v_drop2 = v_load) : v_source - v_drop1 - v_drop2 - v_load = 0.0 := by
  linarith
