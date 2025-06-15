#!/bin/bash
# RPM post-uninstall script for cppSwitchboard
# Updates shared library cache after uninstallation

# Update library cache
/sbin/ldconfig

exit 0 