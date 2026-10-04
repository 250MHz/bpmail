import asyncio

from aiosmtpd.controller import Controller
from aiosmtpd.lmtp import LMTP


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
        return "250 Message accepted for delivery"


if __name__ == "__main__":
    controller = LMTPController(ExampleHandler())
    controller.start()
    print(f"hostname: {controller.hostname}")
    print(f"port: {controller.port}")
    input("lol")
    controller.stop()
