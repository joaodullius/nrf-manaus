#!/usr/bin/env python3
import asyncio
import argparse
from bleak import BleakScanner, BleakClient
from bleak.exc import BleakError

# UUIDs from the filter list service
ADD_DEVICE_CHAR_UUID    = "22f64492-bb02-4ccf-9612-c749be0c897d"
REMOVE_DEVICE_CHAR_UUID = "aebd5484-453a-4bb5-a8ea-1593405a4a36"
CLEAR_LIST_CHAR_UUID    = "67e19398-8879-40e8-b513-b75f0268278c"
SCAN_CONTROL_CHAR_UUID  = "4f3b5a2c-8d1e-4b6c-9f7d-5a2c8d1e4b6c"

# MAC address of the concentrator
CONCENTRATOR_MAC = "E2:E4:4F:24:B0:90"

# Default address type (0x01 means BT_ADDR_LE_RANDOM)
DEFAULT_ADDR_TYPE = 0x01

def convert_mac_to_little_endian(mac):
    """
    Convert a MAC string in standard format (AA:BB:CC:DD:EE:FF)
    to 6 bytes in little-endian order (i.e. reversed order of byte pairs).
    """
    mac_nocolon = mac.replace(":", "")
    pairs = [mac_nocolon[i:i+2] for i in range(0, len(mac_nocolon), 2)]
    reversed_pairs = list(reversed(pairs))
    return bytes.fromhex(''.join(reversed_pairs))

def parse_mac_line(line):
    """
    Parse a line from file, returning 7 bytes:
    6 bytes for the MAC in little-endian order and 1 byte for the type.
    """
    parts = line.strip().split()
    if not parts:
        return None
    mac = parts[0].upper()
    mac_bytes = convert_mac_to_little_endian(mac)
    addr_type = int(parts[1], 16) if len(parts) > 1 else DEFAULT_ADDR_TYPE
    return mac_bytes + bytes([addr_type])

def parse_mac(mac_str, type_str):
    """
    Parse a single MAC provided via command line,
    returning 7 bytes: 6-byte MAC (little-endian) and 1-byte type.
    """
    mac = mac_str.upper()
    mac_bytes = convert_mac_to_little_endian(mac)
    try:
        addr_type = int(type_str, 16)
    except Exception:
        addr_type = DEFAULT_ADDR_TYPE
    return mac_bytes + bytes([addr_type])

async def main(args):
    if args.clear:
        # For clear command, no MAC data is sent.
        packet = b''
        char_uuid = CLEAR_LIST_CHAR_UUID
        mac_entries = None
    else:
        # For add or rem commands, either --mac_list or --mac must be provided (but not both)
        if bool(args.mac_list) == bool(args.mac):
            print("❌ For add or rem, specify either --mac_list or --mac, not both.")
            return

        if args.mac_list:
            mac_entries = []
            try:
                with open(args.mac_list, "r") as f:
                    for line in f:
                        if line.strip() == "":
                            continue
                        entry = parse_mac_line(line)
                        if entry:
                            mac_entries.append(entry)
            except Exception as e:
                print(f"❌ Error reading file: {e}")
                return

            if not mac_entries:
                print("❌ No valid MAC found in the file.")
                return
        else:
            mac_entries = [parse_mac(args.mac, args.type)]
        char_uuid = ADD_DEVICE_CHAR_UUID if args.add else REMOVE_DEVICE_CHAR_UUID

    print("🔍 Scanning for concentrator...")
    devices = await BleakScanner.discover(timeout=5.0)
    target = next((d for d in devices if d.address.upper() == CONCENTRATOR_MAC.upper()), None)
    if not target:
        print(f"❌ Concentrator {CONCENTRATOR_MAC} not found.")
        return
    print(f"✅ Found: {target.name} [{target.address}]")

    try:
        async with BleakClient(target.address) as client:
            if not client.is_connected:
                print("❌ Connection failed.")
                return
            print("🔗 Connected. Sending scan control command to stop scanning...")
            # Stop scanning before modifying the filter list
            await client.write_gatt_char(SCAN_CONTROL_CHAR_UUID, b'\x00', response=True)

            if args.clear:
                await client.write_gatt_char(char_uuid, packet, response=True)
                print("✅ Clear command sent successfully.")
            else:
                print("🔗 Sending individual MAC commands...")
                for i, mac_entry in enumerate(mac_entries):
                    try:
                        await client.write_gatt_char(char_uuid, mac_entry, response=True)
                        # Convert back to big-endian for display purposes.
                        mac_big_endian = ":".join(f"{b:02X}" for b in mac_entry[:6][::-1])
                        addr_type = mac_entry[6]
                        print(f"✅ {i+1:02d}) Sent: {mac_big_endian} (type=0x{addr_type:02X})")
                    except Exception as e:
                        print(f"❌ {i+1:02d}) Failed to send: {e}")

            print("🔗 Re-enabling scanning via scan control command...")
            # Restart scanning after modifications are complete
            await client.write_gatt_char(SCAN_CONTROL_CHAR_UUID, b'\x01', response=True)
            print("✅ Operation completed successfully.")

    except BleakError as e:
        print(f"❌ BLE Error: {e}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Manage filter accept list via BLE with scan control")
    cmd_group = parser.add_mutually_exclusive_group(required=True)
    cmd_group.add_argument("--add", action="store_true", help="Command to add MACs")
    cmd_group.add_argument("--rem", action="store_true", help="Command to remove MACs")
    cmd_group.add_argument("--clear", action="store_true", help="Command to clear the list")

    mac_group = parser.add_mutually_exclusive_group()
    mac_group.add_argument("--mac_list", help="File with list of MACs (one per line, with optional type)")
    mac_group.add_argument("--mac", help="Single MAC in format AA:BB:CC:DD:EE:FF")

    parser.add_argument("--type", default="0x01", help="MAC type in hexadecimal (default 0x01) for --mac")
    
    args = parser.parse_args()
    if (args.add or args.rem) and not (args.mac_list or args.mac):
        parser.error("For add or rem commands, you must provide --mac_list or --mac.")
    asyncio.run(main(args))
