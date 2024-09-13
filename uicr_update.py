from colorama import Fore, Style
import subprocess
import re
from os import system
import argparse

def log_inf(msg):
    print(Fore.GREEN + msg + Style.RESET_ALL)

def log_err(msg):
    print(Fore.RED + msg + Style.RESET_ALL)

def ret_chk(ret, msg):
    if (ret != 0):
        log_err(msg)
        exit()

def get_connected_devices():
    stdout = subprocess.check_output('nrfjprog --ids',
                                     shell=True).decode('utf-8')
    snrs = re.findall(r'([\d]+)', stdout)
    return list(map(int, snrs))

def reg_set(snr, reg, value):
    log_inf("writing HW version to UICR")
    cmd = f'nrfjprog --memwr {hex(reg)} --val {hex(value)} --snr {snr}'
    ret = system(cmd)
    if ret != 0:
        log_err("failed to write HW version to UICR")

    log_inf("Restarting device")
    cmd = f'nrfjprog --hardreset --snr {snr}'
    ret = system(cmd)
    ret_chk(ret, "failed to restart device")

HARDWARE_VERSIONS = {
    "WIMKY001": 0x80000001,
    "rev2": 0xFFFFFFFF,
}

UICR_REG = 0x10001000
UICR_CUSTOMER_OFFSET = 0x080
UICR_CUSTOMER = UICR_REG + UICR_CUSTOMER_OFFSET

HW_SELECT = "WIMKY001"

devices = get_connected_devices()
assert len(devices) == 1, "Only one device should be connected"

reg_set(devices[0], UICR_CUSTOMER, HARDWARE_VERSIONS[HW_SELECT])