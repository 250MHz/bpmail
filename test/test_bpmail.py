from __future__ import annotations

import os
import subprocess
from typing import Sequence

import pytest

src_eid = 'ipn:1.25'
dest_eid = 'ipn:1.52'
test_dir_str = os.getenv('TEST_DIR', 'test')


def run_bpmailsend(
    *cmdline: Sequence[str], input=None, capture_output: bool = True, check: bool = True
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
    *cmdline: Sequence[str], capture_output: bool = True, check: bool = True
) -> subprocess.CompletedProcess:
    bpmailrecv_path = os.getenv('TEST_BPMAILRECV_BINARY', 'bpmailrecv')
    return subprocess.run(
        [bpmailrecv_path] + list(cmdline), capture_output=capture_output, check=check
    )


@pytest.fixture(scope='session', autouse=True)
def kill_ion():
    subprocess.run('killm', check=True)


class TestWithIon:
    @pytest.fixture(scope='class', autouse=True)
    def ion(self):
        subprocess.run(['ionstart', '-I', f'{test_dir_str}/loopback.rc'], check=True)
        yield
        subprocess.run('killm', check=True)

    def test_basic(self):
        with open(f'{test_dir_str}/message.txt', mode='rb') as message:
            message = message.read()
            run_bpmailsend(src_eid, dest_eid, input=message)
            recv = run_bpmailrecv(dest_eid)
            assert message == recv.stdout

    def test_send_no_content(self):
        send = run_bpmailsend(src_eid, dest_eid, check=False)
        assert send.returncode != 0
        assert b'nothing to send' in send.stderr

    def test_send_unknown_eid(self):
        send = run_bpmailsend('blah', dest_eid, check=False)
        assert send.returncode != 0
        assert b'could not open own endpoint blah' in send.stderr

    def test_recv_unknown_eid(self):
        recv = run_bpmailrecv('blah', check=False)
        assert recv.returncode != 0
        assert b'could not open own endpoint blah' in recv.stderr


def test_send_missing_args():
    send = run_bpmailsend(check=False)
    assert send.returncode != 0
    assert b'usage' in send.stderr


def test_recv_missing_args():
    recv = run_bpmailrecv(check=False)
    assert recv.returncode != 0
    assert b'usage' in recv.stderr


def test_send_ion_not_running():
    send = run_bpmailsend(src_eid, dest_eid, check=False)
    assert send.returncode != 0
    assert b'could not attach to BP' in send.stderr


def test_recv_ion_not_running():
    recv = run_bpmailrecv(dest_eid, check=False)
    assert recv.returncode != 0
    assert b'could not attach to BP' in recv.stderr

