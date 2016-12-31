
# CAN-Logger
CAN Logging on SD card

Often, engineers face the problem of logging CAN bus communication via Vector CANOE or DSPACE Deskcontrol or similar tools. For a better interpretation of the collected data MatLab my be the tool of choice oe Excel. So it takes some time to export the data from the one tool and import them into the other.
For this usecase a cheap environment has been build. Based on a Arduino Mega platform with enough internal memory of 8k, the device is connected with a MCP2515 CAN card, a SD card reader, a real time clock RTC and in future with a bluetooth adapter for starting and stopping the measurement.
