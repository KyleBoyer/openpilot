"""
Copyright (c) 2021-, Haibin Wen, sunnypilot, and a number of other contributors.

This file is part of sunnypilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""
from cereal import car

from openpilot.common.constants import CV
from openpilot.sunnypilot.selfdrive.controls.lib.min_lateral_engage_speed import MinLateralEngageSpeed, SPEED_HYSTERESIS


class TestMinLateralEngageSpeed:

  def setup_method(self):
    self.min_lateral_engage_speed = MinLateralEngageSpeed()
    self._reset_states()

  def _reset_states(self):
    self.min_lateral_engage_speed.is_metric = False
    self.min_lateral_engage_speed.min_speed = 5  # MPH

    self.CS = car.CarState.new_message()
    self.CS.vEgo = 0

  def test_below_min_speed_pauses(self):
    self.CS.vEgo = (5 * CV.MPH_TO_MS) - 0.1
    assert self.min_lateral_engage_speed.update(self.CS) is True

  def test_above_min_speed_does_not_pause(self):
    self.CS.vEgo = (5 * CV.MPH_TO_MS) + 0.1
    assert self.min_lateral_engage_speed.update(self.CS) is False

  def test_disabled_when_zero(self):
    self.min_lateral_engage_speed.min_speed = 0
    self.CS.vEgo = 0.0
    assert self.min_lateral_engage_speed.update(self.CS) is False

  def test_disabled_when_negative(self):
    self.min_lateral_engage_speed.min_speed = -1
    self.CS.vEgo = 0.0
    assert self.min_lateral_engage_speed.update(self.CS) is False

  def test_metric_units(self):
    self.min_lateral_engage_speed.is_metric = True
    self.min_lateral_engage_speed.min_speed = 10  # km/h
    self.CS.vEgo = (10 * CV.KPH_TO_MS) - 0.1
    assert self.min_lateral_engage_speed.update(self.CS) is True
    self.CS.vEgo = (10 * CV.KPH_TO_MS) + SPEED_HYSTERESIS + 0.1
    assert self.min_lateral_engage_speed.update(self.CS) is False

  def test_hysteresis_prevents_toggling(self):
    min_speed_ms = 5 * CV.MPH_TO_MS
    # drop below the threshold -> pause
    self.CS.vEgo = min_speed_ms - 0.1
    assert self.min_lateral_engage_speed.update(self.CS) is True
    # just above the threshold but within the resume margin -> stay paused (no toggling)
    self.CS.vEgo = min_speed_ms + (SPEED_HYSTERESIS / 2)
    assert self.min_lateral_engage_speed.update(self.CS) is True
    # clear the resume margin -> resume
    self.CS.vEgo = min_speed_ms + SPEED_HYSTERESIS + 0.1
    assert self.min_lateral_engage_speed.update(self.CS) is False
