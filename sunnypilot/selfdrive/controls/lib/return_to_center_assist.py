"""
Copyright (c) 2021-, Haibin Wen, sunnypilot, and a number of other contributors.

This file is part of sunnypilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""
from cereal import car

from opendbc.car import structs
from openpilot.common.params import Params

# only relevant in a meaningful turn (not highway lane-keeping, where steering angle stays small)
ENGAGE_ANGLE = 45.0     # deg
# once engaged, hold until the wheel is back near center so we don't re-grab and steer back into the turn
RELEASE_ANGLE = 20.0    # deg
# driver torque (toward center) that counts as a deliberate request to bring the wheel back. This is in
# Subaru Steer_Torque_Sensor units (normal active lane-keeping stays under ~80) and is brand-specific,
# so the assist is scoped to angle-LKAS Subaru below.
TORQUE_THRESHOLD = 40
# the wheel must have come down at least this far off its peak before a centering torque is trusted as
# an actual return-to-center request. Without this, a momentary opposing-torque sample during turn-in
# (grip correction, two-handed shift, sensor transient) can cross TORQUE_THRESHOLD right as the angle
# crosses ENGAGE_ANGLE while the driver is still actively turning further in - logs from real drives
# showed this firing while the angle was still rising 30-84 deg/s, well before the driver eased out.
CREST_MARGIN = 5.0      # deg


class ReturnToCenterAssist:
  """Allows the driver to steer out of a turn naturally without fighting MADS. When the wheel is turned
  and the driver applies steering torque toward center, lateral yields (MADS stays armed) so the
  driver and steering geometry bring the wheel back; lateral resumes once it is near center.

  This addresses openpilot holding a turn the model still "wants": the camera's FOV is narrower than
  the driver's view, so on exit the desired angle lags and openpilot keeps commanding into the turn.

  The torque threshold is in Subaru units, so this is limited to angle-LKAS Subaru.
  """
  def __init__(self, CP: structs.CarParams):
    self.params = Params()

    # driver torque units vary by brand; only enable where the threshold has been tuned
    self.supported = CP.brand == "subaru" and CP.steerControlType == structs.CarParams.SteerControlType.angle
    self.enabled = self.params.get_bool("MadsReturnToCenterAssist")
    self.active = False
    self.peak_angle = 0.0  # largest |angle| seen since the wheel last entered the engage zone

  def get_params(self) -> None:
    self.enabled = self.params.get_bool("MadsReturnToCenterAssist")

  def update(self, CS: car.CarState) -> bool:
    if not (self.enabled and self.supported):
      self.active = False
      self.peak_angle = 0.0
      return False

    angle = CS.steeringAngleDeg
    if self.active:
      # hold the yield until the wheel is back near center
      if abs(angle) < RELEASE_ANGLE:
        self.active = False
    elif abs(angle) <= ENGAGE_ANGLE:
      self.peak_angle = 0.0
    else:
      self.peak_angle = max(self.peak_angle, abs(angle))
      # engage when the driver is steering toward center (opposing the angle) AND the wheel has
      # actually crested off its peak - filters out a momentary opposing-torque sample while still
      # turning further in (see CREST_MARGIN above)
      driver_returning = (CS.steeringTorque * angle < 0) and (abs(CS.steeringTorque) > TORQUE_THRESHOLD)
      cresting = abs(angle) < self.peak_angle - CREST_MARGIN
      if driver_returning and cresting:
        self.active = True

    return self.active
