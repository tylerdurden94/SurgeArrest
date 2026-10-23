"""Exercise USB setup and cloud bootstrap without touching hardware or accounts."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import configure
import capture_key
from private_config import write_private


def config():
    return {'devices': [{'lan_ip': '192.0.2.10', 'Lanip': {'lanip': {
        'lanip_key': 'k' * 32, 'lanip_key_id': 1}}}]}


class SetupTests(unittest.TestCase):
    def test_private_file_refuses_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'config.json'
            write_private(path, '{}')
            with self.assertRaises(FileExistsError):
                write_private(path, 'replacement')
            self.assertEqual(path.read_text(), '{}')

    def test_provision_format_and_ip_override(self):
        command = configure.validate(config(), 't' * 32, '192.0.2.11')
        self.assertEqual(command, 'APC_CONFIG|192.0.2.11|1|' + 'k' * 32 + '|' + 't' * 32 + '\n')

    def test_rejects_invalid_private_configuration(self):
        for key in ('short', 'x' * 31 + '|', 'x' * 31 + '\n'):
            value = config()
            value['devices'][0]['Lanip']['lanip']['lanip_key'] = key
            with self.assertRaises(ValueError):
                configure.validate(value, 't' * 32)
        with self.assertRaises(ValueError):
            configure.validate(config(), 'too-short')
        with self.assertRaises(ValueError):
            configure.validate(config(), 't' * 32, 'not-an-ip')

    def test_serial_ack_and_no_secret_output(self):
        class Connection:
            def write(self, value): self.sent = value
            def flush(self): pass
            def readline(self): return b'APC_CONFIG_OK\n'
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            connection = Connection()
            configure.exchange(connection, 'secret\n', 'APC_CONFIG_OK')
        self.assertEqual(connection.sent, b'secret\n')
        self.assertEqual(output.getvalue(), '')

    def test_serial_rejection_aborts(self):
        class Connection:
            def write(self, value): pass
            def flush(self): pass
            def readline(self): return b'CONFIG_ERROR\n'
        with self.assertRaises(ValueError):
            configure.exchange(Connection(), 'secret\n', 'APC_CONFIG_OK')

    def test_cloud_login_failure_has_no_automatic_retries(self):
        from unittest.mock import Mock
        session = Mock()
        session.post.return_value.status_code = 401
        with self.assertRaises(ValueError):
            capture_key.capture('example@example.invalid', 'example-only', session)
        self.assertEqual(session.post.call_count, 1)
        session.get.assert_not_called()

    def test_cloud_output_contains_only_local_configuration(self):
        from unittest.mock import Mock
        session = Mock()
        session.headers = {}
        session.post.return_value.status_code = 200
        session.post.return_value.json.return_value = {'access_token': 'test-account-token'}
        listing, lan = Mock(), Mock()
        listing.json.return_value = [{'device': {'lan_enabled': True, 'product_name': 'Example',
            'dsn': 'EXAMPLE0000', 'key': 42, 'lan_ip': '192.0.2.10'}}]
        lan.json.return_value = config()['devices'][0]['Lanip']
        session.get.side_effect = [listing, lan]
        with contextlib.redirect_stdout(io.StringIO()):
            result = capture_key.capture('example@example.invalid', 'example-only', session)
        self.assertEqual(result, config())
        self.assertNotIn('test-account-token', json.dumps(result))
        self.assertNotIn('example-only', json.dumps(result))


if __name__ == '__main__':
    unittest.main()
