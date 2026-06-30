"""
Copyright (c) 2021-, Haibin Wen, sunnypilot, and a number of other contributors.

This file is part of sunnypilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""
from cereal import car

from opendbc.car import structs
from openpilot.sunnypilot.selfdrive.controls.lib.return_to_center_assist import (
  ReturnToCenterAssist, ENGAGE_ANGLE, RELEASE_ANGLE, TORQUE_THRESHOLD, CREST_MARGIN)


def _subaru_angle_cp():
  CP = structs.CarParams()
  CP.brand = "subaru"
  CP.steerControlType = structs.CarParams.SteerControlType.angle
  return CP


class TestReturnToCenterAssist:

  def setup_method(self):
    self.rtc = ReturnToCenterAssist(_subaru_angle_cp())
    self.rtc.enabled = True
    self.rtc.active = False
    self.CS = car.CarState.new_message()

  def _update(self, angle, torque):
    self.CS.steeringAngleDeg = angle
    self.CS.steeringTorque = torque
    return self.rtc.update(self.CS)

  def test_engages_when_returning_in_turn(self):
    # turn in to a peak first
    self._update(-(ENGAGE_ANGLE + 20), 0)
    # then ease back off the peak with centering torque (positive) above threshold -> engages
    assert self._update(-(ENGAGE_ANGLE + 5), TORQUE_THRESHOLD + 10) is True

  def test_does_not_engage_below_angle(self):
    # small angle (highway lane-keeping): never yields even with centering torque
    assert self._update(-(ENGAGE_ANGLE - 10), TORQUE_THRESHOLD + 50) is False

  def test_does_not_engage_during_turn_in(self):
    # angle still rising (still turning further into the corner) with a momentary opposing-torque
    # sample (grip correction, two-handed shift, sensor transient) must not falsely engage - this is
    # the rising-edge false-engage seen in real drive logs
    self._update(-(ENGAGE_ANGLE + 5), 0)
    assert self._update(-(ENGAGE_ANGLE + 8), TORQUE_THRESHOLD + 50) is False

  def test_does_not_engage_with_into_turn_torque(self):
    # driver torque deepening the turn (same sign as angle) should not yield, even while cresting
    self._update(-(ENGAGE_ANGLE + 20), 0)
    assert self._update(-(ENGAGE_ANGLE + 5), -(TORQUE_THRESHOLD + 50)) is False

  def test_does_not_engage_with_light_torque(self):
    # easing off (torque below threshold) is not a return request, even while cresting
    self._update(-(ENGAGE_ANGLE + 20), 0)
    assert self._update(-(ENGAGE_ANGLE + 5), TORQUE_THRESHOLD - 10) is False

  def test_crest_margin_boundary(self):
    # peak at 100; staying within the margin of the peak does not count as cresting yet
    self._update(-100, 0)
    assert self._update(-(100 - CREST_MARGIN + 1), TORQUE_THRESHOLD + 10) is False
    # past the margin -> cresting, engages
    assert self._update(-(100 - CREST_MARGIN - 1), TORQUE_THRESHOLD + 10) is True

  def test_holds_until_near_center(self):
    # turn in to a peak, then engage, then hold the yield even when torque drops, until the wheel is
    # back near center
    self._update(-100, 0)
    assert self._update(-90, TORQUE_THRESHOLD + 10) is True
    assert self._update(-60, 0) is True                  # still turned, torque released -> still yielding
    assert self._update(-(RELEASE_ANGLE + 5), 0) is True  # just above release -> still yielding
    assert self._update(-(RELEASE_ANGLE - 5), 0) is False  # near center -> resume
    assert not self.rtc.active

  def test_disabled_toggle(self):
    self.rtc.enabled = False
    self._update(-(ENGAGE_ANGLE + 20), 0)
    assert self._update(-(ENGAGE_ANGLE + 5), TORQUE_THRESHOLD + 50) is False

  def test_symmetric_right_turn(self):
    # turned right (positive), driver pushing toward center (negative torque)
    self._update(ENGAGE_ANGLE + 20, 0)
    assert self._update(ENGAGE_ANGLE + 5, -(TORQUE_THRESHOLD + 10)) is True

  def test_unsupported_platform_never_engages(self):
    # torque units are brand-specific, so non-Subaru / non-angle platforms must not yield
    CP = structs.CarParams()
    CP.brand = "toyota"
    CP.steerControlType = structs.CarParams.SteerControlType.torque
    rtc = ReturnToCenterAssist(CP)
    rtc.enabled = True
    self.CS.steeringAngleDeg = -ENGAGE_ANGLE - 10
    self.CS.steeringTorque = TORQUE_THRESHOLD + 50
    assert rtc.update(self.CS) is False
