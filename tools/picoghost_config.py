#!/usr/bin/env python3
import argparse
import datetime as dt
import fcntl
import os
from pathlib import Path
import struct
import sys

REPORT_ID_CONFIG = 3
FEATURE_PAYLOAD_SIZE = 63
FEATURE_REPORT_SIZE = FEATURE_PAYLOAD_SIZE + 1

CMD_SET_TIME = 1
CMD_SET_SCHEDULE = 2
CMD_SET_TIME_AND_SCHEDULE = 3
STATUS_RESPONSE = 0x80

MAGIC = b"PGHC"
VERSION = 1
VID = "0000046D"
PID = "0000C31C"


def _ioc(direction, type_, nr, size):
    return (direction << 30) | (size << 16) | (ord(type_) << 8) | nr


HIDIOCSFEATURE = _ioc(0x3, "H", 0x06, FEATURE_REPORT_SIZE)
HIDIOCGFEATURE = _ioc(0x3, "H", 0x07, FEATURE_REPORT_SIZE)


def parse_hhmm(value):
    try:
        hour_text, minute_text = value.split(":", 1)
        hour = int(hour_text)
        minute = int(minute_text)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("expected HH:MM") from exc

    if hour < 0 or hour > 23 or minute < 0 or minute > 59:
        raise argparse.ArgumentTypeError("expected HH:MM in 00:00..23:59")
    return hour * 60 + minute


def fmt_hhmm(minutes):
    return f"{minutes // 60:02d}:{minutes % 60:02d}"


def find_hidraw():
    matches = []
    for hidraw in sorted(Path("/sys/class/hidraw").glob("hidraw*")):
        device = hidraw / "device" / "uevent"
        try:
            fields = dict(
                line.split("=", 1)
                for line in device.read_text().splitlines()
                if "=" in line
            )
        except OSError:
            continue

        hid_id = fields.get("HID_ID", "").upper()
        hid_name = fields.get("HID_NAME", "")
        if VID in hid_id and PID in hid_id and "USB Keyboard/Mouse Combo" in hid_name:
            matches.append(Path("/dev") / hidraw.name)

    if not matches:
        raise SystemExit("Could not find PicoGhostHID hidraw device.")
    if len(matches) > 1:
        joined = ", ".join(str(match) for match in matches)
        raise SystemExit(f"Multiple matching hidraw devices found: {joined}")
    return matches[0]


def open_device(path):
    try:
        return os.open(path, os.O_RDWR)
    except PermissionError as exc:
        raise SystemExit(f"Permission denied opening {path}. Try sudo or a udev rule.") from exc


def feature_report(command):
    report = bytearray(FEATURE_REPORT_SIZE)
    report[0] = REPORT_ID_CONFIG
    report[1:5] = MAGIC
    report[5] = VERSION
    report[6] = command
    return report


def send_report(device, report):
    fd = open_device(device)
    try:
        fcntl.ioctl(fd, HIDIOCSFEATURE, report, True)
    finally:
        os.close(fd)


def read_status(device):
    report = bytearray(FEATURE_REPORT_SIZE)
    report[0] = REPORT_ID_CONFIG
    fd = open_device(device)
    try:
        fcntl.ioctl(fd, HIDIOCGFEATURE, report, True)
    finally:
        os.close(fd)

    payload = report[1:]
    if payload[0:4] != MAGIC or payload[4] != VERSION or payload[5] != STATUS_RESPONSE:
        raise SystemExit("Device returned an unexpected status report.")

    flags = payload[6]
    current = None if payload[7] == 0xFF else f"{payload[7]:02d}:{payload[8]:02d}:{payload[9]:02d}"
    active_start, active_end, lunch_start, lunch_duration = struct.unpack_from("<HHHH", payload, 10)
    return {
        "time_valid": bool(flags & 0x01),
        "schedule_enabled": bool(flags & 0x02),
        "activity_allowed": bool(flags & 0x04),
        "paused": bool(flags & 0x08),
        "current_time": current,
        "active_start": active_start,
        "active_end": active_end,
        "lunch_start": lunch_start,
        "lunch_duration": lunch_duration,
    }


def fill_time(report, offset, now):
    struct.pack_into(
        "<HBBBBB",
        report,
        offset,
        now.year,
        now.month,
        now.day,
        now.hour,
        now.minute,
        now.second,
    )


def fill_schedule(report, offset, args):
    report[offset] = 1 if args.enable else 0
    struct.pack_into(
        "<HHHH",
        report,
        offset + 1,
        args.start,
        args.end,
        args.lunch,
        args.lunch_duration,
    )


def print_status(status):
    print(f"time_valid={status['time_valid']}")
    print(f"schedule_enabled={status['schedule_enabled']}")
    print(f"activity_allowed={status['activity_allowed']}")
    print(f"paused={status['paused']}")
    print(f"current_time={status['current_time'] or 'unset'}")
    print(f"active={fmt_hhmm(status['active_start'])}-{fmt_hhmm(status['active_end'])}")
    print(
        f"lunch={fmt_hhmm(status['lunch_start'])} "
        f"duration={status['lunch_duration']} minutes"
    )


def main():
    parser = argparse.ArgumentParser(description="Configure PicoGhostHID over HID feature reports.")
    parser.add_argument("--device", type=Path, help="hidraw device, defaults to auto-detect")

    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("sync-time", help="sync the device clock to local host time")

    schedule = subparsers.add_parser("set-schedule", help="set or disable the active window")
    schedule.add_argument("--enable", action=argparse.BooleanOptionalAction, default=True)
    schedule.add_argument("--start", type=parse_hhmm, default=parse_hhmm("09:00"))
    schedule.add_argument("--end", type=parse_hhmm, default=parse_hhmm("17:00"))
    schedule.add_argument("--lunch", type=parse_hhmm, default=parse_hhmm("12:00"))
    schedule.add_argument("--lunch-duration", type=int, default=60)

    configure = subparsers.add_parser("configure", help="sync time and set schedule in one report")
    configure.add_argument("--enable", action=argparse.BooleanOptionalAction, default=True)
    configure.add_argument("--start", type=parse_hhmm, default=parse_hhmm("09:00"))
    configure.add_argument("--end", type=parse_hhmm, default=parse_hhmm("17:00"))
    configure.add_argument("--lunch", type=parse_hhmm, default=parse_hhmm("12:00"))
    configure.add_argument("--lunch-duration", type=int, default=60)

    subparsers.add_parser("status", help="read current device config/status")

    args = parser.parse_args()
    device = args.device or find_hidraw()

    if args.command == "sync-time":
        report = feature_report(CMD_SET_TIME)
        fill_time(report, 7, dt.datetime.now())
        send_report(device, report)
    elif args.command == "set-schedule":
        if args.lunch_duration < 0 or args.lunch_duration > 24 * 60:
            raise SystemExit("--lunch-duration must be 0..1440 minutes")
        report = feature_report(CMD_SET_SCHEDULE)
        fill_schedule(report, 7, args)
        send_report(device, report)
    elif args.command == "configure":
        if args.lunch_duration < 0 or args.lunch_duration > 24 * 60:
            raise SystemExit("--lunch-duration must be 0..1440 minutes")
        report = feature_report(CMD_SET_TIME_AND_SCHEDULE)
        fill_time(report, 7, dt.datetime.now())
        fill_schedule(report, 14, args)
        send_report(device, report)
    elif args.command == "status":
        print_status(read_status(device))
        return

    print_status(read_status(device))


if __name__ == "__main__":
    main()
