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


class ReturnToCenterAssist:
  """Lets the driver steer out of a turn naturally without fighting MADS. When the wheel is turned
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

  def get_params(self) -> None:
    self.enabled = self.params.get_bool("MadsReturnToCenterAssist")

  def update(self, CS: car.CarState) -> bool:
    if not (self.enabled and self.supported):
      self.active = False
      return False

    angle = CS.steeringAngleDeg
    if self.active:
      # hold the yield until the wheel is back near center
      if abs(angle) < RELEASE_ANGLE:
        self.active = False
    else:
      # engage when meaningfully turned and the driver is steering toward center (opposing the angle)
      driver_returning = (CS.steeringTorque * angle < 0) and (abs(CS.steeringTorque) > TORQUE_THRESHOLD)
      if abs(angle) > ENGAGE_ANGLE and driver_returning:
        self.active = True

    return self.active
