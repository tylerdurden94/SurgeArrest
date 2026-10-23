"""One-time APC Home/Ayla bootstrap. Passwords and account tokens are never saved."""
import argparse
import getpass
import json
import requests
from private_config import private_dir, write_private

# Public application identifiers used by the APC Home API, not an owner's LAN key.
APPLICATION = {'app_id': 'schneider-5w-id', 'app_secret': 'schneider-4p5If6sO_QS9F0mQLJmOoCxswng'}
USER_API = 'https://user-field.aylanetworks.com'
DEVICE_API = 'https://ads-field.aylanetworks.com/apiv1'


def capture(email, password, session):
    response = session.post(USER_API + '/users/sign_in.json',
        json={'user': {'email': email, 'password': password, 'application': APPLICATION}}, timeout=25)
    if response.status_code != 200:
        raise ValueError('APC Home sign-in failed. Check the password before another attempt; repeated failures can lock the account.')
    session.headers['Authorization'] = 'auth_token ' + response.json()['access_token']
    response = session.get(DEVICE_API + '/devices.json', timeout=25)
    response.raise_for_status()
    choices = [entry['device'] for entry in response.json() if entry['device'].get('lan_enabled')]
    if not choices:
        raise ValueError('No LAN-enabled strips were returned by APC Home.')
    for index, device in enumerate(choices, 1):
        print(f"{index}. {device.get('product_name', 'APC strip')} (serial ending {device['dsn'][-4:]})")
    index = int(input('Select your SurgeArrest number: ')) - 1 if len(choices) > 1 else 0
    if index < 0 or index >= len(choices):
        raise ValueError('Invalid device selection.')
    device = choices[index]
    response = session.get(DEVICE_API + f"/devices/{device['key']}/lan.json", timeout=25)
    response.raise_for_status()
    lan = response.json()
    if not lan.get('lanip', {}).get('lanip_key'):
        raise ValueError('This device did not return a LAN control key.')
    return {'devices': [{'lan_ip': device['lan_ip'], 'Lanip': lan}]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    from pathlib import Path
    parser.add_argument('--output', type=Path, default=private_dir() / 'config.json')
    args = parser.parse_args()
    if args.output.exists():
        parser.error('A private configuration already exists; use it or choose a different --output file.')
    email = input('APC Home email: ').strip()
    password = getpass.getpass('APC Home password (hidden): ')
    if not email or not password:
        parser.error('Email and password are required.')
    try:
        with requests.Session() as session:
            config = capture(email, password, session)
        del password
        write_private(args.output, json.dumps(config, indent=2) + '\n')
        print(f'LAN configuration saved privately to {args.output}')
    except (requests.RequestException, ValueError, KeyError):
        print('Capture failed. Check APC Home credentials, internet access, and LAN support. No automatic sign-in retries were made.')
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
