import asyncio
from bleak import BleakScanner, BleakClient
from struct import unpack

# UUIDs
WORKER_SHADOW_SERVICE_UUID = "5facdc62-df9e-4403-a258-6f590aea3440"
WORKER_SHADOW_CHAR_UUID = "485ec8f4-c56c-4534-9ba3-d850bf804877"

# Struct format: 24 bytes com padding
# 4 + 2 + 2 + 4 + 4 + 1 + 1 + 1 + 1(pad) + 2 + 2(pad) = 24
STRUCT_FORMAT = "<IhHiiBBBxhxx"

def parse_shadow(data: bytes):
    print(f"\n📦 Raw data ({len(data)} bytes): {data.hex(' ')}")

    if len(data) != 24:
        print(f"⚠️  Unexpected data length: {len(data)} bytes")
        return

    (
        timestamp,
        temperature,
        pressure,
        latitude,
        longitude,
        fix_type,
        movement,
        posture,
        light
    ) = unpack("<IhHiiBBBxhxx", data)

    print(f"\n🛰️  Worker Shadow Update")
    print(f"  ⏱️  Timestamp: {timestamp} ms (0x{timestamp:08X})")
    print(f"  🌡️  Temp: {temperature / 100.0:.2f} °C (0x{temperature & 0xFFFF:04X})")
    print(f"  🌬️  Pressure: {pressure / 10.0:.1f} hPa (0x{pressure:04X})")
    print(f"  🧭 Latitude: {latitude / 1e7:.7f} (0x{latitude & 0xFFFFFFFF:08X})")
    print(f"  🧭 Longitude: {longitude / 1e7:.7f} (0x{longitude & 0xFFFFFFFF:08X})")
    print(f"  📡 Fix Type: {fix_type} (0x{fix_type:02X})")
    print(f"  🚶 Movement: {'MOVING' if movement else 'STILL'} (0x{movement:02X})")
    print(f"  🧍 Posture: {'STANDING' if posture else 'NON-STANDING'} (0x{posture:02X})")
    print(f"  💡 Light: {light} lx (0x{light & 0xFFFF:04X})")

async def main():
    print("🔍 Scanning for the concentrator device...")

    device = await BleakScanner.find_device_by_filter(
        lambda d, adv: WORKER_SHADOW_SERVICE_UUID.lower() in [s.lower() for s in adv.service_uuids]
    )

    if not device:
        print("❌ Could not find the concentrator.")
        return

    print(f"✅ Found device: {device.name or '(no name)'} ({device.address})")

    async with BleakClient(device) as client:
        print("🔗 Connected. Subscribing to shadow notifications...")

        def handle_notification(_, data):
            parse_shadow(data)

        await client.start_notify(WORKER_SHADOW_CHAR_UUID, handle_notification)

        print("📡 Subscribed! Waiting for updates (Ctrl+C to exit)...")
        try:
            while True:
                await asyncio.sleep(1)
        except KeyboardInterrupt:
            print("👋 Disconnecting...")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except Exception as e:
        print(f"❌ Error: {e}")
