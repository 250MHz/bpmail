from __future__ import annotations

import os
import subprocess
from typing import TYPE_CHECKING

import pytest

from resolver import get_dns_server

if TYPE_CHECKING:
    from typing import BinaryIO

profile_id = '1'
dest_eid = 'ipn:1.129'
dns_addr = os.getenv('DNS_TEST_ADDRESS', '127.0.0.1')
dns_port = int(os.getenv('DNS_TEST_PORT', '5300'))
recv_s_arg = f'-s {dns_addr}:{dns_port}'
test_dir_str = os.getenv('TEST_DIR', 'test')
messages_prefix = f'{test_dir_str}/messages'


def run_bpmailsend(
    *cmdline: str, input=None, capture_output: bool = True, check: bool = True
) -> subprocess.CompletedProcess:
    bpmailsend_path = os.getenv('TEST_BPMAILSEND_BINARY', 'bpmailsend')
    if input is None:
        with open(os.devnull, 'rb') as devnull:
            return subprocess.run(
                [bpmailsend_path] + list(cmdline),
                stdin=devnull,
                capture_output=capture_output,
                check=check,
            )
    return subprocess.run(
        [bpmailsend_path] + list(cmdline),
        input=input,
        capture_output=capture_output,
        check=check,
    )


def run_bpmailrecv(
    *cmdline: str, capture_output: bool = True, check: bool = True
) -> subprocess.CompletedProcess:
    bpmailrecv_path = os.getenv('TEST_BPMAILRECV_BINARY', 'bpmailrecv')
    return subprocess.run(
        [bpmailrecv_path] + list(cmdline), capture_output=capture_output, check=check
    )


def peek_line(f: BinaryIO) -> bytes:
    pos = f.tell()
    line = f.readline()
    f.seek(pos)
    return line


@pytest.fixture(scope='session', autouse=True)
def kill_ion():
    subprocess.run('killm', check=True)


class TestWithIon:
    @pytest.fixture(scope='class', autouse=True)
    def ion(self):
        subprocess.run(['ionstart', '-I', f'{test_dir_str}/loopback.rc'], check=True)
        yield
        subprocess.run('killm', check=True)

    @pytest.fixture(scope='class', autouse=True)
    def dns(self):
        server = get_dns_server(dns_port)
        server.start_thread()
        yield
        server.stop()

    def test_verify_ipn_pass_one_address(self):
        with open(f'{messages_prefix}/node_nbr_1_one_addr.eml', mode='rb') as m:
            ret_path = peek_line(m)
            data = m.read()
            run_bpmailsend(profile_id, dest_eid, input=data)
            recv = run_bpmailrecv(recv_s_arg)
            assert ret_path not in recv.stdout
            assert data.removeprefix(ret_path) == recv.stdout

    def test_verify_ipn_pass_multiple_rr_one_address(self):
        with open(f'{messages_prefix}/node_nbr_1-2-3_one_addr.eml', mode='rb') as m:
            ret_path = peek_line(m)
            data = m.read()
            run_bpmailsend(profile_id, dest_eid, input=data)
            recv = run_bpmailrecv(recv_s_arg)
            assert ret_path not in recv.stdout
            assert data.removeprefix(ret_path) == recv.stdout

    def test_verify_ipn_pass_one_idn_address(self):
        with open(f'{messages_prefix}/node_nbr_1_one_idn_addr.eml', mode='rb') as m:
            ret_path = peek_line(m)
            data = m.read()
            run_bpmailsend(profile_id, dest_eid, input=data)
            recv = run_bpmailrecv(recv_s_arg)
            assert ret_path not in recv.stdout
            assert data.removeprefix(ret_path) == recv.stdout

    def test_verify_ipn_pass_multiple_address(self):
        with open(f'{messages_prefix}/node_nbr_1_mult_addr.eml', mode='rb') as m:
            ret_path = peek_line(m)
            data = m.read()
            run_bpmailsend(profile_id, dest_eid, input=data)
            recv = run_bpmailrecv(recv_s_arg)
            assert ret_path not in recv.stdout
            assert data.removeprefix(ret_path) == recv.stdout

    def test_verify_ipn_fail_one_address(self):
        with open(f'{messages_prefix}/node_nbr_2_one_addr.eml', mode='rb') as m:
            run_bpmailsend(profile_id, dest_eid, input=m.read())
            recv = run_bpmailrecv(recv_s_arg, check=False)
            assert recv.returncode != 0
            assert b'IPN verification failed' in recv.stderr

    def test_verify_ipn_fail_multiple_address(self):
        with open(f'{messages_prefix}/node_nbr_2-3-5_one_addr.eml', mode='rb') as m:
            run_bpmailsend(profile_id, dest_eid, input=m.read())
            recv = run_bpmailrecv(recv_s_arg, check=False)
            assert recv.returncode != 0
            assert b'IPN verification failed' in recv.stderr

    def test_verify_ipn_fail_one_idn_address(self):
        with open(f'{messages_prefix}/node_nbr_2_one_idn_addr.eml', mode='rb') as m:
            run_bpmailsend(profile_id, dest_eid, input=m.read())
            recv = run_bpmailrecv(recv_s_arg, check=False)
            assert recv.returncode != 0
            assert b'IPN verification failed' in recv.stderr

    def test_verify_ipn_fail_one_multiple_address(self):
        with open(f'{messages_prefix}/node_nbr_2_mult_addr.eml', mode='rb') as m:
            run_bpmailsend(profile_id, dest_eid, input=m.read())
            recv = run_bpmailrecv(recv_s_arg, check=False)
            assert recv.returncode != 0
            assert b'IPN verification failed' in recv.stderr

    def test_no_verify_ipn(self):
        with open(f'{messages_prefix}/node_nbr_2_one_addr.eml', mode='rb') as m:
            ret_path = peek_line(m)
            data = m.read()
            run_bpmailsend(profile_id, dest_eid, input=data)
            recv = run_bpmailrecv('--no-verify-ipn')
            assert ret_path not in recv.stdout
            assert data.removeprefix(ret_path) == recv.stdout

    def test_no_verify_ipn_s_arg_ignored(self):
        with open(f'{messages_prefix}/node_nbr_2_one_addr.eml', mode='rb') as m:
            ret_path = peek_line(m)
            data = m.read()
            run_bpmailsend(profile_id, dest_eid, input=data)
            recv = run_bpmailrecv(recv_s_arg, '--no-verify-ipn')
            assert ret_path not in recv.stdout
            assert data.removeprefix(ret_path) == recv.stdout

    def test_invalid_mime(self):
        with open(f'{messages_prefix}/batch_smtp.txt', mode='rb') as m:
            data = m.read()
            run_bpmailsend(profile_id, dest_eid, input=data)
            recv = run_bpmailrecv(check=False)
            assert recv.returncode != 0
            assert b'could not parse MIME message' in recv.stderr

    def test_allow_invalid_mime(self):
        with open(f'{messages_prefix}/batch_smtp.txt', mode='rb') as m:
            data = m.read()
            run_bpmailsend(profile_id, dest_eid, input=data)
            recv = run_bpmailrecv('--allow-invalid-mime')
            assert b'could not parse MIME message' in recv.stderr
            assert data == recv.stdout

    def test_send_no_content(self):
        send = run_bpmailsend(profile_id, dest_eid, check=False)
        assert send.returncode != 0
        assert b'nothing to send' in send.stderr

    def test_send_unknown_profile(self):
        # capture_output=True makes the test flaky. dtpc_send() is strange when
        # it errors. So we won't test the error message
        send = run_bpmailsend(
            '34', dest_eid, input=b'blah\0', capture_output=False, check=False
        )
        assert send.returncode != 0


def test_send_missing_args():
    send = run_bpmailsend(check=False)
    assert send.returncode != 0
    assert b'usage' in send.stderr


def test_send_profile_id_validation():
    send = run_bpmailsend(str(2**65), dest_eid, check=False)
    assert send.returncode != 0
    assert b'strtoul' in send.stderr

    send = run_bpmailsend(str(2**33), dest_eid, check=False)
    assert send.returncode != 0
    assert b'profile_id out of range' in send.stderr

    send = run_bpmailsend('blah', dest_eid, check=False)
    assert send.returncode != 0
    assert b'strtoul' in send.stderr


def test_send_topic_id_validation():
    send = run_bpmailsend(f'-t {2**65}', check=False)
    assert send.returncode != 0
    assert b'strtoul' in send.stderr

    send = run_bpmailsend(f'-t {2**33}', check=False)
    assert send.returncode != 0
    assert b'topic_id out of range' in send.stderr

    send = run_bpmailsend('-t blah', check=False)
    assert send.returncode != 0
    assert b'strtoul' in send.stderr


def test_recv_topic_id_validation():
    recv = run_bpmailrecv(f'-t {2**65}', check=False)
    assert recv.returncode != 0
    assert b'strtoul' in recv.stderr

    recv = run_bpmailrecv(f'-t {2**33}', check=False)
    assert recv.returncode != 0
    assert b'topic_id out of range' in recv.stderr

    recv = run_bpmailrecv('-t blah', check=False)
    assert recv.returncode != 0
    assert b'strtoul' in recv.stderr


def test_recv_extra_args():
    recv = run_bpmailrecv('blah', check=False)
    assert recv.returncode != 0
    assert b'usage' in recv.stderr


def test_send_ion_not_running():
    send = run_bpmailsend(profile_id, dest_eid, check=False)
    assert send.returncode != 0
    assert b'could not attach to DTPC' in send.stderr


def test_recv_ion_not_running():
    recv = run_bpmailrecv(check=False)
    assert recv.returncode != 0
    assert b'could not attach to DTPC' in recv.stderr
