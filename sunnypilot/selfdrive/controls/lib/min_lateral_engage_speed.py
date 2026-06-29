"""
Copyright (c) 2021-, Haibin Wen, sunnypilot, and a number of other contributors.

This file is part of sunnypilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""
from cereal import car

from openpilot.common.constants import CV
from openpilot.common.params import Params


class MinLateralEngageSpeed:
  """Pauses lateral control below a user-set speed so the driver can move the steering wheel freely
  at low speed / standstill (e.g. straighten the wheel at a stop sign) without fighting MADS. MADS
  stays armed and lateral resumes once above the speed. A value of 0 disables the feature (always on).

  The stored MadsMinLateralControlSpeed is in the user's display unit (mph or km/h, per IsMetric),
  matching BlinkerMinLateralControlSpeed.
  """
  def __init__(self):
    self.params = Params()

    self.is_metric = self.params.get_bool("IsMetric")
    self.min_speed = 0

  def get_params(self) -> None:
    self.is_metric = self.params.get_bool("IsMetric")
    self.min_speed = self.params.get("MadsMinLateralControlSpeed")

  def update(self, CS: car.CarState) -> bool:
    if self.min_speed <= 0:
      return False

    speed_factor = CV.KPH_TO_MS if self.is_metric else CV.MPH_TO_MS
    min_speed_ms = self.min_speed * speed_factor

    return bool(CS.vEgo < min_speed_ms)
