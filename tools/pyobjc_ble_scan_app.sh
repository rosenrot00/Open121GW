#!/bin/sh
cd /Users/taarimi7/Downloads/ida_pro/eevblog121gw_fw || exit 1
exec .venv-ble/bin/python tools/pyobjc_ble_scan.py 15 >/tmp/eev_pyobjc_ble_scan.log 2>&1
