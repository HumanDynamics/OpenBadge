from __future__ import division, absolute_import, print_function
import unittest
import sys
import logging
import time
import serial
import serial.tools.list_ports
import threading

sys.path.append('../BadgeFramework')
from ble_badge_connection import BLEBadgeConnection
from bluepy.btle import BTLEException
from badge import OpenBadge

logging.basicConfig(filename="integration_test.log", level=logging.DEBUG)
logger = logging.getLogger(__name__)

# Enable log output to terminal
stdout_handler = logging.StreamHandler(sys.stdout)
stdout_handler.setLevel(logging.DEBUG)
logger.addHandler(stdout_handler)

# Uncomment this line to make logging very verbose.
# logging.getLogger().addHandler(stdout_handler)

class IntegrationTest(unittest.TestCase):
	def __init__(self, device_addr):
		self.device_addr = device_addr
		unittest.TestCase.__init__(self)

	def restart_badge(self, badge):
		try:
			badge.restart()
		except BTLEException as e:
			# let it restart
			time.sleep(0.25)
			self.connection.connect()
			return OpenBadge(self.connection)

	def assertStatusesEqual(self, status, expected_values):
		"""
		Takes a status response from a badge
		And a dict with expected values
		Note that the keys in <expected_values> must match
		 those in <status> exactly
		"""
		for key, expected_val in expected_values.iteritems():
			actual_val = getattr(status, key)
			self.assertEqual(actual_val, expected_val, msg="""
			Actual and expected status values did not match for
			status: {}\n
			Expected value: {}\n
			Actual value: {}
			""".format(key, expected_val, actual_val))

	def runTest(self):
		self.runTest_startUART()
		# AdaFruit has this really handy helper function, but we should probably write our own, so that
		# we don't have to propogate the AdaFruit dependency everywhere.
		#Adafruit_BluefruitLE.get_provider().run_mainloop_with(self.runTest_MainLoop)
		self.runTest_MainLoop()


	def runTest_startUART(self):
		uartPort = list(serial.tools.list_ports.comports())[0]
		self.uartSerial = serial.Serial(uartPort.device, 115200, timeout=1)


		def uartRXTarget():
			while True:
				# Some slight implicit control flow going on here:
				#   uartSerial.readline() will sometimes timeout, and then we'll just loop around.

				rx_data = self.uartSerial.readline()

				if rx_data:
					# We truncate the ending newline. 
					self.onUartLineRx(rx_data[:-1])

		uartRXThread = threading.Thread(target=uartRXTarget)
		uartRXThread.setDaemon(True)
		uartRXThread.start()

	def onUartLineRx(self, data):
		logger.info("UART:" + data)

	def runTest_MainLoop(self):

		self.connection = BLEBadgeConnection.get_connection_to_badge(self.device_addr)
		self.connection.connect()
		badge = OpenBadge(self.connection)

		badge = self.restart_badge(badge)


		try:
			self.testCase(badge, logger)
			print("Test Passed! :)")
		except Exception as e:
			self.onTestFailure(badge, logger)
			raise AssertionError("Test Failure")

	def onTestFailure(self, badge, logger):
		logger.exception("Exception during test!")
		logger.info("Badge Status after Failure: {}".format(badge.get_status()))
