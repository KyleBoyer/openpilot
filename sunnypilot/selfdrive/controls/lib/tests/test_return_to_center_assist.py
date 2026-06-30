"""
Copyright (c) 2021-, Haibin Wen, sunnypilot, and a number of other contributors.

This file is part of sunnypilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""
from cereal import car

from openpilot.sunnypilot.selfdrive.controls.lib.return_to_center_assist import (
  ReturnToCenterAssist, ENGAGE_ANGLE, RELEASE_ANGLE, TORQUE_THRESHOLD)


class TestReturnToCenterAssist:

  def setup_method(self):
    self.rtc = ReturnToCenterAssist()
    self.rtc.enabled = True
    self.rtc.active = False
    self.CS = car.CarState.new_message()

  def _update(self, angle, torque):
    self.CS.steeringAngleDeg = angle
    self.CS.steeringTorque = torque
    return self.rtc.update(self.CS)

  def test_engages_when_returning_in_turn(self):
    # turned left (negative), driver pushing toward center (positive torque) above threshold
    assert self._update(-ENGAGE_ANGLE - 10, TORQUE_THRESHOLD + 10) is True

  def test_does_not_engage_below_angle(self):
    # small angle (highway lane-keeping): never yields even with centering torque
    assert self._update(-(ENGAGE_ANGLE - 10), TORQUE_THRESHOLD + 50) is False

  def test_does_not_engage_with_into_turn_torque(self):
    # driver torque deepening the turn (same sign as angle) should not yield
    assert self._update(-ENGAGE_ANGLE - 10, -(TORQUE_THRESHOLD + 50)) is False

  def test_does_not_engage_with_light_torque(self):
    # easing off (torque below threshold) is not a return request
    assert self._update(-ENGAGE_ANGLE - 10, TORQUE_THRESHOLD - 10) is False

  def test_holds_until_near_center(self):
    # engage, then hold the yield even when torque drops, until the wheel is back near center
    assert self._update(-100, TORQUE_THRESHOLD + 10) is True
    assert self._update(-60, 0) is True                  # still turned, torque released -> still yielding
    assert self._update(-(RELEASE_ANGLE + 5), 0) is True  # just above release -> still yielding
    assert self._update(-(RELEASE_ANGLE - 5), 0) is False  # near center -> resume
    assert not self.rtc.active

  def test_disabled_toggle(self):
    self.rtc.enabled = False
    assert self._update(-ENGAGE_ANGLE - 10, TORQUE_THRESHOLD + 50) is False

  def test_symmetric_right_turn(self):
    # turned right (positive), driver pushing toward center (negative torque)
    assert self._update(ENGAGE_ANGLE + 10, -(TORQUE_THRESHOLD + 10)) is True
