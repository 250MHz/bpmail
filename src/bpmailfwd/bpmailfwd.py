import asyncio
import logging
import subprocess

from aiosmtpd.controller import Controller
from aiosmtpd.lmtp import LMTP

logger = logging.getLogger(__name__)


class LMTPController(Controller):
    def factory(self):
        self.smtpd = LMTP(self.handler)
        return self.smtpd


class ExampleHandler:
    async def handle_DATA(self, server, session, envelope):
        print(f"mf = {envelope.mail_from}")
        print(f"rt = {envelope.rcpt_tos}")
        print("Message:\n")
        for ln in envelope.content.decode("utf8", errors="replace").splitlines():
            print(f"> {ln}".strip())
        print()
        print("End of message")
        try:
            subprocess.run(
                ["dtnc", "-N", "ipn:1.2424"],
                input=envelope.original_content,
                capture_output=True,
                check=True,
            )
        except subprocess.CalledProcessError as e:
            logger.info(f"{e.cmd} returned {e.returncode}, {e.stderr}")
            # TODO: selectively send 4yz or 5yz, save that for an actual
            # daemon, not this PoC though
            return "554 5.3.0 Could not gateway message"

        return "250 Message accepted for delivery"


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    controller = LMTPController(ExampleHandler())
    controller.start()
    print(f"hostname: {controller.hostname}")
    print(f"port: {controller.port}")
    input("lol")
    controller.stop()
