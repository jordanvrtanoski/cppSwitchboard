#!/bin/bash
# RPM post-install script for cppSwitchboard
# Updates shared library cache after installation

# Update library cache
/sbin/ldconfig

exit 0 