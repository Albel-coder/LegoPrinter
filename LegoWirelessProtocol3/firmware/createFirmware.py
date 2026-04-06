import asyncio
from pybricksdev.firmware import create_firmware_blob

async def main():
    blob, metadata, license_text = await create_firmware_blob("pybricks-technichub-v3.6.1.zip");
    with open("firmware.blob", "wb") as f:
        f.write(blob)
    with open("firmware.metadata.json.out", "w", encoding="utf-8") as f:
        f.write(str(metadata))
    with open("ReadMe_OSS.out.txt", "w", encoding="utf-8") as f:
        f.write(license_text)

asyncio.run(main())