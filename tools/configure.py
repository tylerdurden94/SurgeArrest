"""Provision a dedicated SurgeArrest ESP32 over USB without putting secrets in source."""
import argparse
import getpass
import ipaddress
import json
import re
import secrets
import time
from pathlib import Path
import serial
from serial.tools import list_ports
from private_config import private_dir, write_private


def validate(config, token, override=None):
    devices = config['devices']
    if len(devices) != 1:
        raise ValueError('The private configuration must contain exactly one strip.')
    device = devices[0]
    ip = str(ipaddress.IPv4Address(override or device['lan_ip']))
    lan = device['Lanip']['lanip']
    key, key_id = str(lan['lanip_key']), int(lan['lanip_key_id'])
    if len(key) != 32 or any(ord(c) < 32 or ord(c) > 126 or c == '|' for c in key):
        raise ValueError('Unsupported LAN key format.')
    if not 0 < key_id <= 0xffffffff or not re.fullmatch(r'[A-Za-z0-9_-]{24,64}', token):
        raise ValueError('Invalid key ID or bridge token.')
    return f'APC_CONFIG|{ip}|{key_id}|{key}|{token}\n'


def exchange(connection, command, expected, timeout=10):
    connection.write(command.encode('utf-8'))
    connection.flush()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = connection.readline().decode('utf-8', errors='replace').strip()
        if line == expected:
            return
        if line in ('CONFIG_ERROR', 'APC_CONFIG_ERROR'):
            raise ValueError('The bridge rejected the configuration.')
    raise TimeoutError('No setup acknowledgement. Check the USB port, firmware, and cable.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--list', action='store_true', help='List USB serial ports without configuring anything')
    parser.add_argument('--port', help='Your ESP32 USB port, such as COM6 or /dev/ttyUSB0')
    parser.add_argument('--config', type=Path, default=private_dir() / 'config.json')
    parser.add_argument('--token-file', type=Path, default=private_dir() / 'bridge_token.txt')
    parser.add_argument('--device-ip', help='Current strip IPv4 address if the captured address changed')
    args = parser.parse_args()
    if args.list:
        for port in list_ports.comports():
            print(f'{port.device}: {port.description}')
        return 0
    if not args.port:
        parser.error('Specify --port after listing the connected USB ports.')
    try:
        token = args.token_file.read_text().strip() if args.token_file.exists() else secrets.token_urlsafe(32)
        command = validate(json.loads(args.config.read_text()), token, args.device_ip)
        ssid = input('Home 2.4 GHz Wi-Fi network name: ')
        password = getpass.getpass('Wi-Fi password (hidden; blank for open network): ')
        if not 1 <= len(ssid.encode('utf-8')) <= 32 or len(password.encode('utf-8')) > 63:
            raise ValueError('Wi-Fi name or password is too long or the name is empty.')
        if '\x00' in ssid or '\x00' in password:
            raise ValueError('Wi-Fi values cannot contain NUL.')
        network = 'NETWORK_CONFIG|' + json.dumps({'ssid': ssid, 'password': password}, ensure_ascii=False) + '\n'
        if len(network.encode('utf-8')) > 512:
            raise ValueError('Encoded network configuration is too long.')
        if not args.token_file.exists():
            write_private(args.token_file, token + '\n')
        with serial.Serial(args.port, 115200, timeout=0.5, write_timeout=5) as connection:
            time.sleep(2)
            connection.reset_input_buffer()
            exchange(connection, 'HELLO\n', 'SURGEARREST_READY')
            exchange(connection, network, 'NETWORK_CONFIG_OK')
            exchange(connection, command, 'APC_CONFIG_OK')
            exchange(connection, 'RESTART\n', 'RESTARTING')
        print('Configuration saved. The ESP32 is restarting and connecting to your Wi-Fi.')
        print(f'Open this private file to copy the token into SmartThings Settings: {args.token_file}')
    except (OSError, KeyError, ValueError, TimeoutError, serial.SerialException) as error:
        print(f'Setup did not complete: {type(error).__name__}. Check the config file, port, and guide; re-run setup to retry.')
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
